#include "kernel/cpu/exception.h"
#include "kernel/cpu/gdt.h"
#include "kernel/cpu/hal.h"
#include "kernel/cpu/idt.h"
#include "kernel/cpu/tss.h"
#include "kernel/devices/kybrd.h"
#include "kernel/devices/pata.h"
#include "kernel/devices/tty.h"
#include "kernel/fs/char_dev.h"
#include "kernel/fs/ext2/ext2.h"
#include "kernel/locking/semaphore.h"
#include "kernel/fs/fat32/fat32.h"
#include "kernel/fs/vfs.h"
#include "kernel/memory/kernel_info.h"
#include "kernel/memory/malloc.h"
#include "kernel/memory/pmm.h"
#include "kernel/memory/vmm.h"
#include "kernel/proc/elf.h"
#include "kernel/proc/task.h"
#include "kernel/system/sysapi.h"
#include "kernel/system/time.h"
#include "kernel/util/ctype.h"
#include "kernel/util/debug.h"
#include "kernel/util/errno.h"
#include "kernel/util/fcntl.h"
#include "kernel/util/list.h"
#include "kernel/system/timer.h"
#include "kernel/util/math.h"

#include "multiboot.h"
#include "test/greatest.h"

extern struct vfs_file_system_type ext2_fs_type;

void cmd_init();
void idle_task();

//! sleeps a little bit. This uses the HALs get_tick_count() which in turn uses the PIT
/*
void sleep(uint32_t ms) {
  int32_t ticks = ms + get_tick_count();
  while (ticks > get_tick_count())
    ;
}
*/

static struct vfs_file* _cur_dir = NULL;
static struct int32 *kybrd_fd = -1;

