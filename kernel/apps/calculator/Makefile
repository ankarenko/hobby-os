include ../../../rules/platform.mk
include ../../../rules/variables.mk

C_SOURCES = $(wildcard *.c)
ASM_SOURCES = $(wildcard *.s)
OBJ = ${C_SOURCES:.c=.o} 
ASM_OBJ = ${ASM_SOURCES:.s=.o}

CFLAGS := $(CFLAGS) -nostdlib -lk -lgcc -fPIC -g0 -nostdinc -fno-builtin -nostartfiles

%.o: %.c
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS)

%.o: %.s
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS)

calc.exe: $(OBJ) linker.ld
	$(CC) -T linker.ld -o $@ $(CFLAGS) $(OBJ)

asm_calc.exe: $(ASM_OBJ) linker.ld
	$(CC) -T linker.ld -o $@ $(CFLAGS) $(ASM_OBJ) 


install: asm_calc.exe calc.exe
test: install
clean:
	rm -rf *.exe

install-headers: