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


static PFILE _cur_dir = NULL;

void user_syscall() {
  asm volatile("int $0x3");
  int32_t a = 2;
  syscall_printf("\nIn user mode");
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


void kthread () {
	int col = 0;
	bool dir = true;
	while(1) {
    printf("\nNew thread 10");
    thread_sleep(300);
	}
}


//! our simple command parser
bool run_cmd(char* cmd_buf) {
  if (strcmp(cmd_buf, "create") == 0) {
    printf("\nNew thread: \n");
    create_system_process(&kthread);
  }
  else if (strcmp(cmd_buf, "kill") == 0) {
    char id[10];

    printf("\nPlease id: \n");
    get_cmd(id, 10);

    if (id) {
      int32_t id_num = atoi(id);
      thread_kill(id_num);
    }
  }
  else if (strcmp(cmd_buf, "lst") == 0) {
    thread* th = NULL;
    printf("\nThreads running: [ ");
    list_for_each_entry(th, get_ready_threads(), sched_sibling) {
      printf("%d ", th->tid);
    }
    printf("]");

    printf("\nProcesses running: [ ");
    process* proc = NULL;
    list_for_each_entry(proc, get_proc_list(), proc_sibling) {
      printf("%d (", proc->id);
      

      list_for_each_entry(th, &proc->threads, th_sibling) {
        printf(" %d", th->tid);
      }
      printf(") ");
    }
    printf("]");

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
  } else if (strcmp(cmd_buf, "createuser") == 0) {
    create_elf_process("a:/calc.exe");
  } else if (strcmp(cmd_buf, "thread") == 0) {
    
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
    //thread_sleep(300);
    //printf("\n CMD");
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

extern void switch_to_task(thread* next_task);
void kthread_4() {
  while (1) {
    printf("\n Thread: 4");
    make_schedule();
  }
}

void kthread_5() {
  while (1) {
    //printf("\n Thread: 5");
    make_schedule();
  }
}

void main_thread() {
  thread* th = NULL;

  //create_system_process(&kthread_2);
  //create_system_process(&kthread_1);
  create_system_process(&cmd_init);

  idle_task();
}

void kernel_main(multiboot_info_t* mbd, uint32_t magic) {
  char** argv = 0;
  int argc = 0;

  hal_initialize();
  terminal_initialize();
  kkybrd_install(IRQ1);

  printf("Kernel size: %.3f mb.\nEnds in 0x%x (DIR: %d, INDEX: %d)  \n\n", 
    (float)(KERNEL_END - KERNEL_START) / 1024 / 1024, 
    KERNEL_END, 
    PAGE_DIRECTORY_INDEX(KERNEL_END), 
    PAGE_TABLE_INDEX(KERNEL_END)
  );

  pmm_init(mbd);


  vmm_init();

  flpydsk_set_working_drive(0);
  flpydsk_install(IRQ6);

  fat12_initialize();
  syscall_init();
  install_tss(5, 0x10, 0);

  
  
  initialise_multitasking(&main_thread);
  
  //_current_task = get_next_thread_to_run(); //3355447388 3222337952
  
  

  //create_system_process(&kthread_5);
  //0xc0101f05
  /*
  while (1) {
    printf("\n Thread: main");
    make_schedule();
  }
  */
  /*
  list_for_each_entry(th, get_thread_list(), sched_sibling) {
    printf("\n t: %d, esp: %x", th->tid, th->esp);
  }
  */

 
  
  
  

  /*
  process* proc = NULL ; 
  list_for_each_entry(proc, get_proc_list(), proc_siblings) { 
    printf("\nProcess: %d\n" , proc->id); 
    
    list_for_each_entry(th, &proc->threads, th_sibling) {
      printf("Thread: %d\n", th->tid);
      printf("ESP: %x", th->esp);
    }
  }
  */
  
  //scheduler_initialize();

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


  //cmd_init();

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
  /*
  for (;;) {
    printf("Thread main \n");
    thread_sleep(300);
  }
  */

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
    thread_sleep(300);
	}
}

/* thread cycles through colors of green. */
void kthread_2 () {
	int col = 0;
	bool dir = true;
	while(1) {
    printf("Thread 2\n");
    thread_sleep(300);
	}
}

/* thread cycles through colors of blue. */
void kthread_3 () {
	int col = 0;
	bool dir = true;
	while(1) {
		printf("Thread 3\n");
    thread_sleep(300);
	}
}
