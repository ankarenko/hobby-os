#include <stdint.h>
#include <malloc.h>
#include <signal.h>

#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <kernel/util/ansi_codes.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

#include "../../../ports/newlib/myos/msyscalls.h"

#define WNOHANG 0x00000001
#define WUNTRACED 0x00000002
#define WSTOPPED WUNTRACED
#define WEXITED 0x00000004
#define WCONTINUED 0x00000008
#define WNOWAIT 0x01000000 /* Don't reap, just poll status.  */

//#include "../../../ports/newlib/myos/print.h"

FILE* stream;

void signal_handler_SIGHUP(int signum) {
  printf("\nSIGHUP: %d", signum);
} 

void custom_signal_handler_SIGTRAP(int signum) {
  fputs("signal SIGTRAP handler: block SIGALRMP! \n!", stream);
  sigset_t msk = 1 << (SIGALRM - 1);
  sigprocmask(SIG_SETMASK, &msk, NULL); // block SIGALRMP interrupts

  fputs("signal SIGTRAP handler: hello! \n!", stream);
  fputs("signal SIGTRAP handler: sleep 10 sec! \n!", stream);
  sleep(10);
  fputs("signal SIGTRAP handler: woke up \n!", stream);
  fflush(stream);
}

void custom_signal_handler_SIGALRMP(int signum) {
  fputs("signal SIGALRM handler: hello! \n!", stream);
  fputs("signal SIGALRM handler: sleep 10 sec! \n!", stream);
  sleep(10);
  fputs("signal SIGALRM handler: woke up \n!", stream);
  fflush(stream);
}

#define CMD_MAX         5
#define PARAM_MAX       10
#define N_TTY_BUF_SIZE  (256)

#define CMD_MAX 5
#define PARAM_MAX 10

struct cmd_t {
  char *argv[PARAM_MAX];
  char *cmd;
  int argc;
};

int cat(char **argv) {
  
  char *filepath = argv[0];

  int32_t fd = open(filepath, O_RDONLY, 0);

  if (fd < 0) {
    printf("\nfile not found");
    return;
  }

  uint32_t size = 5;
  uint8_t *buf = calloc(size + 1, sizeof(uint8_t));
  printf("\n_____________________________________________");
  printf("\n");
  while (read(fd, buf, size) == size) {
    buf[size] = '\0';
    printf(buf);
  };
  printf("\n____________________________________________\n");

  close(fd);
}

void echo(char **argv) {
  int32_t fd = open(argv[0], O_CREAT | O_APPEND, S_IFREG);
  if (fd < 0)
    printf("\nunable to open file");

  // vfs_flseek(fd, 11, SEEK_SET);
  if (write(fd, argv[1], strlen(argv[1])) < 0)
    printf("\nnot a file");
  else
    printf("\nfile updated");
  
  close(fd);
}

void rm(char **argv) {
  if (unlink(argv[0]) < 0) {
    printf("\nfile or directory not found");
  }
}

int ls(char **argv) {
  DIR* dirp;
  bool has_arg = argv[0] != NULL;
  int size = 128;
  char path[size];
  memset(&path, 0, size);
  sprintf(&path, "%s/", !has_arg? "." : argv[0]);
  int base_path_len = strlen(path);
  char *base_path = &path[base_path_len];

  if ((dirp = opendir(path)) == NULL)
    return -1; 
    
  int maxlen = 16;
  char name[maxlen + 1];
  struct dirent* p_dirent = NULL;
  struct stat st;
  char res[32];
  struct tm lt;
  
  while ((p_dirent = readdir(dirp)) != NULL) {
    memset(&name, ' ', maxlen);
    memcpy(&name, p_dirent->d_name, strlen(p_dirent->d_name));
    name[maxlen] = '\0';
    
    sprintf(base_path, "%s\0", p_dirent->d_name);
    //printf(path);

    if (stat(path, &st) != 0) {
      printf(RED"\n[ERROR]:"COLOR_RESET "unable to fetch stats for teh file %s", p_dirent->d_name);
      goto error;
    }

    if (S_ISDIR(st.st_mode)) {
      printf("\n%s", name);
    } else if (S_ISCHR(st.st_mode)) {
      printf(BLU"\n%s", name);
      localtime_r(&st.st_ctime, &lt);
      strftime(&res, sizeof(res), "%H:%M %b %d", &lt);
      printf(res);
      printf(COLOR_RESET);

    } else if (S_ISREG(st.st_mode)) {
      printf(RED"\n%s", name);
      localtime_r(&st.st_ctime, &lt);
      strftime(&res, sizeof(res), "%H:%M %b %d", &lt);
      printf(res);
      printf("   %u bytes", st.st_size);
      printf(COLOR_RESET);
    }
  }

  if (closedir(dirp) < 0)
    goto error;

  _exit(0);
error:
  _exit(-1); 
}

