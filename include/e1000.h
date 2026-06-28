#ifndef E1000_H
#define E1000_H

#include "types.h"

// register offsets
#define E1000_REG_CTRL      0x0000  // Device Control
#define E1000_REG_STATUS    0x0008  // Device Status
#define E1000_REG_EEPROM    0x0014  // EEPROM Read
#define E1000_REG_CTRL_EXT  0x0018  // Extended Device Control
#define E1000_REG_ICR       0x00C0  // Interrupt Cause Read
#define E1000_REG_ITR       0x00C4  // Interrupt Throttling Rate
#define E1000_REG_IMS       0x00D0  // Interrupt Mask Set
#define E1000_REG_IMC       0x00D8  // Interrupt Mask Clear
#define E1000_REG_RCTL      0x0100  // Receive Control
#define E1000_REG_TCTL      0x0400  // Transmit Control
#define E1000_REG_TIPG      0x0410  // Transmit Inter-Packet Gap
#define E1000_REG_RDBAL     0x2800  // RX Descriptor Base Address Low
#define E1000_REG_RDBAH     0x2804  // RX Descriptor Base Address High
#define E1000_REG_RDLEN     0x2808  // RX Descriptor Length
#define E1000_REG_RDH       0x2810  // RX Descriptor Head
#define E1000_REG_RDT       0x2818  // RX Descriptor Tail
#define E1000_REG_TDBAL     0x3800  // TX Descriptor Base Address Low
#define E1000_REG_TDBAH     0x3804  // TX Descriptor Base Address High
#define E1000_REG_TDLEN     0x3808  // TX Descriptor Length
#define E1000_REG_TDH       0x3810  // TX Descriptor Head
#define E1000_REG_TDT       0x3818  // TX Descriptor Tail
#define E1000_REG_MTA       0x5200  // Multicast Table Array
#define E1000_REG_RAL       0x5400  // Receive Address Low
#define E1000_REG_RAH       0x5404  // Receive Address High

// register bits
#define E1000_CTRL_RST      (1 << 26) // Software Reset
#define E1000_CTRL_SLU      (1 << 6)  // Set Link Up
#define E1000_CTRL_ASDE     (1 << 5)  // Auto-Speed Detection Enable

#define E1000_RCTL_EN       (1 << 1)  // Receiver Enable
#define E1000_RCTL_SBP      (1 << 2)  // Store Bad Packets
#define E1000_RCTL_UPE      (1 << 3)  // Unicast Promiscuous Enabled
#define E1000_RCTL_MPE      (1 << 4)  // Multicast Promiscuous Enabled
#define E1000_RCTL_LPE      (1 << 5)  // Long Packet Reception Enable
#define E1000_RCTL_BAM      (1 << 15) // Broadcast Accept Mode
#define E1000_RCTL_BSIZE_2048 0x00000000 // Buffer size 2048

#define E1000_TCTL_EN       (1 << 1)  // Transmit Enable
#define E1000_TCTL_PSP      (1 << 3)  // Pad Short Packets

struct e1000_rx_desc {
    uint64 addr;
    uint16 length;
    uint16 checksum;
    uint8 status;
    uint8 errors;
    uint16 special;
} __attribute__((packed));

struct e1000_tx_desc {
    uint64 addr;
    uint16 length;
    uint8 cso;
    uint8 cmd;
    uint8 status;
    uint8 css;
    uint16 special;
} __attribute__((packed));

void e1000_init(uint8 bus, uint8 slot, uint8 func);
void e1000_send_packet(void* data, uint16 length);

#endif