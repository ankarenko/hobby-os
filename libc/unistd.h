#ifndef _UNISTD_H
#define _UNISTD_H 1

#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>

#define __NR_exit 1
#define __NR_fork 2
#define __NR_read 3
#define __NR_write 4
#define __NR_open 5
#define __NR_close 6
#define __NR_sbrk 10
#define __NR_execve 11
#define __NR_lseek 19
#define __NR_getpid 20
#define __NR_kill 37
#define __NR_fstat 108
#define __NR_nanosleep 162
#define __NR_print 0

#define _syscall0(name)                       \
  static inline int32_t syscall_##name() {    \
    int32_t ret;                              \
    __asm__ __volatile__("int $0x80"          \
                         : "=a"(ret)          \
                         : "0"(__NR_##name)); \
    return ret;                               \
  }
#define _syscall1(name, type1)                           \
  static inline int32_t syscall_##name(type1 arg1) {     \
    int32_t ret;                                         \
    __asm__ __volatile__("int $0x80"                     \
                         : "=a"(ret)                     \
                         : "0"(__NR_##name), "b"(arg1)); \
    return ret;                                          \
  }

#define _syscall2(name, type1, type2)                               \
  static inline int32_t syscall_##name(type1 arg1, type2 arg2) {    \
    int32_t ret;                                                    \
    __asm__ __volatile__("int $0x80"                                \
                         : "=a"(ret)                                \
                         : "0"(__NR_##name), "b"(arg1), "c"(arg2)); \
    return ret;                                                     \
  }

#define _syscall3(name, type1, type2, type3)                                   \
  static inline int32_t syscall_##name(type1 arg1, type2 arg2, type3 arg3) {   \
    int32_t ret;                                                               \
    __asm__ __volatile__("int $0x80"                                           \
                         : "=a"(ret)                                           \
                         : "0"(__NR_##name), "b"(arg1), "c"(arg2), "d"(arg3)); \
    return ret;                                                                \
  }

#define _syscall4(name, type1, type2, type3, type4)                                       \
  static inline int32_t syscall_##name(type1 arg1, type2 arg2, type3 arg3, type4 arg4) {  \
    int32_t ret;                                                                          \
    __asm__ __volatile__("int $0x80"                                                      \
                         : "=a"(ret)                                                      \
                         : "0"(__NR_##name), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4)); \
    return ret;                                                                           \
  }

#define _syscall5(name, type1, type2, type3, type4, type5)                                           \
  static inline int32_t syscall_##name(type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5) { \
    int32_t ret;                                                                                     \
    __asm__ __volatile__("int $0x80"                                                                 \
                         : "=a"(ret)                                                                 \
                         : "0"(__NR_##name), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5)); \
    return ret;                                                                                      \
  }

#define SYSCALL_RETURN(expr) ({ int ret = expr; if (ret < 0) { return errno = -ret, -1; } return 0; })
#define SYSCALL_RETURN_ORIGINAL(expr) ({ int ret = expr; if (ret < 0) { return errno = -ret, -1; } return ret; })
// NOTE: MQ 2020-12-02
// if syscall is succeeded, returned address is always in userspace (0 <= addr < 0xc000000)
// on failure, returned value is in [-1024, -1] which translates to (0xfffffc00 <= addr <= 0xffffffff) -> always in kernelspace
#define HIGHER_HALF_ADDRESS 0xc000000
#define NULL 0
#define SYSCALL_RETURN_POINTER(expr) ({ int ret = expr; if ((int)HIGHER_HALF_ADDRESS < ret && ret < 0) { return errno = -ret, NULL; } return (void *)ret; })

int fork();
int sleep(unsigned int seconds);
int execve(const char *pathname, char *const argv[], char *const envp[]);
void* sbrk(intptr_t increment);
uint32_t read(int fd, char *buf, size_t size);
int write(int fd, const char *buf, size_t size);
int lseek(int fd, off_t offset, int whence);
int close(int fd);
int getpid();
int getdents(unsigned int, struct dirent *, unsigned int);
void print(char* format, ...);
void _exit(int32_t status);

#endif