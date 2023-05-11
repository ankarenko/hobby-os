#include <stdint.h>
#include <stdio.h>
#include "../devices/tty.h"
#include "idt.h"
#include "pic.h"
#include "hal.h"

#define IDT_INIT_ISR(i, sel) i86_install_ir(i, (uint32_t)isr ## i, sel, I86_IDT_DESC_PRESENT | I86_IDT_DESC_BIT32)
#define IDT_INIT_IRQ(i, sel) i86_install_ir(i + 32, (uint32_t)irq ## i, sel, I86_IDT_DESC_PRESENT | I86_IDT_DESC_BIT32)

__attribute__((aligned(0x10)));
static idt_entry_t _idt_entries[I86_MAX_INTERRUPTS];
static I86_IRQ_HANDLER _interrupt_handlers[I86_MAX_INTERRUPTS];
static idt_ptr_t   _idt_ptr;

void i86_default_handler () {
  disable_interrupts();

  terminal_writestring("*** [i86 Hal] i86_default_handler: Unhandled Exception");
	
  for(;;);
}


uint32_t i86_idt_initialize(uint16_t sel)
{
  _idt_ptr.limit = sizeof(idt_entry_t) * I86_MAX_INTERRUPTS -1;
  _idt_ptr.base  = (uint32_t)&_idt_entries;

  memset(&_idt_entries, 0, sizeof(idt_entry_t)*I86_MAX_INTERRUPTS);

  IDT_INIT_ISR(0, sel);
  IDT_INIT_ISR(1, sel);
  IDT_INIT_ISR(2, sel);
  IDT_INIT_ISR(3, sel);
  IDT_INIT_ISR(4, sel);
  IDT_INIT_ISR(5, sel);
  IDT_INIT_ISR(6, sel);
  IDT_INIT_ISR(7, sel);
  IDT_INIT_ISR(8, sel);
  IDT_INIT_ISR(9, sel);
  IDT_INIT_ISR(10, sel);
  IDT_INIT_ISR(11, sel);
  IDT_INIT_ISR(12, sel);
  IDT_INIT_ISR(13, sel);
  IDT_INIT_ISR(14, sel);
  IDT_INIT_ISR(15, sel);
  IDT_INIT_ISR(16, sel);
  IDT_INIT_ISR(17, sel);
  IDT_INIT_ISR(18, sel);
  IDT_INIT_ISR(19, sel);
  IDT_INIT_ISR(20, sel);
  IDT_INIT_ISR(21, sel);
  IDT_INIT_ISR(22, sel);
  IDT_INIT_ISR(23, sel);
  IDT_INIT_ISR(24, sel);
  IDT_INIT_ISR(25, sel);
  IDT_INIT_ISR(26, sel);
  IDT_INIT_ISR(27, sel);
  IDT_INIT_ISR(28, sel);
  IDT_INIT_ISR(29, sel);
  IDT_INIT_ISR(30, sel);
  IDT_INIT_ISR(31, sel);

  pic_remap();

  IDT_INIT_IRQ(0, sel);
  IDT_INIT_IRQ(1, sel);
  IDT_INIT_IRQ(2, sel);
  IDT_INIT_IRQ(3, sel);
  IDT_INIT_IRQ(4, sel);
  IDT_INIT_IRQ(5, sel);
  IDT_INIT_IRQ(6, sel);
  IDT_INIT_IRQ(7, sel);
  IDT_INIT_IRQ(8, sel);
  IDT_INIT_IRQ(9, sel);
  IDT_INIT_IRQ(10, sel);
  IDT_INIT_IRQ(11, sel);
  IDT_INIT_IRQ(12, sel);
  IDT_INIT_IRQ(13, sel);
  IDT_INIT_IRQ(14, sel);
  IDT_INIT_IRQ(15, sel);

  for (uint32_t i = 0; i < I86_MAX_INTERRUPTS; ++i) {
    if (i == IRQ0) { //except timer
      continue;
    }
    register_interrupt_handler(i, i86_default_handler);
  }

  idt_flush((uint32_t)&_idt_ptr);
  
  return 0;
}

int i86_install_ir(uint8_t i, uint32_t base, uint16_t sel, uint8_t flags)
{
  if (i > I86_MAX_INTERRUPTS)
		return 0;

  _idt_entries[i].base_lo = base & 0xFFFF;
  _idt_entries[i].base_hi = (base >> 16) & 0xFFFF;

  _idt_entries[i].sel = sel;
  _idt_entries[i].always0 = 0;
  // We must uncomment the OR below when we get to using user-mode.
  // It sets the interrupt gate's privilege level to 3.
  _idt_entries[i].flags   = flags /* | 0x60 */;
  return 0;
}

void handle_interrupt(interrupt_registers *regs) {
  if (_interrupt_handlers[regs->int_no] != 0)
  {
    I86_IRQ_HANDLER handler = _interrupt_handlers[regs->int_no];
    if (handler(regs) == IRQ_HANDLER_STOP)
			return;
  }
}

// This gets called from our ASM interrupt handler stub.
void isr_handler(interrupt_registers *regs)
{
  handle_interrupt(regs);
}

void register_interrupt_handler(uint8_t n, I86_IRQ_HANDLER handler)
{
  _interrupt_handlers[n] = handler;
}

// This gets called from our ASM interrupt handler stub.
void irq_handler(interrupt_registers *regs)
{
  if (regs->int_no >= 40)
		outportb(PIC2_COMMAND, PIC_EOI);
	outportb(PIC1_COMMAND, PIC_EOI);

  handle_interrupt(regs);
}

