#include <stdint.h>
#include <malloc.h>
#include <signal.h>

#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <kernel/util/ansi_codes.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

#define WNOHANG 0x00000001
#define WUNTRACED 0x00000002
#define WSTOPPED WUNTRACED
#define WEXITED 0x00000004
#define WCONTINUED 0x00000008
#define WNOWAIT 0x01000000 /* Don't reap, just poll status.  */

//#include "../../../ports/newlib/myos/print.h"

FILE* stream;

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
};


void ls(int argc, char **argv) {
  DIR* dirp;
  
  printf("start");
  if (argc > 1) {
    printf("\nARG: %s", argv[1]);
  }

  int size = 128;
  char path[size];
  memset(&path, 0, size);
  sprintf(&path, "%s/", argc < 2? "" : argv[1]);
  int base_path_len = strlen(path);
  printf("\n%d", base_path_len);
  char *base_path = &path[base_path_len];
  
  printf("\nabsolute path to search: %s\n", path);
  
  printf("\n%s", base_path);
  

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


int exec(void *entry(char **), char *name, char **argv, int gid) {
  int pid = 0;
  if ((pid = fork()) == 0) {
    setpgid(0, gid == -1 ? 0 : gid);

    /*
    pid_t gid = -1;
    file->f_op->ioctl(file->f_dentry->d_inode, file, TIOCGPGRP, &gid);

    if (gid != cur_proc->gid && file->f_op->ioctl(file->f_dentry->d_inode, file, TIOCSPGRP, &cur_proc->gid) < 0) {
      assert_not_reached("unable to set foreground process");
    }
    */
    entry(argv);
    _exit(0);
  }
  int id = gid == -1 ? pid : gid;
  setpgid(pid, id);
  
  return pid;
}

void hello() {
  printf("hello");
  _exit(0);
}

int search_and_run(struct cmd_t *com, int gid) {
  int ret = -1;
  if (strcmp(com->cmd, "ls") == 0) {
    ret = exec(ls, "ls", com->argv, gid);
  } else if (strcmp(com->cmd, "hello") == 0) {
    ret = exec(hello, "hello", com->argv, gid);
  } else {
    printf("unknown program");
  }
clean:
  return ret;
}

bool interpret_line(char *line) {
  printf("line: %s", line);
  struct cmd_t commands[CMD_MAX];
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

  command_i++;
  if (command_i == 0)
    goto clean;

  //struct infop inop;
  int gid = -1;
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

    pid = search_and_run(com, gid);

    if (gid == -1)
      gid = pid;

    if (gid == -1)
      goto clean;
  }

  int status;
  int ret = -1;
  while ((ret = waitpid(-pid, &status, WEXITED | WSTOPPED)) > 0) {
    printf("\nwaited for child, exiting");
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

void readline(char *line) {
  int i = 0;
  while (1) {
    line[i] = getchar();
    printf("\nc[%d]=%c", i, line[i]);
    if (line[i] == EOF) {
      line[i] = '\0';
      break;
    }
    ++i;
    sleep(1);
  }
  printf("\n%s", line);
  printf("end");
}


void main(int argc, char** argv) {
  int size = N_TTY_BUF_SIZE - 1;
  char *line = calloc(size, sizeof(char));
  char path[20];
    
  while (true) {
    getcwd(&path, 20);
    printf("\n(%s)root@%s: ", "ext2", path);

    readline(line);
    printf(line);
    sleep(5);
    //interpret_line(line);
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
