#include "pci.h"
#include "e1000.h"
#include "ac97.h"
#include "ports.h"
#include "console.h"
#include "serial.h"

uint32 pci_config_read_dword(uint8 bus, uint8 slot, uint8 func, uint8 offset) {
    uint32 address;
    uint32 lbus = (uint32)bus;
    uint32 lslot = (uint32)slot;
    uint32 lfunc = (uint32)func;
 
    address = (uint32)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xfc) | ((uint32)0x80000000));
 
    outportl(PCI_CONFIG_ADDRESS, address);
    return inportl(PCI_CONFIG_DATA);
}

uint16 pci_config_read_word(uint8 bus, uint8 slot, uint8 func, uint8 offset) {
    uint32 address;
    uint32 lbus = (uint32)bus;
    uint32 lslot = (uint32)slot;
    uint32 lfunc = (uint32)func;
 
    address = (uint32)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xfc) | ((uint32)0x80000000));
 
    outportl(PCI_CONFIG_ADDRESS, address);
    return (uint16)((inportl(PCI_CONFIG_DATA) >> ((offset & 2) * 8)) & 0xffff);
}

void pci_config_write_dword(uint8 bus, uint8 slot, uint8 func, uint8 offset, uint32 data) {
    uint32 address;
    uint32 lbus = (uint32)bus;
    uint32 lslot = (uint32)slot;
    uint32 lfunc = (uint32)func;
 
    address = (uint32)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xfc) | ((uint32)0x80000000));
 
    outportl(PCI_CONFIG_ADDRESS, address);
    outportl(PCI_CONFIG_DATA, data);
}

void pci_config_write_word(uint8 bus, uint8 slot, uint8 func, uint8 offset, uint16 data) {
    uint32 address;
    uint32 lbus = (uint32)bus;
    uint32 lslot = (uint32)slot;
    uint32 lfunc = (uint32)func;
 
    address = (uint32)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xfc) | ((uint32)0x80000000));
 
    outportl(PCI_CONFIG_ADDRESS, address);
    
    // rmw
    uint32 old = inportl(PCI_CONFIG_DATA);
    uint32 shift = (offset & 2) * 8;
    uint32 mask = 0xffff << shift;
    uint32 new_val = (old & ~mask) | ((uint32)data << shift);
    outportl(PCI_CONFIG_DATA, new_val);
}

pci_bar_t pci_get_bar(uint8 bus, uint8 slot, uint8 func, uint8 bar_index) {
    pci_bar_t bar;
    uint8 offset = 0x10 + (bar_index * 4);
    uint32 bar_value = pci_config_read_dword(bus, slot, func, offset);
    
    bar.type = (bar_value & 0x1) ? PCI_BAR_TYPE_IO : PCI_BAR_TYPE_MEM;
    
    if (bar.type == PCI_BAR_TYPE_MEM) {
        bar.base_address = bar_value & 0xFFFFFFF0;
    } else {
        bar.base_address = bar_value & 0xFFFFFFFC;
    }
    
    pci_config_write_dword(bus, slot, func, offset, 0xFFFFFFFF);
    uint32 size_readback = pci_config_read_dword(bus, slot, func, offset);
    pci_config_write_dword(bus, slot, func, offset, bar_value);
    
    if (bar.type == PCI_BAR_TYPE_MEM) {
        bar.size = ~(size_readback & 0xFFFFFFF0) + 1;
    } else {
        bar.size = ~(size_readback & 0xFFFFFFFC) + 1;
    }
    
    return bar;
}

void pci_enable_bus_mastering(uint8 bus, uint8 slot, uint8 func) {
    uint16 command = pci_config_read_word(bus, slot, func, 0x04);
    command |= (1 << 0) | (1 << 1) | (1 << 2); // set IO, mem, bus master
    pci_config_write_word(bus, slot, func, 0x04, command);
}

void pci_check_device(uint8 bus, uint8 device) {
    uint8 function = 0;
    uint16 vendor_id = pci_config_read_word(bus, device, function, 0);
    if (vendor_id == 0xFFFF) return;

    uint8 header_type = (uint8)(pci_config_read_word(bus, device, function, 0x0E) & 0xFF);
    uint8 max_functions = (header_type & 0x80) ? 8 : 1;

    for (function = 0; function < max_functions; function++) {
        vendor_id = pci_config_read_word(bus, device, function, 0);
        if (vendor_id == 0xFFFF) continue;

        uint16 device_id = pci_config_read_word(bus, device, function, 2);
        uint32 class_rev = pci_config_read_dword(bus, device, function, 8);
        uint8 class_code = (uint8)((class_rev >> 24) & 0xFF);
        uint8 subclass = (uint8)((class_rev >> 16) & 0xFF);

        kprint("PCI: BUS %d DEV %d FUNC %d - V: %x D: %x C: %x S: %x\n",
                      bus, device, function, vendor_id, device_id, class_code, subclass);
        
        if (class_code == 0x04) {
             uint32 intr = pci_config_read_dword(bus, device, function, 0x3C);
             uint8 irq = (uint8)(intr & 0xFF);
             kprint("FOUND AUDIO CONTROLLER (C: %x, S: %x, IRQ: %d)\n", class_code, subclass, irq);
             printf("FOUND AUDIO CONTROLLER: %x:%x IRQ %d\n", vendor_id, device_id, irq);
             ac97_init(bus, device, function, irq);
        }
        
        if (class_code == 0x02) {
            kprint("FOUND NETWORK CONTROLLER\n");
            printf("FOUND NETWORK CONTROLLER: %x:%x\n", vendor_id, device_id);

            // e1000
            if (vendor_id == 0x8086 && device_id == 0x100E) {
                kprint("Intel e1000 detected.\n");
                e1000_init(bus, device, function);
            }
        }
    }
}

void pci_init() {
    kprint("PCI: Starting enum...\n");
    for (uint16 bus = 0; bus < 256; bus++) {
        for (uint8 device = 0; device < 32; device++) {
            pci_check_device((uint8)bus, device);
        }
    }
    kprint("PCI: Enum is complete.\n");
}