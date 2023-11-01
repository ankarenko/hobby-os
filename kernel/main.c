#include <ctype.h>
#include <math.h>
#include <list.h>
#include <errno.h>

#include "test/greatest.h"

#include "kernel/util/debug.h"
#include "kernel/devices/tty.h"
#include "kernel/cpu/exception.h"
#include "kernel/cpu/gdt.h"
#include "kernel/cpu/hal.h"
#include "kernel/cpu/idt.h"
#include "kernel/cpu/tss.h"
#include "kernel/devices/kybrd.h"
#include "kernel/fs/vfs.h"
#include "kernel/memory/kernel_info.h"
#include "kernel/memory/malloc.h"
#include "kernel/memory/pmm.h"
#include "kernel/memory/vmm.h"
#include "kernel/proc/elf.h"
#include "kernel/proc/task.h"
#include "kernel/system/sysapi.h"
#include "kernel/fs/fat32/fat32.h"
#include "multiboot.h"

void cmd_init();

//! sleeps a little bit. This uses the HALs get_tick_count() which in turn uses the PIT
void sleep(uint32_t ms) {
  int32_t ticks = ms + get_tick_count();
  while (ticks > get_tick_count())
    ;
}

static vfs_file* _cur_dir = NULL;

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
        //terminal_clrline();
        //terminal_newline();
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
    printf("\nnew thread: ");
    create_system_process(&kthread);
  } else if (strcmp(cmd_buf, "kill") == 0) {
    char id[10];

    printf("\nid: ");
    get_cmd(id, 10);

    if (id) {
      int32_t id_num = atoi(id);
      thread_kill(id_num);
    }
  }
  else if (strcmp(cmd_buf, "lst") == 0) {
    thread* th = NULL;
    printf("\nthreads running: [ ");
    list_for_each_entry(th, get_ready_threads(), sched_sibling) {
      printf("%d ", th->tid);
    }
    printf("]");

    printf("\nprocesses running: [ ");
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
  } else if (strcmp(cmd_buf, "cd") == 0) {
    char filepath[100];

    printf("\npath: ");
    get_cmd(filepath, 100);

    if (!vfs_cd(filepath)) {
      printf("\ndirectory not found");
    }
  } else if (strcmp(cmd_buf, "read") == 0) {
    cmd_read_sect();
  } else if (strcmp(cmd_buf, "cat") == 0) {
    cmd_read_file();
  } else if (strcmp(cmd_buf, "ls") == 0) {
    char filepath[100];

    printf("\npath: ");
    get_cmd(filepath, 100);

    if (!vfs_ls(filepath)) {
      printf("\ndirectory not found");
    }
    //ls(filepath);
  } else if (strcmp(cmd_buf, "run") == 0) {
    char filepath[100];
    printf("\npath: ");
    get_cmd(filepath, 100);

    create_elf_process(filepath);
  } else if (strcmp(cmd_buf, "") == 0) {
    
  } else {
    printf("\ninvalid command");
  }

  return false;
}

void cmd_read_file() {
  char filepath[100];

  printf("\npath: ");
  get_cmd(filepath, 100);

  int32_t fd = vfs_open(filepath, 0);
  
  if (fd == -ENOENT) {
    printf("\nfile not found");
    return;
  }

  uint32_t size = 1;
  uint8_t* buf = kcalloc(size + 1, sizeof(uint8_t));
  printf("\n_____________________________________________\n");
  while (vfs_fread(fd, buf, size) == size) {
    buf[size] = '\0';
    printf(buf);
  };
  printf("____________________________________________");
  
  /*
  uint8_t* buf = vfs_read(filepath);
  printf("\n_____________________________________________\n");
  printf(buf);
  printf("____________________________________________");
  kfree(buf);
  */
}

extern void cmd_init() {
  char cmd_buf[100];

  while (1) {
    printf("\nroot@%s: ", pwd());
    get_cmd(cmd_buf, 98);

    if (run_cmd(cmd_buf) == true)
      break;
  }
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

void main_thread() {
  thread* th = NULL;

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

  fat32_init();

  //fat12_initialize();
  syscall_init();
  install_tss(5, 0x10, 0);

  initialise_multitasking(&main_thread);

  return 0;
}
