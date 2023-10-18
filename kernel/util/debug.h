#ifndef INCLUDE_DEBUG_H
#define INCLUDE_DEBUG_H

#include <stdio.h>
#include "kernel/cpu/hal.h"

#define PANIC(fmt, args...) { printf(fmt, args); disable_interrupts(); for (;;); }
#define LOG(fmt, args...) printf(fmt, args);

#ifndef NDEBUG
#define KASSERT(x) { if (!(x)) PANIC("\nassertion failed: %s", #x); }
#else
#define KASSERT(x)
#endif


#if UNIT_TEST
#define unit_static 
#else
#define unit_static
#endif

#endif