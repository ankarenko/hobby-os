#include "kernel/cpu/exception.h"
#include "kernel/cpu/gdt.h"
#include "kernel/cpu/hal.h"
#include "kernel/cpu/idt.h"
#include "kernel/cpu/tss.h"
#include "kernel/devices/kybrd.h"
#include "kernel/devices/pata.h"
#include "kernel/devices/terminal.h"
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

static struct vfs_file* _cur_dir = NULL;

//! gets next command
void get_cmd(char* buf, int n) {
  vfs_fread(1, buf, n);
  buf[n] = '\0';
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
    kprintf("\nNew struct thread 10");
    thread_sleep(3000);
  }
}

void kthread_fork() {
  int col = 0;
  bool dir = true;
  
  struct process *current_process = get_current_process();
  int parent_id = current_process->pid;

  if (process_fork(current_process) == 0) {
    while (1) {
      kprintf("\nHello from child");
      thread_sleep(3000);
    }
  } else {
    while (1) {
      kprintf("\nHello from parent");
      thread_sleep(3000);
    }
  }
}

void execve(void *entry()) {
  struct process *parent = get_current_process();
  if (process_fork(parent) == 0) {
    entry();
  }
  
  wait_until_wakeup(&parent->wait_chld);
}

void ls(char **argv) {
  if (!vfs_ls(argv[0])) {
    kprintf("\ndirectory not found");
  }
}

void signal(char **argv) {
  if (argv[0] && argv[1] && thread_signal(atoi(argv[0]), atoi(argv[1]))) {
    kprintf("Signal %s: sent to thread: %s", argv[1], argv[0]);
  } else {
    kprintf("Unable to find thread with id %s", argv[0]);
  }
}

void makekdir(char **argv) {
  if (vfs_mkdir(argv[0], 0) < 0) {
    kprintf("\ncannot create directory");
  }
}

void touch(char **argv) {
  int fd = 0;
  if ((fd = vfs_open(argv[0], O_CREAT, S_IFREG)) < 0) {
    kprintf("\ndirectory not found");
    return;
  }
  vfs_close(fd);
}

void ps(char **argv) {
  lock_scheduler();
    
  struct thread* th = NULL;
  kprintf("\nthreads ready: [ ");
  list_for_each_entry(th, get_ready_threads(), sched_sibling) {
    kprintf("%d ", th->tid);
  }
  kprintf("]");
  kprintf("\nthreads waiting: [ ");
  list_for_each_entry(th, get_waiting_threads(), sched_sibling) {
    kprintf("%d ", th->tid);
  }
  kprintf("]");
  

  kprintf("\nprocesses:");
  struct process* proc = NULL;
  struct list_head *ls = get_proc_list();
  
  kprintf("\n");
  kprintformat("PID", 5, BLU);
  kprintformat("NAME", 10, NULL);
  kprintformat("GID", 5, NULL);
  kprintformat("SID", 5, NULL);
  kprintformat("PARENT", 10, NULL);
  kprintformat("THREADS", 10, NULL);
  
  list_for_each_entry(proc, ls, sibling) {
    kprintf("\n");
    kprintformat("%d", 5, BLU, proc->pid);
    kprintformat("%s", 10, NULL, proc->name);
    kprintformat("%d", 5, NULL, proc->gid);
    kprintformat("%d", 5, NULL, proc->sid);
    kprintformat("%d", 10, CYN, proc->parent? proc->parent->pid : -1);
    
    if ((proc->state & (EXIT_ZOMBIE)) == 0) {
      kprintf("[");
      list_for_each_entry(th, &proc->threads, child) {
        kprintf(" %d", th->tid);
      }
      kprintf(" ]" COLOR_RESET);
    } else {
      kprintformat("%s", 10, RED, "zombie");
    }
  }
  unlock_scheduler();

}

void write(char **argv) {
  int32_t fd = vfs_open(argv[0], O_CREAT | O_APPEND, S_IFREG);
  if (fd < 0) {
    kprintf("\nunable to open file");
  }

  // vfs_flseek(fd, 11, SEEK_SET);
  if (vfs_fwrite(fd, argv[1], strlen(argv[1])) < 0) {
    kprintf("\nnot a file");
  } else {
    kprintf("\nfile updated");
  }
  vfs_close(fd);
}

void rm(char **argv) {
  if (vfs_unlink(argv[0], 0) < 0) {
    kprintf("\nfile or directory not found");
  }
}

