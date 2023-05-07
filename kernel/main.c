//#include <stdio.h>
//#include <stdint.h>
//#include <string.h>
//#include <timer.h>
#include "./devices/tty.h"
#include "./kernel/cpu/idt.h"
#include "./kernel/cpu/gdt.h"
#include "./kernel/devices/kybrd.h"

void kernel_main(void) {
  init_gdt();
  init_idt();

  //init_descriptor_tables();
	terminal_initialize();
  asm volatile("sti");
  timer_create(50);
  //init_keyboard();

  /* Mandatory, because the PIC interrupts are maskable. */
  //asm volatile("sti");
  //timer_create(50);
  //keyboard_init();
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
  //printf("Hello World\n");
  //terminal_writestring("Hello");

  /*
  DUMP TEST
  char* START = (char*) 0xB8000;
  char* word = "XUY";
  memmove(START, word, 3); 
  */

  //printf("Test: %d \n %d \n %d \n", 0, -123, 123);

  // test interraptions

  //asm volatile ("int $0x1F");
  //asm volatile ("int $0x20");
  //asm volatile ("int $0x21");
  //asm volatile ("int $0x10");
  //asm volatile ("int $0x3");
  //asm volatile ("int $0x4");
}