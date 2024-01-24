#include <stdint.h>
#include <malloc.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>

#include "../../../ports/newlib/myos/print.h"

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

void main(int argc, char** argv) {
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

  // test signals
  char* buf[30];
  for (;;) {
    //vfprintf(stream, "Hello from loop %d \n!", j);
    //fflush(stream);
    //j++;
    //sprintf(buf, "Hello from loop %d \n!\0", j);
    //fputs(buf, stream);
    //sleep(5);
  }

  
  
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
}