void cd(char **argv) {
  int ret = vfs_cd(argv[0]);
  if (ret < 0) {
    kprintf(ret == -ENOTDIR ? "\nnot a directory" : "\ndirectory not found");
  }

  struct process *cur = get_current_process();
  cur->parent->fs->d_root = cur->fs->d_root;
  cur->parent->fs->mnt_root = cur->fs->mnt_root;
}

void info(char **argv) {
  if (strcmp(argv[0], "layout") == 0) {
    kprintf("Kernel start: %X\n", KERNEL_START);
    kprintf("Text start: %X\n", KERNEL_TEXT_START);
    kprintf("Text end: %X\n", KERNEL_TEXT_END);
    kprintf("Data start: %X\n", KERNEL_DATA_START);
    kprintf("Data end: %X\n", KERNEL_DATA_END);
    kprintf("Stack bottom %X\n", STACK_BOTTOM);
    kprintf("Stack top: %X\n", STACK_TOP);
    kprintf("Kernel end: %X\n", KERNEL_END);
  } else if (strcmp(argv[0], "memory") == 0) { 
    PMM_DEBUG();
  } else {
    kprintf("Invalid param: %s", argv[0]);
  }
  
}

void cat(char **argv) {
  char *filepath = argv[0];

  int32_t fd = vfs_open(filepath, O_RDONLY, 0);

  if (fd == -ENOENT || fd < 0) {
    kprintf("\nfile not found");
    return;
  }

  uint32_t size = 5;
  uint8_t* buf = kcalloc(size + 1, sizeof(uint8_t));
  kprintf("\n_____________________________________________\n");
  while (vfs_fread(fd, buf, size) == size) {
    buf[size] = '\0';
    kprintf(buf);
  };
  kprintf("\n____________________________________________\n");

  vfs_close(fd);
}
 
void hello(char **argv) {
  while (1) {
    kprintf("\nHello %s", argv[0]);
    thread_sleep(2000);
  }
}

void print_time(char **argv) {
  struct time* t = get_time(0);  // current time
  kprintf("\nCurrent time (UTC): %d:%d:%d, %d.%d.%d", t->hour, t->minute, t->second, t->day, t->month, t->year);
}

void pipe_ask() {

}

void pipe_answer() {

}

int clear(char **argv) {
  terminal_clrscr();
}

int ask(char **argv) {
  int size = 20;
  char buf[size];
  kprintf("To print: ");
  vfs_fread(1, &buf, size);
  vfs_fwrite(0, &buf, size);
  return 0;
}

int answer(char **argv) {
  int size = 20;
  char buf[size];
  vfs_fread(1, &buf, 20);
  vfs_fwrite(1, buf, size);
  return 0;
}

int exec(void *entry(char**), char* name, char **argv, int gid) {
  // TODO: FORBID 0 DEREFERENCE!
  struct process *parent = get_current_process();
  int pid = 0;
  
  if ((pid = process_fork(parent)) == 0) {
    struct process *cur_proc = get_current_process();
    cur_proc->name = name;
    parent->tty->pgrp = cur_proc->gid;
    setpgid(0, gid == -1? 0 : gid);
    
    entry(argv);
    do_exit(0);
  }
  setpgid(pid, gid == -1? pid : gid);
  parent->tty->pgrp = pid; // set foreground process

  return pid;
}

void test_pipe() {
  
}

void cmd_read_file();

#define CMD_MAX 5
#define PARAM_MAX 10

struct cmd_t {
  char *argv[PARAM_MAX];
  char *cmd;
};

