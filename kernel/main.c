// #include <stdio.h>
// #include <stdint.h>
// #include <string.h>
// #include <timer.h>
#include <ctype.h>

#include "./devices/tty.h"
#include "./kernel/cpu/exception.h"
#include "./kernel/cpu/gdt.h"
#include "./kernel/cpu/hal.h"
#include "./kernel/cpu/idt.h"
#include "./kernel/devices/kybrd.h"
#include "./kernel/memory/kernel_info.h"
#include "./kernel/memory/pmm.h"
#include "./kernel/memory/vmm.h"
#include "./multiboot.h"

uint32_t calculate_memsize(multiboot_info_t *mbd, uint32_t magic) {
  if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
    printf("invalid magic number!");
    return;
  }

  
}

void print_data_layout() {
  printf("Kernel start: %X\n", KERNEL_START);
  printf("Text start: %X\n", KERNEL_TEXT_START);
  printf("Text end: %X\n", KERNEL_TEXT_END);
  printf("Data start: %X\n", KERNEL_DATA_START);
  printf("Data end: %X\n", KERNEL_DATA_END);
  printf("Stack bottom %X\n", STACK_BOTTOM);
  printf("Stack top: %X\n", STACK_TOP);
  printf("Kernel end: %X\n", KERNEL_END);
}

void kernel_main(multiboot_info_t *mbd, uint32_t magic) {
  i86_gdt_initialize();
  i86_idt_initialize(0x8);
  /* Mandatory, because the PIC interrupts are maskable. */
  enable_interrupts();
  terminal_initialize();
  init_keyboard();
  print_data_layout();
  
  pmm_init(mbd);
  
  PMM_DEBUG();

  physical_addr addr = pmm_alloc_blocks(10);

  PMM_DEBUG();
  pmm_free_block(addr);

  PMM_DEBUG();
  
  vmm_init();
  PMM_DEBUG();
  //uint32_t a = 1/0;
  
  
  /*
  initialise_paging();


  uint32_t *ptr = (uint32_t*)0xA0000000;
  uint32_t do_page_fault = *ptr;
  */
  // a(_A);

  // printf("%d", as);
  // asm volatile ("int $0x0");

  // printf("%d", c);

  // initialise_paging();

  // terminal_writestring("Hello, paging world!\n");

  // uint32_t *ptr = (uint32_t*)0xA0000000;
  // uint32_t do_page_fault = *ptr;

  // terminal_writestring("!!\n");

  /* Make sure the magic number matches for memory mapping*/

  // get_memory_info(mbd, magic);

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