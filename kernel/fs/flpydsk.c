#include "flpydsk.h"

#include <stdbool.h>
#include "kernel/util/string/string.h"

#include "kernel/cpu/hal.h"
#include "kernel/memory/kernel_info.h"
#include "kernel/proc/task.h"

/*
**	Controller I/O Ports. Please see chapter for additional ports
*/

enum FLPYDSK_IO {

  FLPYDSK_DOR = 0x3f2,
  FLPYDSK_MSR = 0x3f4,
  FLPYDSK_FIFO = 0x3f5,
  FLPYDSK_CTRL = 0x3f7
};

/**
 *	Bits 0-4 of command byte. Please see chapter for additional commands
 */

enum FLPYDSK_CMD {

  FDC_CMD_READ_TRACK = 2,
  FDC_CMD_SPECIFY = 3,
  FDC_CMD_CHECK_STAT = 4,
  FDC_CMD_WRITE_SECT = 5,
  FDC_CMD_READ_SECT = 6,
  FDC_CMD_CALIBRATE = 7,
  FDC_CMD_CHECK_INT = 8,
  FDC_CMD_FORMAT_TRACK = 0xd,
  FDC_CMD_SEEK = 0xf
};

/**
 *	Additional command masks. Can be masked with above commands
 */

enum FLPYDSK_CMD_EXT {

  FDC_CMD_EXT_SKIP = 0x20,       // 00100000
  FDC_CMD_EXT_DENSITY = 0x40,    // 01000000
  FDC_CMD_EXT_MULTITRACK = 0x80  // 10000000
};

/*
**	Digital Output Register
*/

enum FLPYDSK_DOR_MASK {

  FLPYDSK_DOR_MASK_DRIVE0 = 0,         // 00000000	= here for completeness sake
  FLPYDSK_DOR_MASK_DRIVE1 = 1,         // 00000001
  FLPYDSK_DOR_MASK_DRIVE2 = 2,         // 00000010
  FLPYDSK_DOR_MASK_DRIVE3 = 3,         // 00000011
  FLPYDSK_DOR_MASK_RESET = 4,          // 00000100
  FLPYDSK_DOR_MASK_DMA = 8,            // 00001000
  FLPYDSK_DOR_MASK_DRIVE0_MOTOR = 16,  // 00010000
  FLPYDSK_DOR_MASK_DRIVE1_MOTOR = 32,  // 00100000
  FLPYDSK_DOR_MASK_DRIVE2_MOTOR = 64,  // 01000000
  FLPYDSK_DOR_MASK_DRIVE3_MOTOR = 128  // 10000000
};

/**
 *	Main Status Register
 */

enum FLPYDSK_MSR_MASK {

  FLPYDSK_MSR_MASK_DRIVE1_POS_MODE = 1,  // 00000001
  FLPYDSK_MSR_MASK_DRIVE2_POS_MODE = 2,  // 00000010
  FLPYDSK_MSR_MASK_DRIVE3_POS_MODE = 4,  // 00000100
  FLPYDSK_MSR_MASK_DRIVE4_POS_MODE = 8,  // 00001000
  FLPYDSK_MSR_MASK_BUSY = 16,            // 00010000
  FLPYDSK_MSR_MASK_DMA = 32,             // 00100000
  FLPYDSK_MSR_MASK_DATAIO = 64,          // 01000000
  FLPYDSK_MSR_MASK_DATAREG = 128         // 10000000
};

/**
 *	Controller Status Port 0
 */

enum FLPYDSK_ST0_MASK {

  FLPYDSK_ST0_MASK_DRIVE0 = 0,      // 00000000	=	for completness sake
  FLPYDSK_ST0_MASK_DRIVE1 = 1,      // 00000001
  FLPYDSK_ST0_MASK_DRIVE2 = 2,      // 00000010
  FLPYDSK_ST0_MASK_DRIVE3 = 3,      // 00000011
  FLPYDSK_ST0_MASK_HEADACTIVE = 4,  // 00000100
  FLPYDSK_ST0_MASK_NOTREADY = 8,    // 00001000
  FLPYDSK_ST0_MASK_UNITCHECK = 16,  // 00010000
  FLPYDSK_ST0_MASK_SEEKEND = 32,    // 00100000
  FLPYDSK_ST0_MASK_INTCODE = 64     // 11000000
};

/*
** LPYDSK_ST0_MASK_INTCODE types
*/

enum FLPYDSK_ST0_INTCODE_TYP {

  FLPYDSK_ST0_TYP_NORMAL = 0,
  FLPYDSK_ST0_TYP_ABNORMAL_ERR = 1,
  FLPYDSK_ST0_TYP_INVALID_ERR = 2,
  FLPYDSK_ST0_TYP_NOTREADY = 3
};

/**
 *	GAP 3 sizes
 */