int exec_program(char* path, char **argv, int foreground_gid) {
  printf("\n");
  int pid = 0;
  if ((pid = fork()) == 0) {
    setpgid(0, foreground_gid == -1 ? 0 : foreground_gid);

    int foreground_pgrp = tcgetpgrp(STDIN_FILENO);
    int tmp = 0;
    
    if ((tmp = getgid()) < 0) {
      printf("\n unable get gid");
    }

    if (foreground_pgrp != tmp && tcsetpgrp(STDIN_FILENO, tmp) < 0) {
      printf("\nunable to set foreground process");
    } else {
      //printf("\nset foreground to %d", tmp);
    }

    execve(path, argv, NULL);
    _exit(0);
  }
  int id = foreground_gid == -1 ? pid : foreground_gid;
  setpgid(pid, id);
  
  return pid;
}

int exec(void *entry(char **), char *name, char **argv, int foreground_gid) {
  int pid = 0;
  if ((pid = fork()) == 0) {
    setpgid(0, foreground_gid == -1 ? 0 : foreground_gid);

    int foreground_pgrp = tcgetpgrp(STDIN_FILENO);

    //printf("\n current foreground pgrp: %d", foreground_pgrp);

    int tmp = 0;
    
    if ((tmp = getgid()) < 0) {
      printf("\n unable get gid");
    }

    //printf("\n current gid: %d", tmp);

    if (foreground_pgrp != tmp && tcsetpgrp(STDIN_FILENO, tmp) < 0) {
      printf("\nunable to set foreground process");
    } else {
      //printf("\nset foreground to %d", tmp);
    }

    entry(argv);
    _exit(0);
  }
  int id = foreground_gid == -1 ? pid : foreground_gid;
  setpgid(pid, id);
  
  return pid;
}

void hello(char **argv) {
  
  while (true) {
    int foreground_pgrp = tcgetpgrp(STDIN_FILENO);

    printf("\nHello %s [foreground = %d]", argv[0] == NULL? "world" : argv[0], foreground_pgrp);
    dbg_ps();
    
    sleep(6);
  } 
}

void ps() {
  dbg_ps();
}

int search_and_run(struct cmd_t *com, int foreground_gid) {
  int ret = -1;
  if (strcmp(com->cmd, "ls") == 0) {
    ret = exec(ls, "ls", com->argv, foreground_gid);
  } else if (strcmp(com->cmd, "hello") == 0) {
    ret = exec(hello, "hello", com->argv, foreground_gid);
  } else if (strcmp(com->cmd, "cat") == 0) {
    ret = exec(cat, "cat", com->argv, foreground_gid);
  } else if (strcmp(com->cmd, "ps") == 0) {
    ret = exec(ps, "ps", com->argv, foreground_gid);
  } else if (strcmp(com->cmd, "echo") == 0) {
    ret = exec(echo, "echo", com->argv, foreground_gid);
  } else if (strcmp(com->cmd, "rm") == 0) {
    ret = exec(rm, "rm", com->argv, foreground_gid);
  } else if (strcmp(com->cmd, "clear") == 0) {
    putchar(12);
  } else if (strcmp(com->cmd, "cd") == 0) {
    chdir(com->argv[0] == NULL? "." : com->argv[0]);
    //ret = exec(cd, "cd", com->argv, gid);
  } else if (strcmp(com->cmd, "run") == 0) {
    ret = exec_program(com->argv[0], com->argv, foreground_gid);
  } else {
    struct stat st;
    if (stat(com->cmd, &st) == 0) {
      ret = exec_program(com->cmd, com->argv, foreground_gid);
    } else {
      printf("\nunknown program: %s", com->cmd);
    }
  }
clean:
  return ret;
}

