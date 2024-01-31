#ifndef _IDT_H
#define _IDT_H
#include <stdint.h>

#define IRQ_HANDLER_CONTINUE 0
#define IRQ_HANDLER_STOP 1
//! i86 defines 256 possible interrupt handlers (0-255)
#define I86_MAX_INTERRUPTS 256

//! must be in the format 0D110, where D is descriptor type
#define I86_IDT_DESC_BIT16 0x06    // 00000110
#define I86_IDT_DESC_BIT32 0x0E    // 00001110
#define I86_IDT_DESC_RING1 0x40    // 01000000
#define I86_IDT_DESC_RING2 0x20    // 00100000
#define I86_IDT_DESC_RING3 0x60    // 01100000
#define I86_IDT_DESC_PRESENT 0x80  // 10000000
#define DISPATCHER_ISR 128  // 0x80

#pragma pack(1)
typedef struct
{
  uint32_t gs, fs, es, ds;
  uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
  uint32_t int_no, err_code;
  uint32_t eip, cs, eflags, useresp, ss;  //TODO: reaname useresp to esp as it's not always true
} /*__attribute__((packed))*/ interrupt_registers;
#pragma pack(0)

#pragma pack(1)
typedef struct {
  uint32_t ebp, edi, esi, ebx;
	uint32_t eip;  
} /*__attribute__((packed))*/ switch_task_frame;
#pragma pack(0)


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

typedef int32_t (*I86_IRQ_HANDLER)(interrupt_registers *registers);

// A struct describing an interrupt gate.
struct idt_entry_struct {
  uint16_t base_lo;  // The lower 16 bits of the address to jump to when this interrupt fires.
  uint16_t sel;      // Kernel segment selector.
  uint8_t always0;   // This must always be zero.
  uint8_t flags;     // More flags. See documentation.
  uint16_t base_hi;  // The upper 16 bits of the address to jump to.
} __attribute__((packed));
typedef struct idt_entry_struct idt_entry_t;

// A struct describing a pointer to an array of interrupt handlers.
// This is in a format suitable for giving to 'lidt'.
struct idt_ptr_struct {
  uint16_t limit;
  uint32_t base;  // The address of the first element in our idt_entry_t array.
} __attribute__((packed));
typedef struct idt_ptr_struct idt_ptr_t;

void register_interrupt_handler(uint8_t n, I86_IRQ_HANDLER handler);
int32_t i86_install_ir(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
uint32_t i86_idt_initialize(uint16_t sel);
idt_entry_t* i86_get_ir(uint32_t i);

#endif