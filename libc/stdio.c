#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <list.h>
#include <unistd.h>
#include <sys/stat.h>

LIST_HEAD(lstream);

static FILE *fdopen(int fd, const char *mode) {
  FILE *stream = calloc(1, sizeof(FILE));
  stream->fd = fd;
  list_add_tail(&stream->sibling, &lstream);

  // fchange_mode(stream, mode);

  // fill in blksize and mode

  struct stat stat = {0};
  fstat(fd, &stat);

  /*
        if (S_ISREG(stat.st_mode))
                stream->_flags |= _IO_FULLY_BUF;
        else if (isatty(fd))
                stream->_flags |= _IO_LINE_BUF;
        else
                stream->_flags |= _IO_UNBUFFERED;
  */

  stream->size = stat.st_size;
  return stream;
}

FILE *fopen(const char *filename, const char *mode) {
  int flags = O_RDWR;

  // support only reading for now
  if (mode[0] == 'r' && mode[1] != '+')
    flags = O_RDONLY;
  else {
    print("Supported only readonly");
    return NULL;
  }

  /*
        else if ((mode[0] == 'w' || mode[0] == 'a') && mode[1] != '+')
                flags = O_WRONLY;

        if ((mode[0] == 'w' && mode[1] == '+') || mode[0] == 'a')
                flags |= O_CREAT;
  */

  int fd = open(filename, flags /*| O_CREAT*/);
  if (fd < 0)
    return NULL;

  return fdopen(fd, mode);
}

static size_t fnget(void *ptr, size_t size, FILE *stream) {
	//assert_stream(stream);
  print("---- %x ----", ptr);
  int32_t count = read(stream->fd, ptr, 1);
  return count;
}


int fgetc(FILE *stream) {
  char ch;
  int ret = fnget(&ch, 1, stream);
  return ret == EOF ? ret : (unsigned char)ch;
}

char *gets(char *s, int n, FILE *stream) {
  int i;
  for (i = 0; i < n; ++i) {
    int ch = fgetc(stream);

    if (ch == '\n')
      break;
    if (ch == EOF)
      return NULL;

    s[i] = ch;
  }

  s[i] = 0;
  return s;
}