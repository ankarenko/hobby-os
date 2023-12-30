include ../rules/platform.mk
include ../rules/variables.mk

PATHS= \
	main.c \
	cpu/*.c \
	devices/*.c devices/**/*.c \
	system/*.c \
	memory/*.c \
	fs/*.c fs/*/**.c \
	ipc/*.c \
	proc/*.c proc/*/**.c \
	locking/*.c locking/*/**.c \
	include/*.c \
	util/*.c util/string/*.c

C_ALL_SOURCES = $(wildcard ${PATHS})
C_SOURCES = $(filter-out ${C_ALL_SOURCES:.c=.test.c}, ${C_ALL_SOURCES})
C_SOURCES_TEST = $(filter-out main.c, $(wildcard main.test.c ${C_ALL_SOURCES}))

A_SOURCES = $(wildcard ${PATHS:.c=.s})
HEADERS = $(wildcard ../test/greatest.h *.h ${PATHS:.c=.h} util/*.h util/string/*.h)
OBJ = \
	crti.o \
	crtbegin.o \
	boot.o \
	${A_SOURCES:.s=.o} \
	${C_SOURCES:.c=.o} \
	crtend.o \
	crtn.o

OBJ_TEST = \
	crti.o \
	crtbegin.o \
	boot.o \
	${A_SOURCES:.s=.o} \
	${C_SOURCES_TEST:.c=.o} \
	crtend.o \
	crtn.o

%.o: %.c
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS)
 
%.o: %.s
	$(CC) -MD -c $< -o $@ $(CFLAGS)

myos.bin: $(OBJ) linker.ld
	$(CC) -T linker.ld -o $@ $(CFLAGS) $(OBJ) -nostdlib -lgcc
	grub-file --is-x86-multiboot myos.bin

test.bin: $(OBJ_TEST) linker.ld
	$(CC) -T linker.ld -o $@ $(CFLAGS) $(OBJ_TEST) -nostdlib -lgcc
	grub-file --is-x86-multiboot test.bin
 
crtbegin.o crtend.o:
	OBJ=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@
 
clean:
	rm -f myos.bin
	rm -f test.bin
	rm -f $(OBJ) *.o */*.o */*/*.o
	rm -f $(OBJ:.o=.d) *.d */*.d */*/*.d
 
install: install-headers install-kernel

test: install-headers test-kernel

install-headers:
	mkdir -p $(SYSROOT)/usr/include/kernel
	cp --parents -R --preserve=timestamps $(patsubst %, ./%, $(HEADERS)) $(SYSROOT)/usr/include/kernel/
 
install-kernel: myos.bin
	mkdir -p $(SYSROOT)/boot
	cp myos.bin $(SYSROOT)/boot

test-kernel: test.bin
	mkdir -p $(SYSROOT)/boot
	cp test.bin $(SYSROOT)/boot
 
-include $(OBJS:.o=.d)