int search_and_run(struct cmd_t *com, int gid) {
  int ret = -1;
  if (strcmp(com->cmd, "hello") == 0) {
    ret = exec(hello, "hello", com->argv, gid);
  } else if (strcmp(com->cmd, "create") == 0) {
    kprintf("\nnew struct thread: ");
    execve(kthread_fork);
  } if (strcmp(com->cmd, "play") == 0) {
    init_consumer_producer();
    create_system_process(&producer, "producer");
    create_system_process(&consumer, "consumer");
  } else if (strcmp(com->cmd, "ps") == 0) {
    ret = exec(ps, "ps", com->argv, gid); 
  } if (strcmp(com->cmd, "signal") == 0) {
    ret = exec(signal, "signal", com->argv, gid);
  } else if (strcmp(com->cmd, "time") == 0) {
    ret = exec(print_time, "print_time", com->argv, gid);
  } else if (strcmp(com->cmd, "info") == 0) {
    ret = exec(info, "info", com->argv, gid);
  } else if (strcmp(com->cmd, "clear") == 0) {
    ret = exec(clear, "clear", com->argv, gid);
  } else if (strcmp(com->cmd, "cd") == 0) {
    ret = exec(cd, "cd", com->argv, gid);
  } else if (strcmp(com->cmd, "ask") == 0) {
    ret = exec(ask, "ask", com->argv, gid);
  } else if (strcmp(com->cmd, "answer") == 0) {
    ret = exec(answer, "answer", com->argv, gid);
  } else if (strcmp(com->cmd, "touch") == 0) {
    ret = exec(touch, "touch", com->argv, gid);
  } else if (strcmp(com->cmd, "write") == 0) {
    ret = exec(write, "write", com->argv, gid);
  } else if (strcmp(com->cmd, "rm") == 0) {
    ret = exec(rm, "rm", com->argv, gid);
  } else if (strcmp(com->cmd, "mkdir") == 0) {
    ret = exec(makekdir, "mkdir", com->argv, gid);
  } else if (strcmp(com->cmd, "cat") == 0) {
    ret = exec(cat, "cat", com->argv, gid);
  } else if (strcmp(com->cmd, "dcat") == 0) {
    cmd_read_kybrd();
  } else if (strcmp(com->cmd, "ls") == 0) {
    ret = exec(ls, "ls", com->argv, gid);
  } else if (strcmp(com->cmd, "run") == 0) {
    char filepath[100];
    kprintf("path: ");
    get_cmd(filepath, 100);
    struct process* cur_proc = get_current_process();
    create_elf_process(cur_proc, filepath);
  } else if (strcmp(com->cmd, "") == 0) {
    goto clean;
  } else {
    kprintf("\ncommand not found: %s\n", com->cmd);
    goto clean;
  }
clean:
  return ret;
}


static struct cmd_t commands[CMD_MAX];

//! our simple command parser
bool interpret_line(char* line) {
  int param_size = PARAM_MAX;
  for (int i = 0; i < CMD_MAX; ++i) {
    commands[i].cmd = NULL;
    for (int j = 0; j < param_size; ++j) {
      commands[i].argv[j] = NULL;
    }
  }

  // Returns first token
  char* token = strtok(line, " ");
  
  int param_i = 0;
  int command_i = -1;
  bool parse_command = true;

  while (token != NULL) {
    if (parse_command) {
      command_i++;
      commands[command_i].cmd = strdup(token);
      log("command: %s", token);
      param_i = 0;
      parse_command = false;
      continue;
    }

    token = strtok(NULL, " ");

    if (
      (strlen(token) == 1 && strcmp(token, "|") == 0) ||
      (strlen(token) == 2 && strcmp(token, "&&") == 0) || 
      (strlen(token) == 2 && strcmp(token, "||") == 0)
    ) {
      command_i++;
      commands[command_i].cmd = strdup(token);
      parse_command = true;
      token = strtok(NULL, " ");
      continue;
    } else if (token == NULL) {
      break;
    } else {
      assert(param_i < param_size, "too many params");
      commands[command_i].argv[param_i] = strdup(token);
      log("param[%d]: %s", param_i, commands[command_i].argv[param_i]);
      ++param_i;
    }
  }

  command_i++;
  if (command_i == 0)
    goto clean;

  struct infop inop;
  int gid = -1;
  log("start command");
  int pid = -1;
  struct process *shell_proc = get_current_process();
  
  for (int i = 0; i < command_i; ++i) {
    struct cmd_t *com = &commands[i];

    if (strcmp(com->cmd, "|") == 0) {
      //int fd[2];
      //do_pipe(&fd);
      
      continue;
    } else if (strcmp(com->cmd, "&&") == 0) {
      do_wait(P_PID, pid, &inop, WEXITED | WSSTOPPED);
      
      continue;
    }

    pid = search_and_run(com, gid);
    
    if (gid == -1) {
      gid = pid;
    }

    log("run gid: %d", gid);
    if (gid == -1) {
      goto clean;
    }

  }
  log("end command");

 
  
  int ret = 0;

  // wait all childs
  while ((ret = do_wait(P_PGID, gid, &inop, WEXITED | WSSTOPPED)) > 0) {}
  shell_proc->tty->pgrp = shell_proc->gid;
  kprintf("\n");
clean:
  for (int i = 0; i < CMD_MAX; ++i) {
    struct cmd_t com = commands[i];
    if (com.cmd) {
      for (int j = 0; j < param_size; ++j) {
        if (com.argv[j] != NULL) {
          kfree(com.argv[j]);
          com.argv[j] = NULL;
        }
      }
      kfree(com.cmd);
      com.cmd = NULL;
    }
    
  }
  return false;
}

