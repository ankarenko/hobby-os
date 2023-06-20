include ../rules/platform.mk
include ../rules/variables.mk

C_SOURCES = $(wildcard main.c cpu/*.c devices/*.c devices/**/*.c system/*.c memory/*.c fs/*.c fs/*/**.c proc/*.c proc/*/**.c)
A_SOURCES = $(wildcard cpu/*.s proc/*.s)
HEADERS = $(wildcard *.h include/*.h cpu/*.h devices/*.h devices/**/*.h system/*.h memory/*.h fs/*.h fs/*/**.h proc/*.h proc/*/**.h)
OBJ = \
	crti.o \
	crtbegin.o \
	boot.o \
	${A_SOURCES:.s=.o} \
	${C_SOURCES:.c=.o} \
	crtend.o \
	crtn.o

%.o: %.c
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS)
 
%.o: %.s
	$(CC) -MD -c $< -o $@ $(CFLAGS)

myos.bin: $(OBJ) linker.ld
	$(CC) -T linker.ld -o $@ $(CFLAGS) $(OBJ) -nostdlib -lk -lgcc
	grub-file --is-x86-multiboot myos.bin
 
crtbegin.o crtend.o:
	OBJ=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@
 
clean:
	rm -f myos.bin
	rm -f $(OBJ) *.o */*.o */*/*.o
	rm -f $(OBJ:.o=.d) *.d */*.d */*/*.d
 
install: install-headers install-kernel

install-headers:
	mkdir -p $(SYSROOT)/usr/include/kernel
	cp --parents -R --preserve=timestamps $(patsubst %, ./%, $(HEADERS)) $(SYSROOT)/usr/include/kernel/
 
install-kernel: myos.bin
	mkdir -p $(SYSROOT)/boot
	cp myos.bin $(SYSROOT)/boot
 
-include $(OBJS:.o=.d)
