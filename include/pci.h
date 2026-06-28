#ifndef PCI_H
#define PCI_H

#include "types.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

#define PCI_BAR_TYPE_MEM 0
#define PCI_BAR_TYPE_IO 1

typedef struct {
    uint32 base_address;
    uint32 size;
    uint8 type;
} pci_bar_t;

typedef struct {
    uint16 vendor_id;
    uint16 device_id;
    uint8 class_code;
    uint8 subclass_code;
    uint8 prog_if;
    uint8 revision_id;
    uint8 bus;
    uint8 device;
    uint8 function;
} pci_device_t;

uint32 pci_config_read_dword(uint8 bus, uint8 slot, uint8 func, uint8 offset);
uint16 pci_config_read_word(uint8 bus, uint8 slot, uint8 func, uint8 offset);
void pci_config_write_dword(uint8 bus, uint8 slot, uint8 func, uint8 offset, uint32 data);
void pci_config_write_word(uint8 bus, uint8 slot, uint8 func, uint8 offset, uint16 data);

pci_bar_t pci_get_bar(uint8 bus, uint8 slot, uint8 func, uint8 bar_index);
void pci_enable_bus_mastering(uint8 bus, uint8 slot, uint8 func);

void pci_init();

#endif
