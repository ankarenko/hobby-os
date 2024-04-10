#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "../../../ports/newlib/myos/msyscalls.h"

#define READ_END 0
#define WRITE_END 1

void main(int argc, char** argv) {
  int pid, pid_child;
  int pipefd[2];

  fprintf(stdout, "TEST PIPE");

  if (pipe(&pipefd) == -1) {
    fprintf(stderr, "can't create a pipe");
    return -1;
  }

  pid_child = fork();
  
  if (pid_child == -1) {
    fprintf(stderr, "Can't create parent");
    return -1;  
  } else if (pid_child == 0) {
    fprintf(stdout, "\nChild: pid=%d gid=%d", getpid(), getgid()); 
    
    // Set fd[0] (stdin) to the read end of the pipe
    
    if (dup2(pipefd[READ_END], STDIN_FILENO) == -1) {
      fprintf(stderr, "child: dup2 failed\n");
      return -1;
    }

    // Close the pipe now that we've duplicated it
    close(pipefd[READ_END]);
    close(pipefd[WRITE_END]);

    char *new_argv[] = { "/bin/print" };
    char *envp[] = { };

    // Call execve(2) which will replace the executable image of this
    // process
    execve(new_argv[0], &new_argv[0], envp);

    return 0;
  }
  
  fprintf(stdout, "\nParent: pid=%d, gid=%d", getpid(), getgid());
  
  int32_t wstatus = 0;
  
  if (dup2(pipefd[WRITE_END], STDOUT_FILENO) == -1) {
    fprintf(stderr, "parent: dup2 failed\n");
    return -1;
  }

  // Close the pipe now that we've duplicated it
  close(pipefd[READ_END]);
  close(pipefd[WRITE_END]);

  while (1) {
    fprintf(stdout, "Hello from parent\n");
    sleep(3);
  }
  
  /*
  fprintf(stdout, "\nWait for child");
  waitpid(pid_child, &wstatus, 0);
  fprintf(stdout, "\nChild exited");
  */

  return 0;
}