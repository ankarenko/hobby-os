#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

_syscall3(open, const char*, int32_t, mode_t);
int open(const char* path, int flags, ...) {

  mode_t mode = 0;
  /*
  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  */

  SYSCALL_RETURN_ORIGINAL(syscall_open(path, flags, mode));
}