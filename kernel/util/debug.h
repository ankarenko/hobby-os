#ifndef INCLUDE_DEBUG_H
#define INCLUDE_DEBUG_H

#include <stdio.h>

#define panic(fmt, args...) { printf(fmt, args); while (true) {}; }

#ifndef NDEBUG
#define KASSERT(x) { if (!(x)) panic("\nassertion failed: %s", #x); }
#else
#define KASSERT(x)
#endif

#endif