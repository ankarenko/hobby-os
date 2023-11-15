#ifndef KERNEL_DEVICES_PATA_H
#define KERNEL_DEVICES_PATA_H

#include <stdbool.h>
#include <stdint.h>

#include "kernel/cpu/idt.h"

#define ATA0_IO_ADDR1 0x1F0
#define ATA0_IO_ADDR2 0x3F0
#define ATA0_IRQ IRQ14
#define ATA1_IO_ADDR1 0x170
#define ATA1_IO_ADDR2 0x370
#define ATA1_IRQ IRQ15

#define ATA_SREG_ERR 0x01
#define ATA_SREG_DF 0x20
#define ATA_SREG_DRQ 0x08
#define ATA_SREG_BSY 0x80

#define ATA_POLLING_ERR 0
#define ATA_POLLING_SUCCESS 1

#define ATA_IDENTIFY_ERR 0
#define ATA_IDENTIFY_SUCCESS 1
#define ATA_IDENTIFY_NOT_FOUND 2

typedef struct _pata_device {
	uint16_t io_base;
	uint16_t associated_io_base;
	uint8_t irq;
	char *dev_name;
	bool is_master;
	bool is_harddisk;
} pata_device;

uint8_t pata_init();
int8_t pata_read(pata_device *device, uint32_t lba, uint8_t n_sectors, uint16_t *buffer);
int8_t pata_write(pata_device *device, uint32_t lba, uint8_t n_sectors, uint16_t *buffer);
pata_device *get_pata_device(char *dev_name);

#endif