enum FLPYDSK_GAP3_LENGTH {

  FLPYDSK_GAP3_LENGTH_STD = 42,
  FLPYDSK_GAP3_LENGTH_5_14 = 32,
  FLPYDSK_GAP3_LENGTH_3_5 = 27
};

/*
**	Formula: 2^sector_number * 128, where ^ denotes "to the power of"
*/

enum FLPYDSK_SECTOR_DTL {

  FLPYDSK_SECTOR_DTL_128 = 0,
  FLPYDSK_SECTOR_DTL_256 = 1,
  FLPYDSK_SECTOR_DTL_512 = 2,
  FLPYDSK_SECTOR_DTL_1024 = 4
};

#define FLOPPY_IRQ 6
#define FLPY_SECTORS_PER_TRACK 18
#define FLPY_HEADS_PER_CYLINDER 2
#define DMA_BUFFER 0x1000  // TODO: make it virtual address, adding KERNEL_HIGH_HALF doesnt work
#define FDC_DMA_CHANNEL 2
#define NO_CACHE -1

//! used to wait in miliseconds
extern void sleep(int32_t);

//! current working drive. Defaults to 0 which should be fine on most systems
static uint8_t current_drive = 0;

//! set when IRQ fires
static volatile uint8_t floppy_disk_irq = 0;
static int32_t last_sector_loaded = -1;
static int8_t dma_cache_buffer[FLPYDSK_SECTOR_SIZE];

static void flpydsk_flush_cache() {
  last_sector_loaded = NO_CACHE;
}

static void flpydsk_save_cache(uint32_t sector_lba) {
  last_sector_loaded = sector_lba;
  memcpy(&dma_cache_buffer, DMA_BUFFER, FLPYDSK_SECTOR_SIZE);
}

bool dma_initialize_floppy(uint8_t* buffer, unsigned length) {
  union {
    uint8_t byte[4];  // Lo[0], Mid[1], Hi[2]
    unsigned long l;
  } a, c;

  a.l = (unsigned)buffer;
  c.l = (unsigned)length - 1;

  // Check for buffer issues
  // Specifically it checks that buffer address is at most 24 bits (under 16MB)
  // that length is at most 16-bit (since that's what DMA accepts),
  // and that if you add the buffer address and the length, you don't get need a carry.
  // if(address & 0xff000000 || length & 0xffff0000 || ((address+length)&0xffff0000)!=(address&0xffff0000))
  if ((a.l >> 24) || (c.l >> 16) || (((a.l & 0xffff) + c.l) >> 16)) {
    return false;
  }

  dma_reset(1);
  dma_mask_channel(FDC_DMA_CHANNEL);  // Mask channel 2
  dma_reset_flipflop(1);              // Flipflop reset on DMA 1

  dma_set_address(FDC_DMA_CHANNEL, a.byte[0], a.byte[1]);  // Buffer address
  dma_reset_flipflop(1);                                   // Flipflop reset on DMA 1

  dma_set_count(FDC_DMA_CHANNEL, c.byte[0], c.byte[1]);  // Set count
  dma_set_read(FDC_DMA_CHANNEL);

  dma_unmask_all(1);  // Unmask channel 2

  return true;
}

/**
 *	Basic Controller I/O Routines
 */

//! return fdc status
uint8_t flpydsk_read_status() {
  //! just return main status register
  return inportb(FLPYDSK_MSR);
}

//! write to the fdc dor
void flpydsk_write_dor(uint8_t val) {
  //! write the digital output register
  outportb(FLPYDSK_DOR, val);
}

//! send command byte to fdc
void flpydsk_send_command(uint8_t cmd) {
  //! wait until data register is ready. We send commands to the data register
  for (int32_t i = 0; i < 500; i++)
    if (flpydsk_read_status() & FLPYDSK_MSR_MASK_DATAREG)
      return outportb(FLPYDSK_FIFO, cmd);
}

//! get data from fdc
uint8_t flpydsk_read_data() {
  //! same as above function but returns data register for reading
  for (int32_t i = 0; i < 500; i++)
    if (flpydsk_read_status() & FLPYDSK_MSR_MASK_DATAREG)
      return inportb(FLPYDSK_FIFO);

  return 0;
}

//! write to the configuation control register
void flpydsk_write_ccr(uint8_t val) {
  //! write the configuation control
  outportb(FLPYDSK_CTRL, val);
}

/**
 *	Interrupt Handling Routines
 */
void flpydsk_wait_irq();
//! wait for irq to fire
inline void flpydsk_wait_irq() {
  //! wait for irq to fire
  while (floppy_disk_irq == 0) {
    thread_sleep(1);
  };
  floppy_disk_irq = 0;
}

//!	floppy disk irq handler
void i86_flpy_irq() {
  //! irq fired
  floppy_disk_irq = 1;
}

