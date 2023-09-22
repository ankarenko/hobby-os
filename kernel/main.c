// #include <stdio.h>
// #include <stdint.h>
// #include <string.h>
// #include <timer.h>
#include <ctype.h>
#include <math.h>
#include "include/list.h"

#include "../test/greatest.h"
#include "./devices/tty.h"
#include "./kernel/cpu/exception.h"
#include "./kernel/cpu/gdt.h"
#include "./kernel/cpu/hal.h"
#include "./kernel/cpu/idt.h"
#include "./kernel/cpu/tss.h"
#include "./kernel/devices/kybrd.h"
#include "./kernel/fs/fat12/fat12.h"
#include "./kernel/fs/flpydsk.h"
#include "./kernel/fs/fsys.h"
#include "./kernel/memory/kernel_info.h"
#include "./kernel/memory/malloc.h"
#include "./kernel/memory/pmm.h"
#include "./kernel/memory/vmm.h"
#include "./kernel/proc/elf.h"
#include "./kernel/proc/task.h"
#include "./kernel/system/sysapi.h"
#include "./multiboot.h"

extern void enter_usermode();
extern void asm_syscall_print();

void kthread_1();
void kthread_2();
void kthread_3();
void cmd_init();


//! sleeps a little bit. This uses the HALs get_tick_count() which in turn uses the PIT
void sleep(uint32_t ms) {
  int32_t ticks = ms + get_tick_count();
  while (ticks > get_tick_count())
    ;
}

//! wait for key stroke
enum KEYCODE getch() {
  enum KEYCODE key = KEY_UNKNOWN;

  //! wait for a keypress
  while (key == KEY_UNKNOWN)
    key = kkybrd_get_last_key();

  //! discard last keypress (we handled it) and return
  kkybrd_discard_last_key();
  return key;
}

//! gets next command
void get_cmd(char* buf, int n) {
  int32_t i = 0;
  while (i < n) {
    enum KEYCODE key = getch();

    switch (key) {
      case KEY_BACKSPACE:
        terminal_popchar();
        i = max(i - 1, 0);
        break;
      case KEY_RETURN:
        buf[i] = '\0';
        terminal_clrline();
        // terminal_newline();
        return;
      case KEY_UNKNOWN:
        break;
      default:
        char c = kkybrd_key_to_ascii(key);

        if (isascii(c)) {
          terminal_write(&c, 1);
        }

        buf[i] = c;
        i++;
        break;
    }
  }
}

static PFILE _cur_dir = NULL;

void user_syscall() {
  asm volatile("int $0x3");
  int32_t a = 2;
  syscall_printf("\nIn user mode");
}

void cmd_user() {
  int32_t esp;
  __asm__ __volatile__("mov %%esp, %0"
                       : "=r"(esp));
  tss_set_stack(0x10, esp);

  uint8_t* phys = pmm_alloc_frame();
  uint8_t* virt = 0x40002000;
  
  
  vmm_map_address(vmm_get_directory(), 
    virt, phys, 
    I86_PDE_PRESENT | I86_PDE_PRESENT| I86_PDE_WRITABLE
  );

  


  enter_usermode();
  int32_t a = 1;
  //asm_syscall_print();

  __asm__ __volatile__(
      "mov %0, %%eax;	      \n"
      "int $0x80;           \n"
      : "=a"(a)
  );

  //syscall_printf("\nIn user mode\n");
  
}

//! our simple command parser
bool run_cmd(char* cmd_buf) {
  if (strcmp(cmd_buf, "user") == 0) {
    cmd_user();
  } else if (strcmp(cmd_buf, "exit") == 0) {
    printf("Goodbuy!");
    return true;
  } else if (strcmp(cmd_buf, "layout") == 0) {
    printf("Kernel start: %X\n", KERNEL_START);
    printf("Text start: %X\n", KERNEL_TEXT_START);
    printf("Text end: %X\n", KERNEL_TEXT_END);
    printf("Data start: %X\n", KERNEL_DATA_START);
    printf("Data end: %X\n", KERNEL_DATA_END);
    printf("Stack bottom %X\n", STACK_BOTTOM);
    printf("Stack top: %X\n", STACK_TOP);
    printf("Kernel end: %X\n", KERNEL_END);
  } else if (strcmp(cmd_buf, "dump") == 0) {
    PMM_DEBUG();
  } else if (strcmp(cmd_buf, "clear") == 0) {
    terminal_clrscr();
  } else if (strcmp(cmd_buf, "read") == 0) {
    cmd_read_sect();
  } else if (strcmp(cmd_buf, "cat") == 0) {
    cmd_read_file();
  } else if (strcmp(cmd_buf, "ls") == 0) {
    cmd_read_ls();
  } else if (strcmp(cmd_buf, "process") == 0) {
    // loads process in user space and puts it in a schedule que
    process_load("a:/calc.exe");
  } else if (strcmp(cmd_buf, "thread") == 0) {
    /* create kernel threads. */
    virtual_addr stack1 = 0;
    virtual_addr stack2 = 0;
    virtual_addr stack3 = 0;
    virtual_addr stack4 = 0;
    
    if (
      !create_kernel_stack(&stack1) ||
      !create_kernel_stack(&stack2) || 
      !create_kernel_stack(&stack3)
    ) {
      return false;
    }

    queue_insert(thread_create(0, (virtual_addr)kthread_1, stack1, true));
    queue_insert(thread_create(0, (virtual_addr)kthread_2, stack2, true));
    //queue_insert(thread_create(0, (virtual_addr)kthread_3, stack3, true));
    queue_insert(thread_create(0, (virtual_addr)cmd_init, stack4, true));
    
    /* execute idle thread. */
    execute_idle();
  }

  else {
    printf("Invalid command\n");
  }

  return false;
}

