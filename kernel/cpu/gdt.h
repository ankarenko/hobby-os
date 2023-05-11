#ifndef _GDT_H
#define _GDT_H

#include <stdint.h>

//! maximum amount of descriptors allowed
#define MAX_DESCRIPTORS 6

/***	 gdt descriptor access bit flags.	***/

//! set access bit
#define I86_GDT_DESC_ACCESS 0x0001	//00000001

//! descriptor is readable and writable. default: read only
#define I86_GDT_DESC_READWRITE 0x0002  //00000010

//! set expansion direction bit
#define I86_GDT_DESC_EXPANSION 0x0004  //00000100

//! executable code segment. Default: data segment
#define I86_GDT_DESC_EXEC_CODE 0x0008  //00001000

//! set code or data descriptor. defult: system defined descriptor
#define I86_GDT_DESC_CODEDATA 0x0010  //00010000

//! set dpl bits
#define I86_GDT_DESC_DPL 0x0060	 //01100000

//! set "in memory" bit
#define I86_GDT_DESC_MEMORY 0x0080	//10000000

/**	gdt descriptor grandularity bit flags	***/

//! masks out limitHi (High 4 bits of limit)
#define I86_GDT_GRAND_LIMITHI_MASK 0x0f	 //00001111

//! set os defined bit
#define I86_GDT_GRAND_OS 0x10  //00010000

//! set if 32bit. default: 16 bit
#define I86_GDT_GRAND_32BIT 0x40  //01000000

//! 4k grandularity. default: none
#define I86_GDT_GRAND_4K 0x80  //10000000


// This structure contains the value of one GDT entry.
// We use the attribute 'packed' to tell GCC not to change
// any of the alignment in the structure.
struct gdt_entry_struct
{
  uint16_t limit_low;           // The lower 16 bits of the limit.
  uint16_t base_low;            // The lower 16 bits of the base.
  uint8_t  base_middle;         // The next 8 bits of the base.
  uint8_t  access;              // Access flags, determine what ring this segment can be used in.
  uint8_t  granularity;
  uint8_t  base_high;           // The last 8 bits of the base.
} __attribute__((packed));
typedef struct gdt_entry_struct gdt_entry_t;

struct gdt_ptr_struct
{
  uint16_t limit;               // The upper 16 bits of all selector limits.
  uint32_t base;                // The address of the first gdt_entry_t struct.
}
 __attribute__((packed));
typedef struct gdt_ptr_struct gdt_ptr_t;

// Lets us access our ASM functions from our C code.
extern void gdt_flush(uint32_t);

// Internal function prototypes.
void i86_gdt_initialize();
int gdt_set_descriptor(int32_t, uint32_t, uint32_t, uint8_t, uint8_t);

#endif