/**
 *	Controller Command Routines
 */

//! check interrupt status command
void flpydsk_check_int(uint32_t* st0, uint32_t* cyl) {
  flpydsk_send_command(FDC_CMD_CHECK_INT);

  *st0 = flpydsk_read_data();
  *cyl = flpydsk_read_data();
}

//! turns the current floppy drives motor on/off
void flpydsk_control_motor(bool b) {
  //! sanity check: invalid drive
  if (current_drive > 3)
    return;

  uint32_t motor = 0;

  //! select the correct mask based on current drive
  switch (current_drive) {
    case 0:
      motor = FLPYDSK_DOR_MASK_DRIVE0_MOTOR;
      break;
    case 1:
      motor = FLPYDSK_DOR_MASK_DRIVE1_MOTOR;
      break;
    case 2:
      motor = FLPYDSK_DOR_MASK_DRIVE2_MOTOR;
      break;
    case 3:
      motor = FLPYDSK_DOR_MASK_DRIVE3_MOTOR;
      break;
  }

  //! turn on or off the motor of that drive
  if (b)
    flpydsk_write_dor(current_drive | motor | FLPYDSK_DOR_MASK_RESET | FLPYDSK_DOR_MASK_DMA);
  else
    flpydsk_write_dor(FLPYDSK_DOR_MASK_RESET);

  //! in all cases; wait a little bit for the motor to spin up/turn off
  // sleep(20);
  thread_sleep(1);
}

//! configure drive
void flpydsk_drive_data(uint32_t stepr, uint32_t loadt, uint32_t unloadt, bool dma) {
  uint32_t data = 0;

  //! send command
  flpydsk_send_command(FDC_CMD_SPECIFY);
  data = ((stepr & 0xf) << 4) | (unloadt & 0xf);
  flpydsk_send_command(data);
  data = (loadt) << 1 | (dma == false) ? 0 : 1;
  flpydsk_send_command(data);
}

//! calibrates the drive
int32_t flpydsk_calibrate(uint32_t drive) {
  uint32_t st0, cyl;

  if (drive >= 4)
    return -2;

  //! turn on the motor
  flpydsk_control_motor(true);

  for (int32_t i = 0; i < 10; i++) {
    //! send command
    flpydsk_send_command(FDC_CMD_CALIBRATE);
    flpydsk_send_command(drive);
    flpydsk_wait_irq();
    flpydsk_check_int(&st0, &cyl);

    //! did we fine cylinder 0? if so, we are done
    if (!cyl) {
      flpydsk_control_motor(false);
      return 0;
    }
  }

  flpydsk_control_motor(false);
  return -1;
}

//! disable controller
void flpydsk_disable_controller() {
  flpydsk_write_dor(0);
}

//! enable controller
void flpydsk_enable_controller() {
  flpydsk_write_dor(FLPYDSK_DOR_MASK_RESET | FLPYDSK_DOR_MASK_DMA);
}

//! reset controller
void flpydsk_reset() {
  uint32_t st0, cyl;

  //! reset the controller
  flpydsk_disable_controller();
  flpydsk_enable_controller();
  flpydsk_wait_irq();

  //! send CHECK_INT/SENSE INTERRUPT command to all drives
  for (int32_t i = 0; i < 4; i++)
    flpydsk_check_int(&st0, &cyl);

  //! transfer speed 500kb/s
  flpydsk_write_ccr(0);

  //! pass mechanical drive info. steprate=3ms, unload time=240ms, load time=16ms
  flpydsk_drive_data(3, 16, 240, true);

  //! calibrate the disk
  flpydsk_calibrate(current_drive);
}

//! read a sector
static void flpydsk_read_sector_imp(uint8_t head, uint8_t track, uint8_t sector) {
  uint32_t st0, cyl;

  //! set the DMA for read transfer
  dma_set_read(FDC_DMA_CHANNEL);

  //! read in a sector
  flpydsk_send_command(
      FDC_CMD_READ_SECT | FDC_CMD_EXT_MULTITRACK | FDC_CMD_EXT_SKIP | FDC_CMD_EXT_DENSITY);
  flpydsk_send_command(head << 2 | current_drive);
  flpydsk_send_command(track);
  flpydsk_send_command(head);
  flpydsk_send_command(sector);
  flpydsk_send_command(FLPYDSK_SECTOR_DTL_512);
  flpydsk_send_command(((sector + 1) >= FLPY_SECTORS_PER_TRACK) ? FLPY_SECTORS_PER_TRACK : sector + 1);
  flpydsk_send_command(FLPYDSK_GAP3_LENGTH_3_5);
  flpydsk_send_command(0xff);

  //! wait for irq
  flpydsk_wait_irq();

  //! read status info
  for (int32_t j = 0; j < 7; j++)
    flpydsk_read_data();

  //! let FDC know we handled interrupt
  flpydsk_check_int(&st0, &cyl);
}

