// drivers/pci.c
#include "drivers/pci.h"
#include "io.h"

#define PCI_CONFIG_ADDRESS 0xCF_CONFIG_DATA    0xCFC

uint32_t pci_read_reg(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg_offset) {
    uint32_t address = ()(0x80000000 |
                                  ((uint32_t)bus << 16) |
                                  ((uint32_t)device << 11) |
                                  ((uint32_t)function << 8) |
                                  (reg_offset & 0xFC));
    
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_write_reg(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg_offset, uint32_t value) {
    uint32_t address = (uint32_t)(0x80000000 |
                                  ((uint32_t)bus << 16) |
                                  ((uint32_t)device << 11) |
                                  ((uint32_t)function << 8) |
                                  (reg_offset & 0xFC));
    
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

