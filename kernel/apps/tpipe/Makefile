include ../../../rules/platform.mk
include ../../../rules/variables.mk

C_SOURCES = $(wildcard *.c)
ASM_SOURCES = $(wildcard *.s)
OBJ = ${C_SOURCES:.c=.o} 
ASM_OBJ = ${ASM_SOURCES:.s=.o}

# with static variables doesn't work -fPIC 
# with -nostdinc doesnt work, I use stddef for example
# -fno-builtin not sure
CFLAGS1 := -g0

%.o: %.c
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS1) 

%.o: %.s
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS1)

bin/tpipe: $(OBJ)
	$(CC)  -o  $@ $(CFLAGS1) $(OBJ)

install: bin/tpipe
test: install
clean:
	rm -rf bin/*

install-headers: