#include "gdt.h"

#include <stdint.h>

#include "hal.h"

__attribute__((aligned(0x10)));
static gdt_entry_t _gdt_entries[MAX_DESCRIPTORS];
static gdt_ptr_t _gdt_ptr;

void i86_gdt_initialize() {
  _gdt_ptr.limit = (sizeof(gdt_entry_t) * MAX_DESCRIPTORS) - 1;
  _gdt_ptr.base = (uint32_t)&_gdt_entries;

  //! set null descriptor
  gdt_set_descriptor(0, 0, 0, 0, 0);

  //! set default code descriptor
  gdt_set_descriptor(1, 0, 0xffffffff,
                     I86_GDT_DESC_READWRITE | I86_GDT_DESC_EXEC_CODE | I86_GDT_DESC_CODEDATA | I86_GDT_DESC_MEMORY,
                     I86_GDT_GRAND_4K | I86_GDT_GRAND_32BIT | I86_GDT_GRAND_LIMITHI_MASK);

  //! set default data descriptor
  gdt_set_descriptor(2, 0, 0xffffffff,
                     I86_GDT_DESC_READWRITE | I86_GDT_DESC_CODEDATA | I86_GDT_DESC_MEMORY,
                     I86_GDT_GRAND_4K | I86_GDT_GRAND_32BIT | I86_GDT_GRAND_LIMITHI_MASK);

  // gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User mode code segment
  // gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User mode data segment

  gdt_flush((uint32_t)&_gdt_ptr);
}

// Set the value of one GDT entry.
int gdt_set_descriptor(int32_t i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
  if (i > MAX_DESCRIPTORS)
    return;

  //! null out the descriptor
  memset((void*)&_gdt_entries[i], 0, sizeof(gdt_entry_t));

  _gdt_entries[i].base_low = (base & 0xFFFF);
  _gdt_entries[i].base_middle = (base >> 16) & 0xFF;
  _gdt_entries[i].base_high = (base >> 24) & 0xFF;

  _gdt_entries[i].limit_low = (limit & 0xFFFF);
  _gdt_entries[i].granularity = (limit >> 16) & 0x0F;

  _gdt_entries[i].granularity |= gran & 0xF0;
  _gdt_entries[i].access = access;

  return 0;
}