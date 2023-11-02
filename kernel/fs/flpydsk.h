
#ifndef _FLPYDSK_DRIVER_H
#define _FLPYDSK_DRIVER_H

#include <stdint.h>

#define FLPYDSK_SECTOR_SIZE 512

//! install floppy driver
void flpydsk_install(uint32_t irq);

//! set current working drive
void flpydsk_set_working_drive(uint8_t drive);

//! get current working drive
uint8_t flpydsk_get_working_drive();

uint8_t *flpydsk_read_sector(uint32_t sector_lba);
int32_t flpydsk_write_sector(int32_t sector_lba);
void flpydsk_set_buffer(uint8_t* buf);

#endif
