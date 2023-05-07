include ../rules/platform.mk
include ../rules/variables.mk

C_SOURCES = $(wildcard *.c stdio/*.c stdlib/*.c string/*.c)

OBJS=$(ARCH_FREEOBJS) $(C_SOURCES:.c=.o)
LIBK_OBJS=$(OBJS:.o=.libk.o)
OUTPUT=libk.a
 
all: ${OUTPUT}
 
libc.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

${OUTPUT}: $(LIBK_OBJS)
	$(AR) rcs $@ $(LIBK_OBJS)
 
%.o: %.c
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS)
 
%.o: %.s
	$(CC) -MD -c $< -o $@ $(CFLAGS)
 
%.libk.o: %.c
	$(CC) -MD -c $< -o $@ -std=gnu11 $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)
 
%.libk.o: %.s
	$(CC) -MD -c $< -o $@ $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)
 
clean:
	rm -f $(OUTPUT) *.a
	rm -f $(OBJS) $(LIBK_OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) $(LIBK_OBJS:.o=.d) *.d */*.d */*/*.d

install: install-headers install-libs
 
install-headers:
	mkdir -p $(SYSROOT)/usr/include
	cp -R --preserve=timestamps include/. $(SYSROOT)/usr/include/
 
install-libs: $(OUTPUT)
	mkdir -p $(SYSROOT)/usr/lib
	cp $(OUTPUT) $(SYSROOT)/usr/lib
 
-include $(OBJS:.o=.d)
-include $(LIBK_OBJS:.o=.d)
