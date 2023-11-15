#include "kernel/fs/buffer.h"
#include "kernel/fs/vfs.h"
#include "kernel/fs/flpydsk.h"
#include "kernel/devices/pata.h"
#include "kernel/memory/malloc.h"
#include "kernel/util/debug.h"

//static char buf[BYTES_PER_SECTOR];

char* bread(char *dev_name, sect_t sector, uint32_t size) {
  pata_device *device = get_pata_device(dev_name);
	char *buf = kcalloc(div_ceil(size, BYTES_PER_SECTOR) * BYTES_PER_SECTOR, sizeof(char));
	pata_read(device, sector, div_ceil(size, BYTES_PER_SECTOR), (uint16_t *)buf);
	return buf;
}

void bwrite(char *dev_name, sect_t sector, char *buf, uint32_t size) {
  pata_device *device = get_pata_device(dev_name);
	pata_write(device, sector, div_ceil(size, BYTES_PER_SECTOR), (uint16_t *)buf);
}