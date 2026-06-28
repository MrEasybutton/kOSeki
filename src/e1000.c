#include "e1000.h"
#include "pci.h"
#include "pmm.h"
#include "kheap.h"
#include "string.h"
#include "serial.h"
#include "console.h"
#include "utils.h"
#include "isr.h"
#include "net.h"
#include "8259_pic.h"

//driver internal state
static uint32 mmio_base = 0;
static uint8 mac[6];

#define E1000_NUM_RX_DESC 128
#define E1000_NUM_TX_DESC 128

#define ICR_TXDW (1 << 0)
#define ICR_TXQE (1 << 1)

#define TSTA_DD (1 << 0)

static struct e1000_rx_desc *rx_descs;
static struct e1000_tx_desc *tx_descs;
static uint32 rx_cur = 0;
static uint32 tx_cur = 0;

static void* tx_buffers[E1000_NUM_TX_DESC];

static inline void e1000_write(uint32 reg, uint32 value) {
    *(volatile uint32*)(mmio_base + reg) = value;
}

static inline uint32 e1000_read(uint32 reg) {
    return *(volatile uint32*)(mmio_base + reg);
}

static void e1000_handler(REGISTERS *r) {
    (void)r;
    uint32 status = e1000_read(E1000_REG_ICR);
    
    if (status & (1 << 2)) { // LSC
        uint32 s = e1000_read(E1000_REG_STATUS);
        kprint("E1000: Link Changed: %s\n", (s & (1 << 1)) ? "UP" : "DOWN");
    }

    // process if rx set
    if (status & ( (1 << 7) | (1 << 6) | (1 << 4) | (1 << 0) )) {
        while (rx_descs[rx_cur].status & 0x01) {
            uint8 *buffer = (uint8*)((uint32)rx_descs[rx_cur].addr);
            uint16 len = rx_descs[rx_cur].length;

            net_receive(buffer, len);

            rx_descs[rx_cur].status = 0;
            uint32 old_cur = rx_cur;
            rx_cur = (rx_cur + 1) % E1000_NUM_RX_DESC;
            e1000_write(E1000_REG_RDT, old_cur);
        }
    }

    // cleanup
    if (status & (ICR_TXDW | ICR_TXQE)) {
        for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
            if (tx_buffers[i] && (tx_descs[i].status & TSTA_DD)) {
                kfree(tx_buffers[i]);
                tx_buffers[i] = NULL;
                tx_descs[i].status = 0; // clear DD
            }
        }
    }
}

static void e1000_detect_mac() {
    uint32 low = e1000_read(E1000_REG_RAL);
    uint32 high = e1000_read(E1000_REG_RAH);

    if (high & 0x80000000) {
        mac[0] = low & 0xFF;
        mac[1] = (low >> 8) & 0xFF;
        mac[2] = (low >> 16) & 0xFF;
        mac[3] = (low >> 24) & 0xFF;
        mac[4] = high & 0xFF;
        mac[5] = (high >> 8) & 0xFF;
    }

    kprint("E1000: MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void e1000_init_rx() {
    // alloc 128 byte signed desc
    uint32 rx_size = E1000_NUM_RX_DESC * sizeof(struct e1000_rx_desc);
    void* rx_ptr = kmalloc(rx_size + 128);
    uint32 rx_phys = (uint32)rx_ptr;
    if (rx_phys % 128 != 0) rx_phys += 128 - (rx_phys % 128);
    rx_descs = (struct e1000_rx_desc*)rx_phys;
    memset(rx_descs, 0, rx_size);

    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        void* buffer = kmalloc(2048 + 16);
        uint32 addr = (uint32)buffer;
        if (addr % 16 != 0) addr += 16 - (addr % 16);
        
        rx_descs[i].addr = (uint64)addr;
        rx_descs[i].status = 0;
    }

    e1000_write(E1000_REG_RDBAL, (uint32)rx_descs);
    e1000_write(E1000_REG_RDBAH, 0);
    e1000_write(E1000_REG_RDLEN, rx_size);
    e1000_write(E1000_REG_RDH, 0);
    e1000_write(E1000_REG_RDT, E1000_NUM_RX_DESC - 1);
    e1000_write(E1000_REG_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_BSIZE_2048);
}

