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
#include "kernel/fs/fat32/fat32.h"
#include "kernel/fs/vfs.h"
#include "kernel/locking/semaphore.h"
#include "kernel/memory/kernel_info.h"
#include "kernel/memory/malloc.h"
#include "kernel/memory/pmm.h"
#include "kernel/memory/vmm.h"
#include "kernel/proc/elf.h"
#include "kernel/proc/task.h"
#include "kernel/system/sysapi.h"
#include "kernel/system/time.h"
#include "kernel/system/timer.h"
#include "kernel/include/ctype.h"
#include "kernel/util/debug.h"
#include "kernel/include/errno.h"
#include "kernel/include/fcntl.h"
#include "kernel/include/ioctls.h"
#include "kernel/include/list.h"
#include "kernel/util/math.h"
#include "kernel/util/vsprintf.h"
#include "multiboot.h"
#include "test/greatest.h"

extern struct vfs_file_system_type ext2_fs_type;

void cmd_init();
void idle_task();

static struct vfs_file *_cur_dir = NULL;

//! gets next command
void get_cmd(char *buf, int n) {
  vfs_fread(1, buf, n);
  buf[n] = '\0';
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

  if (process_spawn(current_process) == 0) {
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

  struct thread *th = NULL;
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
  struct process *proc = NULL;
  struct list_head *ls = get_proc_list();

  kprintf("\n");
  kprintformat("PID", 5, BLU);
  kprintformat("NAME", 10, NULL);
  kprintformat("GID", 5, NULL);
  kprintformat("SID", 5, NULL);
  kprintformat("PARENT", 8, NULL);
  kprintformat("USER", 7, NULL);
  kprintformat("THREADS", 10, NULL);


  list_for_each_entry(proc, ls, sibling) {
    kprintf("\n");
    kprintformat("%d", 5, BLU, proc->pid);
    kprintformat("%s", 10, NULL, proc->name);
    kprintformat("%d", 5, NULL, proc->gid);
    kprintformat("%d", 5, NULL, proc->sid);
    kprintformat("%d", 8, CYN, proc->parent ? proc->parent->pid : -1);
    kprintformat("%c", 7, CYN, vmm_is_kernel_directory(proc->va_dir)? '-' : '+');

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
  uint8_t *buf = kcalloc(size + 1, sizeof(uint8_t));
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
  struct time *t = get_time(0);  // current time
  kprintf("\nCurrent time (UTC): %d:%d:%d, %d.%d.%d", t->hour, t->minute, t->second, t->day, t->month, t->year);
}

int clear(char **argv) {
  terminal_clrscr();
}

int ask(char **argv) {
  int i = 0;
  int size = 64;
  char buf[size];
  char question[size];
  struct process *cur_proc = get_current_process();

  while (true) {
    snprintf(&buf, size, "\np(%d) %d: question: ", cur_proc->pid, ++i);
    vfs_fwrite(stderr, buf, strlen(buf));
    int read = vfs_fread(stdin, &question, size);
    question[read] = '\0';
    vfs_fwrite(stdout, &question, read);
    thread_sleep(1000);
  }
  return 0;
}

int run_program(char **argv) {
  process_execve(argv[0], &argv[1], NULL);
  assert_not_reached("run program: ...");
}

int answer(char **argv) {
  int i = 0;
  int size = 64;
  char buf[size];
  char answ[size];
  struct process *cur_proc = get_current_process();

  while (true) {
    int read = vfs_fread(stdin, &answ, size);
    answ[read] = '\0';
    snprintf(&buf, size, "\np(%d) %d: answer: %s", cur_proc->pid, ++i, answ);
    vfs_fwrite(stdout, buf, strlen(buf));
  }
  return 0;
}

void make_foreground() {
  struct process *current_process = get_current_process();
  struct vfs_file *file_in = current_process->files->fd[stdin];
  struct vfs_file *file_out = current_process->files->fd[stdout];

  if (
      file_in->f_op->ioctl(NULL, file_in, TIOCSPGRP, 0) < 0 &&
      file_out->f_op->ioctl(NULL, file_out, TIOCSPGRP, 0) < 0) {
    assert_not_reached();
  }
}

int run_userprogram(char **argv) {
  process_load(argv[0], argv);
  return 0;
}

int exec(void *entry(char **), char *name, char **argv, int gid) {
  // TODO: FORBID 0 DEREFERENCE!
  struct process *parent = get_current_process();
  int pid = 0;

  if ((pid = process_spawn(parent)) == 0) {
    setpgid(0, gid == -1 ? 0 : gid);

    struct process *cur_proc = get_current_process();
    struct vfs_file *file = cur_proc->files->fd[stdin];
    assert(file == cur_proc->files->fd[stdout]);
    cur_proc->name = strdup(name);

    pid_t gid = -1;
    file->f_op->ioctl(file->f_dentry->d_inode, file, TIOCGPGRP, &gid);

    if (gid != cur_proc->gid && file->f_op->ioctl(file->f_dentry->d_inode, file, TIOCSPGRP, &cur_proc->gid) < 0) {
      assert_not_reached("unable to set foreground process");
    }

    entry(argv);
    do_exit(0);
  }
  int id = gid == -1 ? pid : gid;
  setpgid(pid, id);
  
  return pid;
}

void cmd_read_file();

#define CMD_MAX 5
#define PARAM_MAX 32

struct cmd_t {
  char *argv[PARAM_MAX];
  char *cmd;
};

void test_pipe() {
  struct infop inop;
  struct process *shell_proc = get_current_process();

  int pipe_fd[2];

  if (do_pipe(&pipe_fd) != 0) {
    assert_not_reached();
  }

  int pid = 0;
  int gid = 0;

  if ((pid = process_spawn(shell_proc)) == 0) {
    struct process *cur_proc = get_current_process();
    cur_proc->name = strdup("ask");
    setpgid(0, 0);
    shell_proc->tty->pgrp = cur_proc->gid;
    int fd_out = dup(stdout);
    dup2(pipe_fd[1], stdout);
    dup2(fd_out, stderr);

    vfs_close(pipe_fd[0]);
    vfs_close(fd_out);

    ask(NULL);
    do_exit(0);
  }
  gid = pid;
  setpgid(pid, gid);

  if ((pid = process_spawn(shell_proc)) == 0) {
    setpgid(0, gid);
    struct process *cur_proc = get_current_process();
    cur_proc->name = strdup("answer");
    dup2(pipe_fd[0], stdin);
    vfs_close(pipe_fd[1]);
    answer(NULL);
    do_exit(0);
  }
  setpgid(pid, gid);
  shell_proc->tty->pgrp = gid;  // set foreground process group

  // wait all childs
  int ret = -1;
  while ((ret = do_wait(P_PGID, gid, &inop, WEXITED | WSSTOPPED)) > 0) {
  }
  shell_proc->tty->pgrp = shell_proc->gid;
}

int search_and_run(struct cmd_t *com, int gid) {
  int ret = -1;
  if (strcmp(com->cmd, "hello") == 0) {
    ret = exec(hello, "hello", com->argv, gid);
  } else if (strcmp(com->cmd, "ps") == 0) {
    ret = exec(ps, "ps", com->argv, gid);
  } else if (strcmp(com->cmd, "signal") == 0) {
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
  } else if (strcmp(com->cmd, "exec") == 0) {
    ret = exec(run_program, "program", com->argv, gid);
  } else if (strcmp(com->cmd, "run") == 0) {
    run_userprogram(&com->argv);
    ret = -1;
    //ret = exec(run_userprogram, com->argv[0], com->argv, gid);
  } else if (strcmp(com->cmd, "") == 0) {
    goto clean;
  } else {
    kprintf("\ncommand not found: %s\n", com->cmd);
    goto clean;
  }
clean:
  return ret;
}



//! our simple command parser
bool interpret_line(char *line) {
  struct cmd_t *commands = kcalloc(CMD_MAX, sizeof(struct cmd_t));
  
  int param_size = PARAM_MAX;
  for (int i = 0; i < CMD_MAX; ++i) {
    commands[i].cmd = NULL;
    for (int j = 0; j < param_size; ++j) {
      commands[i].argv[j] = NULL;
    }
  }

  // Returns first token
  char *token = strtok(line, " ");

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
    
    if (token == NULL) {
      break;
    } else if (
        (strlen(token) == 1 && strcmp(token, "|") == 0) ||
        (strlen(token) == 2 && strcmp(token, "&&") == 0) ||
        (strlen(token) == 2 && strcmp(token, "||") == 0)) {
      command_i++;
      commands[command_i].cmd = strdup(token);
      parse_command = true;
      token = strtok(NULL, " ");
      continue;
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
      continue;
    } else if (strcmp(com->cmd, "&&") == 0) {
      do_wait(P_PID, pid, &inop, WEXITED | WSSTOPPED);

      continue;
    } else if (strcmp(com->cmd, "pipe") == 0) {
      test_pipe();
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
  while ((ret = do_wait(P_PGID, gid, &inop, WEXITED | WSSTOPPED)) > 0) {
  }
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
  uint8_t *buf = kcalloc(1, size);
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
  uint8_t *sector = 0;

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
      // getch();
    }
  } else
    kprintf("\n*** Error reading sector from disk\n");

  kprintf("\nDone.");
}

GREATEST_MAIN_DEFS();

void shell_start() {
  int size = N_TTY_BUF_SIZE - 1;
  char *line = kcalloc(size, sizeof(char));
  struct process *proc = get_current_process();

  while (true) {
  newline:
    kprintf("\n(%s)root@%s: ",
            strcmp(proc->fs->mnt_root->mnt_devname, "/dev/hda") == 0 ? "ext2" : proc->fs->mnt_root->mnt_devname,
            proc->fs->d_root->d_name);

    kreadline(line, size);
    interpret_line(line);
  }
  kfree(line);
}

void init_process() {
  struct process *parent = get_current_process();

  /*
  if (process_spawn(parent) == 0) {
    get_current_process()->name = strdup("garbage");
    garbage_worker_task();  // 1
  }
  */

  if (process_spawn(parent) == 0) {
    get_current_process()->name = strdup("idle");
    idle_task();  // 2
  }

  vfs_init(&ext2_fs_type, "/dev/hda");
  chrdev_init();

  kkybrd_install(IRQ1);

  tty_init();
  pid_t id = 0;

  // NOTE: calling setpgid twice seems redudant but it eluminates race conditions
  // caused by not knowing whether the parent or the child is selected by the scheduler
  if ((id = process_spawn(parent)) == 0) {
    struct process *cur_proc = get_current_process();
    setpgid(0, 0);
    cur_proc->sid = cur_proc->pid;
    cur_proc->name = strdup("term");
    assert(cur_proc->sid == cur_proc->gid && cur_proc->gid == cur_proc->pid);
    terminal_run();
  }
  setpgid(id, id);

  // wait until is waken up (should not be waken up)
  wait_until_wakeup(&parent->wait_chld);
  assert_not_reached();  // no, please no!
}

void kernel_main(multiboot_info_t *mbd, uint32_t magic) {
  char **argv = 0;
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
