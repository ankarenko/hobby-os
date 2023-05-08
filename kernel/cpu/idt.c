#include <stdint.h>
#include <stdio.h>
#include "idt.h"
#include "hal.h"

#define IDT_INIT_ISR(i) idt_set_gate(i, (uint32_t)isr ## i, 0x08, 0x8E)
#define IDT_INIT_IRQ(i) idt_set_gate(i + 32, (uint32_t)irq ## i, 0x08, 0x8E)
static void idt_set_gate(uint8_t,uint32_t,uint16_t,uint8_t);

__attribute__((aligned(0x10)));
idt_entry_t idt_entries[256];
idt_ptr_t   idt_ptr;

/* Normally, IRQs 0 to 7 are mapped to entries 8 to 15. This
*  is a problem in protected mode, because IDT entry 8 is a
*  Double Fault! Without remapping, every time IRQ0 fires,
*  you get a Double Fault Exception, which is NOT actually
*  what's happening. We send commands to the Programmable
*  Interrupt Controller (PICs - also called the 8259's) in
*  order to make IRQ0 to 15 be remapped to IDT entries 32 to
*  47 */
void irq_remap(void)
{
  /* Remap the irq table. */
  outb(PIC_MASTER_CMD, 0x11);
  outb(PIC_SLAVE_CMD,  0x11);
  outb(PIC_MASTER_DATA, 0x20);
  outb(PIC_SLAVE_DATA,  0x28);
  outb(PIC_MASTER_DATA, 0x04);
  outb(PIC_SLAVE_DATA,  0x02);
  outb(PIC_MASTER_DATA, 0x01);
  outb(PIC_SLAVE_DATA,  0x01);
  outb(PIC_MASTER_DATA, 0x00);
  outb(PIC_SLAVE_DATA,  0x00);
}

void init_idt()
{
  idt_ptr.limit = sizeof(idt_entry_t) * 256 -1;
  idt_ptr.base  = (uint32_t)&idt_entries;

  memset(&idt_entries, 0, sizeof(idt_entry_t)*256);

  IDT_INIT_ISR(0);
  IDT_INIT_ISR(1);
  IDT_INIT_ISR(2);
  IDT_INIT_ISR(3);
  IDT_INIT_ISR(4);
  IDT_INIT_ISR(5);
  IDT_INIT_ISR(6);
  IDT_INIT_ISR(7);
  IDT_INIT_ISR(8);
  IDT_INIT_ISR(9);
  IDT_INIT_ISR(10);
  IDT_INIT_ISR(11);
  IDT_INIT_ISR(12);
  IDT_INIT_ISR(13);
  IDT_INIT_ISR(14);
  IDT_INIT_ISR(15);
  IDT_INIT_ISR(16);
  IDT_INIT_ISR(17);
  IDT_INIT_ISR(18);
  IDT_INIT_ISR(19);
  IDT_INIT_ISR(20);
  IDT_INIT_ISR(21);
  IDT_INIT_ISR(22);
  IDT_INIT_ISR(23);
  IDT_INIT_ISR(24);
  IDT_INIT_ISR(25);
  IDT_INIT_ISR(26);
  IDT_INIT_ISR(27);
  IDT_INIT_ISR(28);
  IDT_INIT_ISR(29);
  IDT_INIT_ISR(30);
  IDT_INIT_ISR(31);

  irq_remap();

  IDT_INIT_IRQ(0);
  IDT_INIT_IRQ(1);
  IDT_INIT_IRQ(2);
  IDT_INIT_IRQ(3);
  IDT_INIT_IRQ(4);
  IDT_INIT_IRQ(5);
  IDT_INIT_IRQ(6);
  IDT_INIT_IRQ(7);
  IDT_INIT_IRQ(8);
  IDT_INIT_IRQ(9);
  IDT_INIT_IRQ(10);
  IDT_INIT_IRQ(11);
  IDT_INIT_IRQ(12);
  IDT_INIT_IRQ(13);
  IDT_INIT_IRQ(14);
  IDT_INIT_IRQ(15);

  idt_flush((uint32_t)&idt_ptr);
}

static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags)
{
  idt_entries[num].base_lo = base & 0xFFFF;
  idt_entries[num].base_hi = (base >> 16) & 0xFFFF;

  idt_entries[num].sel     = sel;
  idt_entries[num].always0 = 0;
  // We must uncomment the OR below when we get to using user-mode.
  // It sets the interrupt gate's privilege level to 3.
  idt_entries[num].flags   = flags /* | 0x60 */;
}

int_callback interrupt_handlers[256];

// This gets called from our ASM interrupt handler stub.
void isr_handler(uint32_t esp)
{
  interrupt_registers *regs = (interrupt_registers *)esp;

  if (interrupt_handlers[regs->int_no] != 0)
  {
    int_callback handler = interrupt_handlers[regs->int_no];
    handler(*regs);
  }

}

void register_interrupt_handler(uint8_t n, int_callback handler)
{
  interrupt_handlers[n] = handler;
}

// This gets called from our ASM interrupt handler stub.
void irq_handler(uint32_t esp)
{
  interrupt_registers *regs = (interrupt_registers *)esp;

  // Send an EOI (end of interrupt) signal to the PICs.
  // If this interrupt involved the slave.
  if (regs->int_no >= 40)
  {
    // Send reset signal to slave.
    outb(0xA0, 0x20);
  }

  // Send reset signal to master. (As well as slave, if necessary).
  outb(0x20, 0x20);

  if (interrupt_handlers[regs->int_no] != 0)
  {
    int_callback handler = interrupt_handlers[regs->int_no];
    handler(*regs);
  }
}