void e1000_init_tx() {
    uint32 tx_size = E1000_NUM_TX_DESC * sizeof(struct e1000_tx_desc);
    void* tx_ptr = kmalloc(tx_size + 128);
    uint32 tx_phys = (uint32)tx_ptr;
    if (tx_phys % 128 != 0) tx_phys += 128 - (tx_phys % 128);
    tx_descs = (struct e1000_tx_desc*)tx_phys;
    memset(tx_descs, 0, tx_size);
    memset(tx_buffers, 0, sizeof(tx_buffers));

    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        tx_descs[i].addr = 0;
        tx_descs[i].cmd = 0;
        tx_descs[i].status = 0;
    }

    e1000_write(E1000_REG_TDBAL, (uint32)tx_descs);
    e1000_write(E1000_REG_TDBAH, 0);
    e1000_write(E1000_REG_TDLEN, tx_size);
    e1000_write(E1000_REG_TDH, 0);
    e1000_write(E1000_REG_TDT, 0);
    e1000_write(E1000_REG_TIPG, 0x0060200A);
    e1000_write(E1000_REG_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP);
}

void e1000_init(uint8 bus, uint8 slot, uint8 func) {
    kprint("E1000: Initializing at %02x:%02x.%d\n", bus, slot, func);

    pci_enable_bus_mastering(bus, slot, func);
    pci_bar_t bar = pci_get_bar(bus, slot, func, 0);
    
    if (bar.type != PCI_BAR_TYPE_MEM) {
        kprint("ERROR: BAR0 != MMIO\n");
        return;
    }

    mmio_base = bar.base_address;
    kprint("E1000: MMIO Base: 0x%x, Size: %d bytes\n", mmio_base, bar.size);

    e1000_write(E1000_REG_CTRL, E1000_CTRL_RST);
    while (e1000_read(E1000_REG_CTRL) & E1000_CTRL_RST);
    
    uint32 ctrl = e1000_read(E1000_REG_CTRL);
    e1000_write(E1000_REG_CTRL, ctrl | E1000_CTRL_SLU | E1000_CTRL_ASDE);

    for (int i = 0; i < 128; i++) {
        e1000_write(E1000_REG_MTA + (i * 4), 0);
    }

    e1000_detect_mac();
    printf("E1000 NIC: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    e1000_init_rx();
    e1000_init_tx();
    
    net_init(mac);

    uint8 irq = (uint8)(pci_config_read_word(bus, slot, func, 0x3C) & 0xFF);
    kprint("E1000: Using IRQ %d\n", irq);
    register_interrupt_handler(IRQ_BASE + irq, e1000_handler);
    pic8259_unmask(irq);

    e1000_write(E1000_REG_IMS, 0x1FFFF);
    e1000_read(E1000_REG_ICR);
    
    uint32 status = e1000_read(E1000_REG_STATUS);
    kprint("E1000: status: 0x%x (Link %s)\n", status, (status & (1 << 1)) ? "UP" : "DOWN");
}

void e1000_send_packet(void* data, uint16 length) {
    // wait for free time
    while (tx_buffers[tx_cur] != NULL && !(tx_descs[tx_cur].status & 0x01)) {}

    // cleanup if not cleared by isr
    if (tx_buffers[tx_cur] && (tx_descs[tx_cur].status & 0x01)) {
        kfree(tx_buffers[tx_cur]);
        tx_buffers[tx_cur] = NULL;
    }

    struct e1000_tx_desc *desc = &tx_descs[tx_cur];
    tx_buffers[tx_cur] = data;
    
    desc->addr = (uint32)data;
    desc->length = length;
    desc->cmd = (1 << 0) | (1 << 1) | (1 << 3); // EOP IFCS RS
    desc->status = 0;

    tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;
    
    e1000_write(E1000_REG_TDT, tx_cur);
}