void cmd_read_kybrd() {
  char filepath[100];

  kprintf("\npath: ");
  get_cmd(filepath, 100);

  int32_t fd = vfs_open(filepath, O_RDONLY, 0);

  if (fd == -ENOENT || fd < 0) {
    kprintf("\nfile not found");
    return;
  }

  uint32_t size = sizeof(struct key_event);
  uint8_t* buf = kcalloc(1, size);
  kprintf("\n_____________________________________________\n");
  while (vfs_fread(fd, buf, size) >= 0) {
    struct key_event *ev = buf;
    char c = kkybrd_key_to_ascii(ev->key);
    kprintf("%c", c);
  };
  kprintf("\n____________________________________________");
  kfree(buf);
  vfs_close(fd);
}

//! read sector command
void cmd_read_sect() {
  uint32_t sectornum = 0;
  char sectornumbuf[4];
  uint8_t* sector = 0;

  kprintf("\nPlease type in the sector number [0 is default] \n");
  get_cmd(sectornumbuf, 3);
  sectornum = atoi(sectornumbuf);

  kprintf("\nSector %d contents:\n\n", sectornum);

  //! read sector from disk
  sector = flpydsk_read_sector(sectornum);

  //! display sector
  if (sector != 0) {
    int i = 0;
    for (int c = 0; c < 4; c++) {
      for (int j = 0; j < 128; j++)
        kprintf("%x ", sector[i + j]);
      i += 128;

      kprintf("\nPress any key to continue\n");
      //getch();
    }
  } else
    kprintf("\n*** Error reading sector from disk\n");

  kprintf("\nDone.");
}

GREATEST_MAIN_DEFS();

void kybrd_manager() {
  int kybrd_fd = 0;
  if ((kybrd_fd = vfs_open("/dev/input/kybrd", O_RDONLY)) < 0) {
    err("Cannot open keyboard");
  }

  struct key_event ev;

  while (true) {
    vfs_fread(kybrd_fd, &ev, sizeof(ev));

    //log("%c", ev.key);
    if (ev.type == KEY_RELEASE) {
      continue;
    }
    
    char c = kkybrd_key_to_ascii(ev.key);
    //log("%c", c);
    
    if (pts_driver) {
      struct tty_struct *pts = list_first_entry(&pts_driver->ttys, struct tty_struct, sibling);
      if (pts != NULL) {
        pts->ldisc->receive_buf(pts, &c, 1);
      }
    } 
  }
}

void shell_start() {
  int size = N_TTY_BUF_SIZE - 1;
  char *line = kcalloc(size, sizeof(char));
  struct process* proc = get_current_process();
  
  while (true) {
newline:
    kprintf("(%s)root@%s: ", 
      strcmp(proc->fs->mnt_root->mnt_devname, "/dev/hda") == 0 ? "ext2" : proc->fs->mnt_root->mnt_devname,
      proc->fs->d_root->d_name
    );
    

    kreadline(line, size);
    interpret_line(line);
  }
  kfree(line);
}

void init_process() {
  struct process *parent = get_current_process();
  
  if (process_fork(parent) == 0) {
    get_current_process()->name = strdup("garbage");
    garbage_worker_task();   // 1
  }

  if (process_fork(parent) == 0) {
    get_current_process()->name = strdup("idle");
    idle_task(); // 2
  }

  vfs_init(&ext2_fs_type, "/dev/hda");
  chrdev_init();

  kkybrd_install(IRQ1);
  
  tty_init();

  if (process_fork(parent) == 0) {
    get_current_process()->name = strdup("kybrd");
    kybrd_manager(); // 3
  }
  
  pid_t id = 0;

  // NOTE: calling setpgid twice seems redudant but it eluminates race conditions
  // caused by not knowing whether the parent or the child is selected by the scheduler
  if ((id = process_fork(parent)) == 0) {
    setpgid(0, 0);
    get_current_process()->sid = get_next_sid();
    get_current_process()->name = strdup("term");
    terminal_run();
  }
  setpgid(id, id); 
  
  // wait until is waken up (should not be waken up)
  wait_until_wakeup(&parent->wait_chld);
  assert_not_reached(); // no, please no!
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
  
  log("Kernel size: %dKB", (int)(KERNEL_END - KERNEL_START) / 1024);
  log("Ends in 0x%x (DIR: %d, INDEX: %d)", KERNEL_END, PAGE_DIRECTORY_INDEX(KERNEL_END), PAGE_TABLE_INDEX(KERNEL_END));

  pmm_init(mbd);
  vmm_init();

  exception_init();
  hal_initialize();
  
  pata_init();
  syscall_init();

  timer_init();
  initialise_multitasking(&init_process);

  return 0;
}
