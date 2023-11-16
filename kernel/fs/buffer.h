#ifndef KERNEL_FS_BUFFER_H
#define KERNEL_FS_BUFFER_H

#include <stdint.h>
#include "kernel/fs/vfs.h"

char* bread(char *dev_name, sect_t sector, uint32_t size);
void bwrite(char *dev_name, sect_t sector, char *buf, uint32_t size);
char* breads(char *dev_name, sect_t sector); //cached

#endif