void cmd_read_file() {
  char filepath[100];

  printf("\nPlease enter file path \n");
  get_cmd(filepath, 100);

  FILE file = vol_open_file(filepath);

  if (file.flags == FS_INVALID) {
    printf("\n*** File not found ***\n");
    return;
  }

  uint8_t buf[512];
  printf("___________________%s_________________________\n", filepath);
  while (!file.eof) {
    vol_read_file(&file, buf, 512);
    printf(buf);
  }
  printf("\n___________________EOF_________________________\n");
}

extern void cmd_init() {
  char cmd_buf[100];

  while (1) {
    get_cmd(cmd_buf, 98);

    if (run_cmd(cmd_buf) == true)
      break;
  }
}

void cmd_read_ls() {
  char filepath[100];

  printf("\nPlease folder path \n");
  get_cmd(filepath, 100);

  FILE folder;

  if (strcmp(filepath, "") == 0) {
    printf("a:\n");
    folder = vol_get_root('a');
  } else {
    printf("a:/%s\n", filepath);
    folder = vol_open_file(filepath);
  }

  vol_ls(&folder);
}

//! read sector command
void cmd_read_sect() {
  uint32_t sectornum = 0;
  char sectornumbuf[4];
  uint8_t* sector = 0;

  printf("\nPlease type in the sector number [0 is default] \n");
  get_cmd(sectornumbuf, 3);
  sectornum = atoi(sectornumbuf);

  printf("\nSector %d contents:\n\n", sectornum);

  //! read sector from disk
  sector = flpydsk_read_sector(sectornum);

  //! display sector
  if (sector != 0) {
    int i = 0;
    for (int c = 0; c < 4; c++) {
      for (int j = 0; j < 128; j++)
        printf("%x ", sector[i + j]);
      i += 128;

      printf("\nPress any key to continue\n");
      getch();
    }
  } else
    printf("\n*** Error reading sector from disk\n");

  printf("\nDone.");
}

GREATEST_MAIN_DEFS();


struct just_item {
	int data;
  struct list_head list; /* kernel's list structure */	
};



void kernel_main(multiboot_info_t* mbd, uint32_t magic) {
  char** argv = 0;
  int argc = 0;

  hal_initialize();
  terminal_initialize();
  kkybrd_install(IRQ1);

	
  /*
  int items_size = 10;
  struct just_item items[items_size];
  LIST_HEAD(items_head);

  for (int i = 0; i < items_size; ++i) {
    tmp = (struct just_item *)malloc(sizeof(struct just_item));
    items[i].data = i;
    INIT_LIST_HEAD(&items[i].list);
    list_add_tail(&items[i].list, &items_head);
  }
  
  struct list_head* pos; 
  list_for_each(pos, &items_head) { 
    printf("surfing the linked list next = %x and prev = %x\n" , 
      pos->next, 
      pos->prev 
    );
  }

  struct just_item* ret = list_entry(items_head.prev, typeof(*ret), list);
  printf("erased: %d\n", ret->data);

  struct just_item* item = NULL ; 
  list_for_each_entry(item, &items_head, list) { 
    printf("data  =  %d\n" , item->data); 
  }
  */

  pmm_init(mbd);

  vmm_init();

  flpydsk_set_working_drive(0);
  flpydsk_install(IRQ6);

  fat12_initialize();
  syscall_init();
  install_tss(5, 0x10, 0);

  scheduler_initialize();

  // char* path = "/folder/content.txt";
  // char* path2 = "names.txt";

  // FILE file = vol_open_file("folder");
  // vol_ls(&file);
  /*
  uint8_t buf[512];

  while (!file.eof)
  {
    vol_read_file(&file, buf, 512);
    printf(buf);
  }
  */


  cmd_init();

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


/* thread cycles through colors of red. */
void kthread_1 () {
	int col = 0;
	bool dir = true;
	while(1) {
    printf("Thread 1\n");
    thread_sleep(200);
	}
}

/* thread cycles through colors of green. */
void kthread_2 () {
	int col = 0;
	bool dir = true;
	while(1) {
    printf("Thread 2\n");
    thread_sleep(200);
	}
}

/* thread cycles through colors of blue. */
void kthread_3 () {
	int col = 0;
	bool dir = true;
	while(1) {
		printf("Thread 3\n");
    thread_sleep(200);
	}
}
