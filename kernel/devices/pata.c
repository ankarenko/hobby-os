#include "kernel/devices/pata.h"

uint8_t pata_init() {
  return 0;
}

int8_t pata_read(pata_device *device, uint32_t lba, uint8_t n_sectors, uint16_t *buffer) {
  return 0;
}

int8_t pata_write(pata_device *device, uint32_t lba, uint8_t n_sectors, uint16_t *buffer) {
  return 0;
}

int8_t patapi_read(pata_device *device, uint32_t lba, uint8_t n_sectors, uint16_t *buffer) {
  return 0;
}

pata_device *get_pata_device(char *dev_name) {
  return 0;
}
