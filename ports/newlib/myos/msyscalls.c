#include "msyscalls.h"

#include <stdint.h>

_syscall0(dbg_ps);
int dbg_ps() {
  SYSCALL_RETURN(syscall_dbg_ps());
}