//! gets next command
void get_cmd(char* buf, int n) {
  int32_t i = 0;
  struct key_event ev;
  while (i < n) {
    vfs_fread(kybrd_fd, &ev, sizeof(ev));
    if (ev.type == KEY_RELEASE) {
      continue;
    }
    enum KEYCODE key = ev.key;

    switch (key) {
      case KEY_BACKSPACE:
        terminal_popchar();
        i = max(i - 1, 0);
        break;
      case KEY_RETURN:
        buf[i] = '\0';
        // terminal_clrline();
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

#define N 100
static int buffer[N];
static int i = -1;

static INIT_MUTEX(mutex);
static INIT_SEMAPHORE(left, N);
static INIT_SEMAPHORE(reserved, N);

void init_consumer_producer() {
  left.count = N;
  reserved.count = 0;
  for (int i = 0; i < N; ++i) {
    buffer[i] = 0;
  } 
}

void consumer() {
  while (1) {
    log("Consumer: start");

    semaphore_down(&reserved);
    semaphore_down(&mutex);
    log("Consumer: enter CS");

    buffer[i] = 0;
    log("Consumed %d", i);
    i--;
    
    
    semaphore_up(&mutex);
    log("Consumer: leave CS");
    semaphore_up(&left);

    thread_sleep(300);
  }
}

void producer() {
  while (1) {
    log("Producer: start");

    semaphore_down(&left);
    semaphore_down(&mutex);
    log("Producer: enter CS");
    
    ++i;
    buffer[i] = 1;
    log("Produced %d", i);

    semaphore_up(&mutex);
    log("Producer: leave CS");
    semaphore_up(&reserved);

    thread_sleep(300);
  }
}

void kthread() {
  int col = 0;
  bool dir = true;
  while (1) {
    printf("\nNew thread 10");
    thread_sleep(3000);
  }
}

//! our simple command parser
bool run_cmd(char* cmd_buf) {
  if (strcmp(cmd_buf, "create") == 0) {
    printf("\nnew thread: ");
    create_system_process(&kthread);
  } if (strcmp(cmd_buf, "play") == 0) {
    init_consumer_producer();
    create_system_process(&producer);
    create_system_process(&producer);
    create_system_process(&consumer);
    create_system_process(&consumer);
  } else if (strcmp(cmd_buf, "kill") == 0) {
    char id[10];

    printf("\nid: ");
    get_cmd(id, 10);

    if (id) {
      int32_t id_num = atoi(id);
      thread_kill(id_num);
    }
  } else if (strcmp(cmd_buf, "lst") == 0) {
    thread* th = NULL;
    printf("\nthreads running: [ ");
    list_for_each_entry(th, get_ready_threads(), sched_sibling) {
      printf("%d ", th->tid);
    }
    printf("]");

    printf("\nprocesses running: [ ");
    process* proc = NULL;
    list_for_each_entry(proc, get_proc_list(), proc_sibling) {
      printf("%d (", proc->pid);

      list_for_each_entry(th, &proc->threads, th_sibling) {
        printf(" %d", th->tid);
      }
      printf(") ");
    }
    printf("]");

  } else if (strcmp(cmd_buf, "exit") == 0) {
    printf("Goodbuy!");
    return true;
  } else if (strcmp(cmd_buf, "signal") == 0) {
    char id[10];
    char signal[10];

    printf("\nid: ");
    get_cmd(id, 10);

    printf("\nsignal: ");
    get_cmd(signal, 10);
    
    if (id && signal && thread_signal(atoi(id), atoi(signal))) {
      log("Signal: sent to process: %s", id);
    } else {
      printf("Unable to find process with id %s", id);
    }

  } else if (strcmp(cmd_buf, "time") == 0) {
    struct time* t = get_time(0);  // current time
    printf("\nCurrent time (UTC): %d:%d:%d, %d.%d.%d", t->hour, t->minute, t->second, t->day, t->month, t->year);
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

    int ret = vfs_cd(filepath);
    if (ret < 0) {
      printf(ret == -ENOTDIR ? "\nnot a directory" : "\ndirectory not found");
    }
  } else if (strcmp(cmd_buf, "mfile") == 0) {
    char filepath[100];

    printf("\npath: ");
    get_cmd(filepath, 100);
    int fd = 0;
    if ((fd = vfs_open(filepath, O_CREAT, S_IFREG)) < 0) {
      printf("\ndirectory not found");
    }
    vfs_close(fd);
  } else if (strcmp(cmd_buf, "write") == 0) {
    char text[100];

    printf("\npath: ");
    get_cmd(text, 100);

    int32_t fd = vfs_open(text, O_CREAT | O_APPEND, S_IFREG);
    if (fd < 0) {
      printf("\nunable to open file");
    }

    printf("\ntext: ");
    get_cmd(text, 100);

    char* line = "#helloworl-helloworl-helloworl-helloworl-helloworl-helloworl-helloworl-hellowor#";

    // vfs_flseek(fd, 11, SEEK_SET);
    if (vfs_fwrite(fd, text, strlen(text)) < 0) {
      printf("\nnot a file");
    } else {
      printf("\nfile updated");
    }
    vfs_close(fd);
    // vfs_flseek(fd, 80, SEEK_SET);
    // vfs_fwrite(fd, line, 100);
    // vfs_fwrite(fd, "!", 1);

  } else if (strcmp(cmd_buf, "test") == 0) {
    printf("\nStart:");
    uint8_t* program = vfs_read("calc.exe");
    kfree(program);

    printf("\nEnd");
  } else if (strcmp(cmd_buf, "rm") == 0) {
    char filepath[100];

    printf("\npath: ");
    get_cmd(filepath, 100);

    if (vfs_unlink(filepath, 0) < 0) {
      printf("\nfile or directory not found");
    }
  } else if (strcmp(cmd_buf, "mkdir") == 0) {
    char filepath[100];

    printf("\npath: ");
    get_cmd(filepath, 100);

    if (vfs_mkdir(filepath, 0) < 0) {
      printf("\ncannot create directory");
    }
  } else if (strcmp(cmd_buf, "read") == 0) {
    cmd_read_sect();
  } else if (strcmp(cmd_buf, "cat") == 0) {
    cmd_read_file();
  } else if (strcmp(cmd_buf, "dcat") == 0) {
    cmd_read_kybrd();
  } else if (strcmp(cmd_buf, "ls") == 0) {
    char filepath[100];

    printf("\npath: ");
    get_cmd(filepath, 100);

    if (!vfs_ls(filepath)) {
      printf("\ndirectory not found");
    }
    // ls(filepath);
  } else if (strcmp(cmd_buf, "run") == 0) {
    char filepath[100];
    printf("\npath: ");
    get_cmd(filepath, 100);
    process* cur_proc = get_current_process();
    create_elf_process(cur_proc, filepath);
  } else if (strcmp(cmd_buf, "") == 0) {
  } else {
    printf("\ninvalid command");
  }

  return false;
}

void cmd_read_kybrd() {
  char filepath[100];

  printf("\npath: ");
  get_cmd(filepath, 100);

  int32_t fd = vfs_open(filepath, O_RDONLY, 0);

  if (fd == -ENOENT || fd < 0) {
    printf("\nfile not found");
    return;
  }

  uint32_t size = sizeof(struct key_event);
  uint8_t* buf = kcalloc(1, size);
  printf("\n_____________________________________________\n");
  while (vfs_fread(fd, buf, size) >= 0) {
    struct key_event *ev = buf;
    char c = kkybrd_key_to_ascii(ev->key);
    printf("%c", c);
  };
  printf("\n____________________________________________");
  kfree(buf);
  vfs_close(fd);
  /*
  uint8_t* buf = vfs_read(filepath);
  printf("\n_____________________________________________\n");
  printf(buf);
  printf("____________________________________________");
  kfree(buf);
  */
}

void cmd_read_file() {
  char filepath[100];

  printf("\npath: ");
  get_cmd(filepath, 100);

  int32_t fd = vfs_open(filepath, O_RDONLY, 0);

  if (fd == -ENOENT || fd < 0) {
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
  printf("\n____________________________________________");

  vfs_close(fd);
  /*
  uint8_t* buf = vfs_read(filepath);
  printf("\n_____________________________________________\n");
  printf(buf);
  printf("____________________________________________");
  kfree(buf);
  */
}

extern void cmd_init() {
  if ((kybrd_fd = vfs_open("/dev/input/kybrd", O_RDONLY)) < 0) {
    err("Cannot open keyboard");
  }

  char cmd_buf[100];

  while (1) {
    process* proc = get_current_process();
    printf("\n(%s)root@%s: ",
           strcmp(proc->fs->mnt_root->mnt_devname, "/dev/hda") == 0 ? "ext2" : proc->fs->mnt_root->mnt_devname,
           proc->fs->d_root->d_name);

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
      //getch();
    }
  } else
    printf("\n*** Error reading sector from disk\n");

  printf("\nDone.");
}

GREATEST_MAIN_DEFS();

static int fd_ptmx = -1;

void kybrd_manager() {
  int kybrd_fd = 0;
  if ((kybrd_fd = vfs_open("/dev/input/kybrd", O_RDONLY)) < 0) {
    err("Cannot open keyboard");
  }

  struct key_event ev;

  while (true) {
    vfs_fread(kybrd_fd, &ev, sizeof(ev));
    char c = kkybrd_key_to_ascii(ev.key);
    log("%c", c);

    if (pts_driver) {
      struct tty_struct *pts = list_first_entry(&pts_driver->ttys, struct tty_struct, sibling);
      if (pts != NULL) {
        struct tty_struct *ptm = pts->link;
        ptm->ldisc->write(ptm, NULL, &c, 1);
      }
    }
        
  }
}

void main_thread() {
  thread* th = NULL;

  create_system_process(&idle_task);

  vfs_init(&ext2_fs_type, "/dev/hda");
  chrdev_init();

  kkybrd_install(IRQ1);
  
  tty_init();
  fd_ptmx = 0;
  // returns fd of a master terminal
  if ((fd_ptmx = vfs_open("/dev/ptmx", O_RDONLY)) < 0) {
    err("Unable to open ptmx");
  }

  create_system_process(&kybrd_manager);
  cmd_init();
}

void kernel_main(multiboot_info_t* mbd, uint32_t magic) {
  char** argv = 0;
  int argc = 0;

  // setup serial port A for debug
  debug_init();

  disable_interrupts();
  i86_gdt_initialize();
  i86_idt_initialize(0x8);
  install_tss(5, 0x10, 0);
  enable_interrupts();
  
  terminal_initialize();
  
  log("Kernel size: %dKB", (int)(KERNEL_END - KERNEL_START) / 1024);
  log("Ends in 0x%x (DIR: %d, INDEX: %d)", KERNEL_END, PAGE_DIRECTORY_INDEX(KERNEL_END), PAGE_TABLE_INDEX(KERNEL_END));

  pmm_init(mbd);
  vmm_init();

  exception_init();
  hal_initialize();
  
  pata_init();
  syscall_init();

  timer_init();
  initialise_multitasking(&main_thread);

  return 0;
}
