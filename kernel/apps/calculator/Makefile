include ../../../rules/platform.mk
include ../../../rules/variables.mk

C_SOURCES = $(wildcard *.c)
OBJ = ${C_SOURCES:.c=.o}

$(info $$var is [${PWD}])

%.o: %.c
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS)

calc.exe: $(OBJ) linker.ld
	$(CC) -T linker.ld -o $@ $(CFLAGS) $(OBJ) -nostdlib -lk -lgcc -fPIC

install: calc.exe
test: install
clean:

install-headers: