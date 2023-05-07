#ifndef HAL_H
#define HAL_H
#include <stdint.h>

#define IRQ_MASTER_0 31
#define IRQ_SLAVE_0 40
#define PIC_MASTER_CMD 0x20
#define PIC_MASTER_DATA 0x21
#define PIC_SLAVE_CMD 0xA0
#define PIC_SLAVE_DATA 0xA1
#define PIC_CMD_RESET 0x20

void outb(uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);


//! enable all hardware interrupts
static __inline void enable_interrupts()
{
	__asm__ __volatile__("sti");
}

//! disable all hardware interrupts
static __inline void disable_interrupts()
{
	__asm__ __volatile__("cli");
}

#endif