#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kernel/util/fcntl.h"
#include "kernel/util/list.h"
#include "stdio.h"

#define MIN_WRITE_BUF_LEN 32

LIST_HEAD(lstream);

static bool valid_stream(FILE *stream) {
	return stream->fd != -1;
}

static void assert_stream(FILE *stream) {
	if (stream->_IO_write_base <= stream->_IO_write_ptr && stream->_IO_write_ptr <= stream->_IO_write_end + 1) {
    
  } else {
    __asm__ __volatile("int $0x01");
  }
}

static int fnput(const char *s, size_t size, FILE *stream) {
  assert_stream(stream);

	size_t remaining_len = stream->_IO_write_end - stream->_IO_write_ptr;
	size_t current_len = stream->_IO_write_end - stream->_IO_write_base;
  
  if (remaining_len < size) {
    size_t new_len = max(max(size, current_len << 1), MIN_WRITE_BUF_LEN);
		char *buf = calloc(new_len, sizeof(char));

		memcpy(buf, stream->_IO_write_base, current_len);
		free(stream->_IO_write_base);

		stream->_IO_write_base = buf;
		stream->_IO_write_ptr = buf + current_len - remaining_len;
		stream->_IO_write_end = buf + new_len;
	}
	memcpy(stream->_IO_write_ptr, s, size);
	stream->_IO_write_ptr += size;
	stream->_offset += size;
	return size;
}

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


  stream->_IO_write_base = stream->_IO_write_ptr = stream->_IO_write_end = NULL;
  stream->size = stat.st_size;
  return stream;
}

static int32_t fnget(void *ptr, size_t size, FILE *stream) {
  return read(stream->fd, ptr, 1);
}

int fflush(FILE *stream) {
	if (!stream) {
		FILE *iter;
		list_for_each_entry(iter, &lstream, sibling) {
			fflush(iter);
      //if (!(iter->_flags & _IO_NO_WRITES))
		}
		return 0;
	}

	assert_stream(stream);
	if (!valid_stream(stream))
		return -EBADF;

	size_t unwritten_len = stream->_IO_write_ptr - stream->_IO_write_base;

	if (!unwritten_len)
		return 0;

	write(stream->fd, stream->_IO_write_base, unwritten_len);
	free(stream->_IO_write_base);

	stream->_IO_write_base = stream->_IO_write_ptr = stream->_IO_write_end = NULL;
	return 0;
}

FILE *fopen(const char *filename, const char *mode) {
  int flags = O_RDWR;

  // support only reading for now
  if (mode[0] == 'r' && mode[1] != '+')
    flags = O_RDONLY;
  
  if ((mode[0] == 'w' || mode[0] == 'a') && mode[1] != '+')
    flags = O_WRONLY;

  if ((mode[0] == 'w' && mode[1] == '+') || mode[0] == 'a')
    flags |= O_CREAT;

  int fd = open(filename, flags | O_CREAT);
  
  if (fd < 0)
    return -EBADF;

  return fdopen(fd, mode);
}

int fgetc(FILE *stream) {
  char ch;
  int ret = fnget(&ch, 1, stream);
  return ret == 0 ? EOF : (unsigned char)ch;
}

char *fgets(char *s, int n, FILE *stream) {
  int i;
  for (i = 0; i < n; ++i) {
    int ch = fgetc(stream);

    if (ch == '\n' || (ch == EOF && i != 0)) {
      break;
    }

    if (ch <= 0) {
      return NULL;
    }

    s[i] = ch;
  }

  s[i] = '\0';
  return s;
}

/*
Note: fread() function itself does not provide a way to
distinguish between end-of-file and error, feof and ferror can
be used to determine which occurred.
*/
size_t fread(void *buffer, size_t size, size_t count, FILE *stream) {
  size_t final_size = count * size;
  int32_t res = fnget(buffer, final_size, stream);
  return max(res, 0);
}

int fputs(const char *s, FILE *stream) {
  assert_stream(stream);
  int slen = strlen(s);
	return fnput(s, slen, stream);
}

int fputc(int c, FILE *stream) {
	char ch = (unsigned char)c;
	fnput(&ch, 1, stream);
	return ch;
}

int fclose(FILE *stream) {
	assert_stream(stream);
	if (!valid_stream(stream))
		return -EBADF;

	fflush(stream);
	free(stream->_IO_write_base);
	stream->_IO_write_base = stream->_IO_write_ptr = stream->_IO_write_end = NULL;

	close(stream->fd);
	stream->fd = -1;

	return 0;
}