// write a sector
static void flpydsk_write_sector_imp(unsigned char head, unsigned char track, unsigned char sector) {
  uint32_t st0, cyl;
  //! set the DMA for read transfer
  dma_set_write(FDC_DMA_CHANNEL);

  flpydsk_send_command(FDC_CMD_WRITE_SECT | FDC_CMD_EXT_MULTITRACK | FDC_CMD_EXT_DENSITY);  // write a sector
  flpydsk_send_command(head << 2 | current_drive);
  flpydsk_send_command(track);
  flpydsk_send_command(head);
  flpydsk_send_command(sector);
  flpydsk_send_command(FLPYDSK_SECTOR_DTL_512);
  flpydsk_send_command(((sector + 1) >= FLPY_SECTORS_PER_TRACK) ? FLPY_SECTORS_PER_TRACK : sector + 1);
  flpydsk_send_command(FLPYDSK_GAP3_LENGTH_3_5);
  flpydsk_send_command(0xFF);

  flpydsk_wait_irq();

  int j;
  for (j = 0; j < 7; ++j)
    flpydsk_read_data();          // read status info

  flpydsk_check_int(&st0, &cyl);  // let FDC know we handled interrupt
}

//! seek to given track/cylinder
static int32_t flpydsk_seek(uint32_t cyl, uint32_t head) {
  uint32_t st0, cyl0;

  if (current_drive >= 4)
    return -1;

  for (int32_t i = 0; i < 10; i++) {
    //! send the command
    flpydsk_send_command(FDC_CMD_SEEK);
    flpydsk_send_command((head) << 2 | current_drive);
    flpydsk_send_command(cyl);

    //! wait for the results phase IRQ
    flpydsk_wait_irq();
    flpydsk_check_int(&st0, &cyl0);

    //! found the cylinder?
    if (cyl0 == cyl)
      return 0;
  }

  return -1;
}

//! convert LBA to CHS
void flpydsk_lba_to_chs(uint32_t lba, uint32_t* head, uint32_t* track, uint32_t* sector) {
  *head = (lba % (FLPY_SECTORS_PER_TRACK * FLPY_HEADS_PER_CYLINDER)) / (FLPY_SECTORS_PER_TRACK);
  *track = lba / (FLPY_SECTORS_PER_TRACK * FLPY_HEADS_PER_CYLINDER);
  *sector = lba % FLPY_SECTORS_PER_TRACK + 1;
}

//! install floppy driver
void flpydsk_install(uint32_t irq) {
  //! install irq handler
  register_interrupt_handler(irq, i86_flpy_irq);

  //! initialize the DMA for FDC
  dma_initialize_floppy((uint8_t*)DMA_BUFFER, FLPYDSK_SECTOR_SIZE);

  //! reset the fdc
  flpydsk_reset();

  //! set drive information
  flpydsk_drive_data(13, 1, 0xf, true);
}

//! set current working drive
void flpydsk_set_working_drive(uint8_t drive) {
  if (drive < 4)
    current_drive = drive;
}

//! get current working drive
uint8_t flpydsk_get_working_drive() {
  return current_drive;
}

//! read a sector
uint8_t* flpydsk_read_sector(uint32_t sector_lba) {
  // TODO: be very careful with that, if you happen to have some
  // bugs with reading disk,
  // this is the first place to look
  if (last_sector_loaded == sector_lba) {
    return &dma_cache_buffer;
  }

  if (current_drive >= 4)
    return NULL;

  //! convert LBA sector to CHS
  int32_t head = 0, track = 0, sector = 1;
  flpydsk_lba_to_chs(sector_lba, &head, &track, &sector);

  //! turn motor on and seek to track
  flpydsk_control_motor(true);
  if (flpydsk_seek(track, head) != 0)
    return 0;

  //! read sector and turn motor off
  flpydsk_read_sector_imp(head, track, sector);
  flpydsk_control_motor(false);

  flpydsk_save_cache(sector_lba);
  return (uint8_t*)DMA_BUFFER;
}

void flpydsk_set_buffer(uint8_t* buf) {
  flpydsk_flush_cache();
  memcpy(DMA_BUFFER, buf, FLPYDSK_SECTOR_SIZE);
}

// write a sector
int32_t flpydsk_write_sector(int32_t sector_lba) {
  if (current_drive >= 4)
    return -1;

  flpydsk_save_cache(sector_lba);
  int32_t head = 0, track = 0, sector = 1;
  flpydsk_lba_to_chs(sector_lba, &head, &track, &sector);
  
  flpydsk_control_motor(true);
  if (flpydsk_seek(track, head)) 
    return -2;

  flpydsk_write_sector_imp(head, track, sector);
  flpydsk_control_motor(false);

  return 0;
}