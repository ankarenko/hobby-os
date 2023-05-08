// timer.c -- Initialises the PIT, and handles clock updates.
// Written for JamesM's kernel development tutorials.

#include <stdio.h>
#include "../cpu/idt.h"
#include "timer.h"

uint32_t tick = 0;

static void timer_callback(interrupt_registers regs)
{
  tick++;
  printf("Tick: %d \n", tick);
}

void timer_create(uint32_t frequency)
{
  // Firstly, register our timer callback.
  register_interrupt_handler(IRQ0, &timer_callback);

  // The value we send to the PIT is the value to divide it's input clock
  // (1193180 Hz) by, to get our required frequency. Important to note is
  // that the divisor must be small enough to fit into 16-bits.
  uint32_t divisor = 1193180 / frequency;

  // Send the command byte.
  outb(0x43, 0x36);

  // Divisor has to be sent byte-wise, so split here into upper/lower bytes.
  uint8_t l = (uint8_t)(0xFF);
  uint8_t h = (uint8_t)(0xFF);

  // Send the frequency divisor.
  outb(0x40, l);
  outb(0x40, h);
}
