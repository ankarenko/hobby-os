#include "flpdsk.h"

#include "../cpu/hal.h"

const int FLOPPY_IRQ = IRQ6;
//! dma tranfer buffer starts here and ends at 0x1000+64k
//! You can change this as needed. It must be below 16MB and in idenitity mapped memory!
const int DMA_BUFFER = 0xC0001000;  // virtual address mapped to first 16mb of physical memmpry

//! set when IRQ fires
static volatile uint8_t _FloppyDiskIRQ = 0;

void i86_flp_irq() {
  //! irq fired
  _FloppyDiskIRQ = 1;
}

inline void flpdsk_wait_irq() {
  //! wait for irq to fire
  while (_FloppyDiskIRQ == 0)
    ;
  _FloppyDiskIRQ = 0;
}

//! initialize DMA to use phys addr 1k-64k
void flpdsk_initialize_dma() {
  outportb(0x0a, 0x06);  // mask dma channel 2
  outportb(0xd8, 0xff);  // reset master flip-flop
  outportb(0x04, 0);     // address=0x1000
  outportb(0x04, 0x10);
  outportb(0xd8, 0xff);  // reset master flip-flop
  outportb(0x05, 0xff);  // count to 0x23ff (number of bytes in a 3.5" floppy disk track)
  outportb(0x05, 0x23);
  outportb(0x80, 0);     // external page register = 0
  outportb(0x0a, 0x02);  // unmask dma channel 2
}

void flpdsk_play() {
}

void flpydsk_write_ccr(uint8_t val) {
  //! write the configuation control
  outportb(FLPYDSK_CTRL, val);
}

void flpydsk_send_command(uint8_t cmd) {
  //! wait until data register is ready. We send commands to the data register
  for (int i = 0; i < 500; i++)
    if (flpydsk_read_status() & FLPYDSK_MSR_MASK_DATAREG)
      return outportb(FLPYDSK_FIFO, cmd);
}

uint8_t flpydsk_read_status() {
  //! just return main status register
  return inportb(FLPYDSK_MSR);
}

uint8_t flpydsk_read_data() {
  //! same as above function but returns data register for reading
  for (int i = 0; i < 500; i++)
    if (flpydsk_read_status() & FLPYDSK_MSR_MASK_DATAREG)
      return inportb(FLPYDSK_FIFO);
}

//! prepare the DMA for read transfer
void flpdsk_dma_read() {
  outportb(0x0a, 0x06);  // mask dma channel 2
  outportb(0x0b, 0x56);  // single transfer, address increment, autoinit, read, channel 2
  outportb(0x0a, 0x02);  // unmask dma channel 2
}

//! prepare the DMA for write transfer
void flpdsk_dma_write() {
  outportb(0x0a, 0x06);  // mask dma channel 2
  outportb(0x0b, 0x5a);  // single transfer, address increment, autoinit, write, channel 2
  outportb(0x0a, 0x02);  // unmask dma channel 2
}