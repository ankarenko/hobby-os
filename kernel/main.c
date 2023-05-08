// #include <stdio.h>
// #include <stdint.h>
// #include <string.h>
// #include <timer.h>
#include "./devices/tty.h"
#include "./kernel/cpu/idt.h"
#include "./kernel/cpu/gdt.h"
#include "./kernel/cpu/hal.h"
#include "./kernel/devices/kybrd.h"
#include "./kernel/memory/vmm.h"
#include "./multiboot.h"
#include "./kernel/memory/kernel_info.h"

void kernel_main(multiboot_info_t *mbd, uint32_t magic)
{
  init_gdt();
  init_idt();
  /* Mandatory, because the PIC interrupts are maskable. */
  enable_interrupts();
  terminal_initialize();
  init_keyboard();
  // initialise_paging();

  // terminal_writestring("Hello, paging world!\n");

  // uint32_t *ptr = (uint32_t*)0xA0000000;
  // uint32_t do_page_fault = *ptr;

  // terminal_writestring("!!\n");

  /* Make sure the magic number matches for memory mapping*/
  if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
  {
    printf("invalid magic number!");
  }

  /* Check bit 6 to see if we have a valid memory map */
  if (!(mbd->flags >> 6 & 0x1))
  {
    printf("invalid memory map given by GRUB bootloader");
  }

  /* Loop through the memory map and display the values */
  int i;
  for (i = 0; i < mbd->mmap_length;
       i += sizeof(multiboot_memory_map_t))
  {
    multiboot_memory_map_t *mmmt =
        (multiboot_memory_map_t *)(mbd->mmap_addr + i);

    printf("Start Addr: %x | Length: %d | Size: %d | Type: %d\n",
           mmmt->addr_low, mmmt->len_low, mmmt->size, mmmt->type);

    if (mmmt->type == MULTIBOOT_MEMORY_AVAILABLE)
    {
      /*
       * Do something with this memory block!
       * BE WARNED that some of memory shown as availiable is actually
       * actively being used by the kernel! You'll need to take that
       * into account before writing to memory!
       */
    }
  }

  printf("Kernel start: %x\n", KERNEL_START);
  printf("Kernel end: %x\n", KERNEL_END);

  return 0;

  // terminal_putchar((int)'a');
  // terminal_putchar((int)'b');
  // init_keyboard();
  // timer_create(1000);

  // keyboard_init();
  /*
  for (size_t y = 0; y <= 24; ++y) {
    size_t size = y == 0? 79 : 80;
    unsigned char line[size];
    for (int x = 0; x < size; ++x) {
      line[x] = 65 + y;
    }

    terminal_writestring(line);
  }
  */
  // printf("Hello World\n");
  // terminal_writestring("Hello");

  /*
  DUMP TEST
  char* START = (char*) 0xB8000;
  char* word = "XUY";
  memmove(START, word, 3);
  */

  // printf("Test: %d \n %d \n %d \n", 0, -123, 123);

  // test interraptions

  // asm volatile ("int $0x1F");
  // asm volatile ("int $0x20");
  // asm volatile ("int $0x21");
  // asm volatile ("int $0x10");
  // asm volatile ("int $0x3");
  // asm volatile ("int $0x4");
}