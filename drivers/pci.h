// drivers/pci.h
#ifndef _DRIVERS_PCI_H
#define _DRIVERS_PCI_H

#include "types.h"

uint32_t pci_read_reg(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg_offset);
void pci_write_reg(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg_offset, uint32_t value);

#endif // _DRIVERS_PCI_H