bool interpret_line(char *line) {
  struct cmd_t commands[CMD_MAX];
  int param_size = PARAM_MAX;
  memset(commands, 0, sizeof(commands));

  // Returns first token
  char *token = strtok(line, " ");

  int param_i = 0;
  int command_i = -1;
  bool parse_command = true;

  while (token != NULL) {
    if (parse_command) {
      command_i++;
      commands[command_i].cmd = strdup(token);
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
      (strlen(token) == 2 && strcmp(token, "||") == 0)
    ) {
      command_i++;
      commands[command_i].cmd = strdup(token);
      parse_command = true;
      token = strtok(NULL, " ");
      continue;
    } else {
      commands[command_i].argv[param_i] = strdup(token);
      ++param_i;
    }
  }

  commands[command_i].argc = param_i;

  command_i++;
  if (command_i == 0)
    goto clean;

  //struct infop inop;
  int foreground_gid = -1;
  int pid = -1;
  //struct process *shell_proc = get_current_process();

  for (int i = 0; i < command_i; ++i) {
    struct cmd_t *com = &commands[i];

    if (strcmp(com->cmd, "|") == 0) {
      continue;
    } else if (strcmp(com->cmd, "&&") == 0) {
      //do_wait(P_PID, pid, &inop, WEXITED | WSSTOPPED);

      continue;
    } else if (strcmp(com->cmd, "pipe") == 0) {
      //test_pipe();
      continue;
    }

    pid = search_and_run(com, foreground_gid);

    if (foreground_gid == -1)
      foreground_gid = pid;

    if (foreground_gid == -1)
      goto clean;
  }

  int status;
  int ret = -1;
  while ((ret = waitpid(-pid, &status, WEXITED | WSTOPPED)) > 0) {
    //printf("\nwaited for child, exiting");
  }

  int shell_gid = getgid();
  if (shell_gid <= 0) {
    printf("\nInvalid gid");
  }
  
  if (tcsetpgrp(STDIN_FILENO, shell_gid) < 0) {
    printf("\nUnable to return foreground process to shell");
  } else {
    //printf("\n Return foreground to gid: %d", shell_gid);
  }


  
  /*
  shell_proc->tty->pgrp = shell_proc->gid;
  */
  printf("\n");
clean:
  for (int i = 0; i < CMD_MAX; ++i) {
    struct cmd_t com = commands[i];
    if (com.cmd) {
      for (int j = 0; j < param_size; ++j) {
        if (com.argv[j] != NULL) {
          free(com.argv[j]);
          com.argv[j] = NULL;
        }
      }
      free(com.cmd);
      com.cmd = NULL;
    }
  }
  return false;
}

