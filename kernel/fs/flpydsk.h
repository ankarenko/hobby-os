
#ifndef _FLPYDSK_DRIVER_H
#define _FLPYDSK_DRIVER_H

#include <stdint.h>

//! install floppy driver
void flpydsk_install(uint32_t irq);

//! set current working drive
void flpydsk_set_working_drive(uint8_t drive);

//! get current working drive
uint8_t flpydsk_get_working_drive();

//! read a sector
uint8_t *flpydsk_read_sector(uint32_t sectorLBA);

//! converts an LBA address to CHS
void flpydsk_lba_to_chs(uint32_t lba, uint32_t *head, uint32_t *track, uint32_t *sector);

#endif
