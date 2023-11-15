#include "kernel/devices/pata.h"

#include "kernel/cpu/hal.h"
#include "kernel/util/errno.h"
#include "kernel/proc/task.h"
#include "kernel/util/string/string.h"

#define MAX_ATA_DEVICE 4

static pata_device devices[MAX_ATA_DEVICE];
static uint8_t number_of_actived_devices = 0;
static volatile bool ata_irq_called;

static void pata_400ns_delays(pata_device *device) {
  inportb(device->io_base + 7);
  inportb(device->io_base + 7);
  inportb(device->io_base + 7);
  inportb(device->io_base + 7);
}

static uint8_t pata_polling_identify(pata_device *device) {
  uint8_t status;

  while (true) {
    status = inportb(device->io_base + 7);

    if (status == 0)
      return ATA_IDENTIFY_NOT_FOUND;
    if (!(status & ATA_SREG_BSY) && (inportb(device->io_base + 4) || inportb(device->io_base + 5)))
      return ATA_IDENTIFY_ERR;

    if (status & ATA_SREG_DRQ)
      break;
    if (status & ATA_SREG_ERR || status & ATA_SREG_DF)
      return ATA_IDENTIFY_ERR;
  }

  return ATA_IDENTIFY_SUCCESS;
}

static void pata_irq(interrupt_registers *regs) {
  ata_irq_called = true;
}

static uint8_t pata_polling(pata_device *device) {
  uint8_t status;

  while (true) {
    status = inportb(device->io_base + 7);
    if (!(status & ATA_SREG_BSY) || (status & ATA_SREG_DRQ))
      return ATA_POLLING_SUCCESS;
    if ((status & ATA_SREG_ERR) || (status & ATA_SREG_DF))
      return ATA_POLLING_ERR;

    thread_sleep(1);
  }
}

static uint8_t pata_identify(pata_device *device) {
  outportb(device->io_base + 6, device->is_master ? 0xA0 : 0xB0);
  pata_400ns_delays(device);

  outportb(device->io_base + 2, 0);
  outportb(device->io_base + 3, 0);
  outportb(device->io_base + 4, 0);
  outportb(device->io_base + 5, 0);
  outportb(device->io_base + 7, 0xEC);

  uint8_t identify_status = pata_polling_identify(device);
  if (identify_status != ATA_IDENTIFY_SUCCESS)
    return identify_status;

  if (!(inportb(device->io_base + 7) & ATA_SREG_ERR)) {
    uint16_t buffer[256];

    inportsw(device->io_base, buffer, 256);

    return ATA_IDENTIFY_SUCCESS;
  }
  return ATA_IDENTIFY_ERR;
}

static void pata_wait_irq() {
  while (!ata_irq_called)
    ;
  ata_irq_called = false;
}

static struct ata_device *pata_detect(uint16_t io_addr1, uint16_t io_addr2, uint8_t irq, bool is_master, char *dev_name) {
  pata_device *device = &devices[number_of_actived_devices];

  device->io_base = io_addr1;
  device->associated_io_base = io_addr2;
  device->irq = irq;
  device->is_master = is_master;

  if (pata_identify(device) == ATA_IDENTIFY_SUCCESS) {
    device->is_harddisk = true;
    device->dev_name = dev_name;
    devices[number_of_actived_devices++] = *device;
    return device;
  }

  memset(device, 0, sizeof(pata_device));
  return 0;
}

int8_t pata_read(pata_device *device, uint32_t lba, uint8_t n_sectors, uint16_t *buffer) {
  outportb(device->io_base + 6, (device->is_master ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F));
  pata_400ns_delays(device);

  outportb(device->io_base + 1, 0x00);
  outportb(device->io_base + 2, n_sectors);
  outportb(device->io_base + 3, (uint8_t)lba);
  outportb(device->io_base + 4, (uint8_t)(lba >> 8));
  outportb(device->io_base + 5, (uint8_t)(lba >> 16));
  outportb(device->io_base + 7, 0x20);

  if (pata_polling(device) == ATA_POLLING_ERR)
    return -ENXIO;

  for (int i = 0; i < n_sectors; ++i) {
    inportsw(device->io_base, buffer + i * 256, 256);
    pata_400ns_delays(device);

    if (pata_polling(device) == ATA_POLLING_ERR)
      return -ENXIO;
  }
  return 0;
}

int8_t pata_write(pata_device *device, uint32_t lba, uint8_t n_sectors, uint16_t *buffer) {
  outportb(device->io_base + 6, (device->is_master ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F));
  pata_400ns_delays(device);

  outportb(device->io_base + 1, 0x00);
  outportb(device->io_base + 2, n_sectors);
  outportb(device->io_base + 3, (uint8_t)lba);
  outportb(device->io_base + 4, (uint8_t)(lba >> 8));
  outportb(device->io_base + 5, (uint8_t)(lba >> 16));
  outportb(device->io_base + 7, 0x30);

  if (pata_polling(device) == ATA_POLLING_ERR)
    return -ENXIO;

  for (int i = 0; i < n_sectors; ++i) {
    outportsw(device->io_base, buffer + i * 256, 256);
    outportw(device->io_base + 7, 0xE7);
    pata_400ns_delays(device);

    if (pata_polling(device) == ATA_POLLING_ERR)
      return -ENXIO;
  }
  return 0;
}

pata_device *get_pata_device(char *dev_name) {
  for (uint8_t i = 0; i < MAX_ATA_DEVICE; ++i) {
    if (strcmp(devices[i].dev_name, dev_name) == 0)
      return &devices[i];
  }
  return NULL;
}

uint8_t pata_init() {
  memset(&devices, 0, MAX_ATA_DEVICE * sizeof(pata_device));

  register_interrupt_handler(ATA0_IRQ, pata_irq);
  register_interrupt_handler(ATA1_IRQ, pata_irq);

  pata_detect(ATA0_IO_ADDR1, ATA0_IO_ADDR2, ATA0_IRQ, true, "/dev/hda");
  pata_detect(ATA0_IO_ADDR1, ATA0_IO_ADDR2, ATA0_IRQ, false, "/dev/hdb");
  pata_detect(ATA1_IO_ADDR1, ATA1_IO_ADDR2, ATA1_IRQ, true, "/dev/hdc");
  pata_detect(ATA1_IO_ADDR1, ATA1_IO_ADDR2, ATA1_IRQ, false, "/dev/hdd");

  return 0;
}