void main(int argc, char** argv) {
  struct sigaction new_action, old_action;

  new_action.sa_handler = signal_handler_SIGHUP;
  sigemptyset(&new_action.sa_mask);
  new_action.sa_flags = 0;
  
  sigaction(SIGHUP, NULL, &old_action);
  if (old_action.sa_handler != SIG_IGN) {
    sigaction(SIGHUP, &new_action, NULL);
  }
  
  /*
  int c;
  char* argv_t[] = { "shell", "--" };

  printf("\nhello");
  //return 0;
  printf("\nargu,ents: %d", argc);

  for (int i = 0; i < argc; ++i) {
    printf("%x", argv_t[i]);
    printf("v[%d]=%s", i, argv_t[i]);
    printf("v[%d]=%s", i, argv[i]);
  }

  
  while ((c = getopt(argc, argv_t, "abc")) != -1) {
    printf("bro");
    //printf("\nc = %d", c);
  }

  printf("\nend");
  */
  //return 0;

  int size = N_TTY_BUF_SIZE - 1;
  char *line = calloc(size, sizeof(char));
  char path[20];
  bool start = true;
  while (true) {
    getcwd(&path, 20);
    if (start) {
      printf("                                   welcome to");
      interpret_line("run bin/figlet -c -k -- simple shell");
      start = false;
      continue;
    }
    printf("\nroot@%s: ", path);
    //dprintf(2, "\nget liune");
    gets(line);
    dprintf(2, "line: %s", line);
    
    interpret_line(line);
  }
  free(line);
  return 0;
}
  

 
  
  /*
  return ls(argc, argv);
  */
  
  /*
  setpgid(0, 0);
  
  int size = N_TTY_BUF_SIZE - 1;
  char *line = malloc(size * sizeof(char));

  char path[20];
  size_t  n = 256;

  while (true) {
newline:
    getcwd(&path, 20);
    printf("\n(%s)root@%s: ", "ext2", path);

    sleep(3);

    gets(line);
    
  }
clean:
  free(line);
  return 0;
  /*
  int pid = 0; 
  char *param1 = "final";
  char *_argv[] = { param1, 0 };
  if (argc < 2 && (pid = fork()) == 0) {
    setpgid(0, 0);
    execve("calc.exe", _argv, NULL);
  }
  setpgid(pid, pid);
  
  while (1) {
    printf("\nHello %s", argc > 1? argv[1] : " parent");
    sleep(3);
  }
  /*
  while (1) {
    fprintf(stdout, "hello my man");
    sleep(3);
  }
  
  */

  /*
  stream = fopen("dev/tty0", "ab+");
  fputs("Hello user main! \n!", stream);

  struct sigaction new_action, old_action;

  new_action.sa_handler = custom_signal_handler_SIGTRAP;
  //sigemptyset(&new_action.sa_mask);
  //new_action.sa_flags = 0;
  
  sigaction(SIGTRAP, NULL, &old_action);
  if (old_action.sa_handler != SIG_IGN) {
    sigaction(SIGTRAP, &new_action, NULL);
  }

  new_action.sa_handler = custom_signal_handler_SIGALRMP;
  sigaction(SIGALRM, NULL, &old_action);
  if (old_action.sa_handler != SIG_IGN) {
    sigaction(SIGALRM, &new_action, NULL);
  }
  */



  
  /*
  FILE* stream = fopen("dev/tty0", "ab+");
  
  if (fputs("Hello, world\n!", stream) == 0) {
    fflush(stream);
    int32_t size = 100;
    char* buf = calloc(size + 1, sizeof(char));
    memset(buf, 0, size);
    
    if (fseek(stream, 0, SEEK_SET) == 0) {
      print("\n_______________________________\n");
      while (fgets(buf, size, stream) != NULL) {
        print("%s", buf);
      }
      print("\n________________________________\n");
    }
    fclose(stream);
    free(buf);
  }
  */


  
  
  /*
  int32_t id = -1;
  
  if (argc == 2) {
    id = (int32_t)*argv[1];

    print("File: %s\n", argv[0]);
    print("struct processid: %d\n", id);
  }
  */
  
  //FILE* stream = fopen("test.txt", "a");
  /*
  int32_t size = 100;
  
  if (stream == NULL) {
    //print("Not stream");
    _exit(0);
  }

  fputs("Hello, world\n!", stream);

  fflush(stream); // write to file

  char* buf = calloc(size + 1, sizeof(char));
  lseek(stream->_file, 0, SEEK_SET);
  
  // Read the content and print it
  while(fgets(buf, size, stream)) {
    buf[size] = '\0';
    //print("%s", buf);
  }

  free(buf);
  fclose(stream);

  /*
  while (1) {
    sleep(3);
    print("\nHello from userprocess: %d", id);
  }
  */

  //_exit(0);
  // while (1) {};
//}
