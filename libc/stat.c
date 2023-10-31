#include <sys/stat.h>
#include <unistd.h>

_syscall2(fstat, int32_t, struct stat*);
int fstat(int fildes, struct stat* buf) {
	SYSCALL_RETURN(syscall_fstat(fildes, buf));
}