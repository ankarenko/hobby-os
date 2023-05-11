#ifndef _IDT_H
#define _IDT_H
//#include <common.h>

#include <stdint.h>
#define IRQ_HANDLER_CONTINUE 0
#define IRQ_HANDLER_STOP 1
//! i86 defines 256 possible interrupt handlers (0-255)
#define I86_MAX_INTERRUPTS 256

//! must be in the format 0D110, where D is descriptor type
#define I86_IDT_DESC_BIT16 0x06		//00000110
#define I86_IDT_DESC_BIT32 0x0E		//00001110
#define I86_IDT_DESC_RING1 0x40		//01000000
#define I86_IDT_DESC_RING2 0x20		//00100000
#define I86_IDT_DESC_RING3 0x60		//01100000
#define I86_IDT_DESC_PRESENT 0x80 //10000000

typedef struct
{
  uint32_t gs, fs, es, ds;
  uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
  uint32_t int_no, err_code;
  uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) interrupt_registers;

// A few defines to make life a little easier
#define IRQ0 32
#define IRQ1 33
#define IRQ2 34
#define IRQ3 35
#define IRQ4 36
#define IRQ5 37
#define IRQ6 38
#define IRQ7 39
#define IRQ8 40
#define IRQ9 41
#define IRQ10 42
#define IRQ11 43
#define IRQ12 44
#define IRQ13 45
#define IRQ14 46
#define IRQ15 47

typedef int32_t (*I86_IRQ_HANDLER)(struct interrupt_registers *registers);
extern void idt_flush(uint32_t);

uint32_t i86_idt_initialize(uint16_t sel);

// A struct describing an interrupt gate.
struct idt_entry_struct
{
  uint16_t base_lo;             // The lower 16 bits of the address to jump to when this interrupt fires.
  uint16_t sel;                 // Kernel segment selector.
  uint8_t  always0;             // This must always be zero.
  uint8_t  flags;               // More flags. See documentation.
  uint16_t base_hi;             // The upper 16 bits of the address to jump to.
} __attribute__((packed));
typedef struct idt_entry_struct idt_entry_t;

// A struct describing a pointer to an array of interrupt handlers.
// This is in a format suitable for giving to 'lidt'.
struct idt_ptr_struct
{
  uint16_t limit;
  uint32_t base;                // The address of the first element in our idt_entry_t array.
} __attribute__((packed));
typedef struct idt_ptr_struct idt_ptr_t;

#define ISR(index) extern void isr## index()
#define IRQ(index) extern void irq## index()

void register_interrupt_handler(uint8_t n, I86_IRQ_HANDLER handler);
int i86_install_ir(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void irq_handler(interrupt_registers *regs);
void isr_handler(interrupt_registers *regs);

ISR(0);
ISR(1);
ISR(2);
ISR(3);
ISR(4);
ISR(5);
ISR(6);
ISR(7);
ISR(8);
ISR(9);
ISR(10);
ISR(11);
ISR(12);
ISR(13);
ISR(14);
ISR(15);
ISR(16);
ISR(17);
ISR(18);
ISR(19);
ISR(20);
ISR(21);
ISR(22);
ISR(23);
ISR(24);
ISR(25);
ISR(26);
ISR(27);
ISR(28);
ISR(29);
ISR(30);
ISR(31);

IRQ(0);
IRQ(1);
IRQ(2);
IRQ(3);
IRQ(4);
IRQ(5);
IRQ(6);
IRQ(7);
IRQ(8);
IRQ(9);
IRQ(10);
IRQ(11);
IRQ(12);
IRQ(13);
IRQ(14);
IRQ(15);

#endif