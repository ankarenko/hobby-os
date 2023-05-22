#ifndef HAL_H
#define HAL_H
#include <stddef.h>
#include <stdint.h>

#include "pic.h"
#include "idt.h"

static __inline void io_wait() {
  /* Port 0x80 is used for 'checkpoints' during POST. */
  /* The Linux kernel seems to think it is free for use :-/ */
  __asm__ __volatile__("outb %%al, $0x80"
                       :
                       : "a"(0));
}

//! enable all hardware interrupts
static __inline void enable_interrupts() {
  __asm__ __volatile__("sti");
}

//! disable all hardware interrupts
static __inline void disable_interrupts() {
  __asm__ __volatile__("cli");
}

static __inline void halt() {
  __asm__ __volatile__("hlt");
}

static __inline unsigned char inportb(unsigned short _port) {
  unsigned char rv;
  asm volatile("inb %1, %0"
               : "=a"(rv)
               : "dN"(_port));
  return rv;
}

static __inline void outportb(unsigned short _port, unsigned char _data) {
  asm volatile("outb %1, %0"
               :
               : "dN"(_port), "a"(_data));
}

static __inline unsigned short inportw(unsigned short _port) {
  unsigned short rv;
  asm volatile("inw %1, %0"
               : "=a"(rv)
               : "dN"(_port));
  return rv;
}

static __inline void outportw(unsigned short _port, unsigned short _data) {
  asm volatile("outw %1, %0"
               :
               : "dN"(_port), "a"(_data));
}

static __inline unsigned int inportl(unsigned short _port) {
  unsigned int rv;
  asm volatile("inl %%dx, %%eax"
               : "=a"(rv)
               : "dN"(_port));
  return rv;
}

static __inline void outportl(unsigned short _port, unsigned int _data) {
  asm volatile("outl %%eax, %%dx"
               :
               : "dN"(_port), "a"(_data));
}

static __inline unsigned short inports(unsigned short _port) {
  unsigned short rv;
  asm volatile("inw %1, %0"
               : "=a"(rv)
               : "dN"(_port));
  return rv;
}

static __inline void outports(unsigned short _port, unsigned short _data) {
  asm volatile("outw %1, %0"
               :
               : "dN"(_port), "a"(_data));
}

static __inline void inportsw(uint16_t portid, void *addr, size_t count) {
  __asm__ __volatile__("rep insw"
                       : "+D"(addr), "+c"(count)
                       : "d"(portid)
                       : "memory");
}

static __inline void outportsw(uint16_t portid, const void *addr, size_t count) {
  __asm__ __volatile__("rep outsw"
                       : "+S"(addr), "+c"(count)
                       : "d"(portid));
}

void cpuid(int code, uint32_t *a, uint32_t *d);
const char *get_cpu_vender();

static __inline void interruptdone(uint32_t intno) {
  //! insure its a valid hardware irq
  if (intno < IRQ0 || intno > IRQ15)
    return;

  //! test if we need to send end-of-interrupt to second pic
  if (intno >= IRQ8)
    i86_pic_send_command(I86_PIC_OCW2_MASK_EOI, 1);

  //! always send end-of-interrupt to primary pic
  i86_pic_send_command(I86_PIC_OCW2_MASK_EOI, 0);
}

uint32_t hal_initialize();
uint32_t get_tick_count();

#endif