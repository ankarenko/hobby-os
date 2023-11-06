include ../rules/platform.mk
include ../rules/variables.mk
PATHS= *.c \
	stdio/*.c \
	stdlib/*.c \
	string/*.c

C_ALL_SOURCES = $(wildcard ${PATHS})
C_SOURCES = $(filter-out ${C_ALL_SOURCES:.c=.test.c}, ${C_ALL_SOURCES})
C_SOURCES_TEST = $(filter-out main.c, $(wildcard main.test.c ${C_ALL_SOURCES}))
HEADERS = $(wildcard *.h ${PATHS:.c=.h})

OBJS=$(ARCH_FREEOBJS) $(C_SOURCES_TEST:.c=.o)
LIBK_OBJS=$(OBJS:.o=.libk.o)

$(info $$var is [$(OUTPUT)])
$(info $$var is [$(LIBC_FLAG)])

LIBK_CFLAGS := $(LIBK_CFLAGS) $(LIBC_FLAG)

#-D__is_libk

all: ${OUTPUT}
 
libc.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

${OUTPUT}: $(LIBK_OBJS)
	$(AR) rcs $@ $(LIBK_OBJS)
 
%.o: %.c
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(LIBK_CFLAGS)
 
%.o: %.s
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(LIBK_CFLAGS)
 
%.libk.o: %.c
	$(CC) -MD -c $< -o $@ -std=gnu11 $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)
 
%.libk.o: %.s
	$(CC) -MD -c $< -o $@ $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)
 
clean:
	rm -f $(OUTPUT) *.a
	rm -f $(OBJS) $(LIBK_OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) $(LIBK_OBJS:.o=.d) *.d */*.d */*/*.d

install: install-headers install-libs
 
test: install-headers install-libs

install-headers:
	mkdir -p $(SYSROOT)/usr/include
	cp -R --preserve=timestamps include/. ${HEADERS} $(SYSROOT)/usr/include/

install-libs: $(OUTPUT)
	mkdir -p $(SYSROOT)/usr/lib
	cp $(OUTPUT) $(SYSROOT)/usr/lib
 
-include $(OBJS:.o=.d)
-include $(LIBK_OBJS:.o=.d)
