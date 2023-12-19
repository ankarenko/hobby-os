#ifndef INCLUDE_DEBUG_H
#define INCLUDE_DEBUG_H

#include "kernel/util/stdio.h"
#include "kernel/cpu/hal.h"


#define PANIC(fmt, args...) { printf(fmt, args); disable_interrupts(); for (;;); }
#define LOG(fmt, args...) printf(fmt, args);


#define assert_not_reached() PANIC("\nshould not be reached", NULL)
#define assert_not_implemented() PANIC("\nnot implemented", NULL)

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