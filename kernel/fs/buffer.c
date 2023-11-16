#include "kernel/fs/buffer.h"
#include "kernel/util/string/string.h"
#include "kernel/fs/vfs.h"
#include "kernel/fs/flpydsk.h"
#include "kernel/devices/pata.h"
#include "kernel/memory/malloc.h"
#include "kernel/util/debug.h"

#define NO_CACHE -1;

static uint8_t cache[BYTES_PER_SECTOR];
static sect_t cache_sector = NO_CACHE;

static bool check_cache(char* buf, sect_t sector) {
  if (sector == cache_sector) {
    memcpy(buf, &cache, BYTES_PER_SECTOR);
    return true;
  }

  return false;
}

static void set_cache(sect_t sector, uint8_t* buf) {
  cache_sector = sector;
  memcpy(&cache, buf, BYTES_PER_SECTOR);
}

static void clear_cache() {
  cache_sector = NO_CACHE;
}

// reads one sector, does not allocate any data
char* breads(char *dev_name, sect_t sector) {
  if (sector == cache_sector) {
    return &cache;
  }

  pata_device *device = get_pata_device(dev_name);
	if (pata_read(device, sector, 1, (uint16_t *)cache) == 0) {
    cache_sector = sector;
    return &cache;
  }
   
  return 0;
}

char* bread(char *dev_name, sect_t sector, uint32_t size) {
  char *buf = kcalloc(div_ceil(size, BYTES_PER_SECTOR) * BYTES_PER_SECTOR, sizeof(char));
  pata_device *device = get_pata_device(dev_name);
	pata_read(device, sector, div_ceil(size, BYTES_PER_SECTOR), (uint16_t *)buf);
  clear_cache();
	return buf;
}

void bwrite(char *dev_name, sect_t sector, char *buf, uint32_t size) {
  pata_device *device = get_pata_device(dev_name);
	pata_write(device, sector, div_ceil(size, BYTES_PER_SECTOR), (uint16_t *)buf);
  clear_cache();
}