# Work around that the -elf gcc targets doesn't have a system include directory
# because it was configured with --without-headers rather than --with-sysroot.
CFLAGS=-O2 -g --sysroot=${SYSROOT} -isystem=/usr/include -ffreestanding -Wall -Wextra
LIBK_CFLAGS:=$(CFLAGS)
LIBK_CPPFLAGS:=$(CPPFLAGS) -D__is_libk
