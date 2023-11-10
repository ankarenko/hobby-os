include ../../../rules/platform.mk
include ../../../rules/variables.mk

C_SOURCES = $(wildcard *.c)
ASM_SOURCES = $(wildcard *.s)
OBJ = ${C_SOURCES:.c=.o} 
ASM_OBJ = ${ASM_SOURCES:.s=.o}

# with static variables doesn't work -fPIC 
# with -nostdinc doesnt work, I use stddef for example
# -fno-builtin not sure
CFLAGS := $(CFLAGS) -L $SYSROOT/usr/lib -lc -lg -lgcc -g0 -nostartfiles #-nostdlib

%.o: %.c
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) 

%.o: %.s
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS)

calc.exe: $(OBJ) linker.ld
	$(CC) -T linker.ld -o  $@ $(CFLAGS) $(OBJ)

install: calc.exe
test: install
clean:
	rm -rf *.exe

install-headers: