#!/bin/sh
BOOT_DIR=isodir/boot
DISK=./fat32_floppy.img

if alias os-machine >/dev/null 2>&1; then 
  ENVIRONMENT="os-machine"
else 
  ENVIRONMENT="docker run -it --rm -v $PWD:/os -v $PWD/ports/newlib/myos:/src/newlib-2.5.0/newlib/libc/sys/myos -v $PWD/ports/newlib/build:/src/build-newlib os_tools"; 
fi

if [ "$1" = "clean" ] 
then
  $ENVIRONMENT ./clean.sh
elif [ "$1" = "libc" ]
then
  $ENVIRONMENT sh /src/build_libc.sh
elif [ "$1" = "rebuild" ] 
then 
  $ENVIRONMENT ./clean.sh && \
  $ENVIRONMENT ./build.sh install && \
  $ENVIRONMENT ./make_iso.sh myos
elif [ "$1" = "build" ]
then 
  $ENVIRONMENT ./build.sh install && \
  $ENVIRONMENT ./make_iso.sh myos
elif [ "$1" = "test-build" ]
then
  $ENVIRONMENT ./clean.sh && \
  $ENVIRONMENT ./build.sh test && \
  $ENVIRONMENT ./make_iso.sh test
elif [ "$1" = "test" ]
then
  $ENVIRONMENT ./clean.sh && \
  $ENVIRONMENT ./build.sh test && \
  $ENVIRONMENT ./make_iso.sh test && \
  qemu-system-i386 -m 128 -monitor unix:qemu-monitor-socket,server,nowait -blockdev driver=file,node-name=f0,filename=$DISK -device floppy,drive=f0 -no-reboot -kernel isodir/boot/test.bin
elif [ "$1" = "run" ] 
then
  $ENVIRONMENT ./build.sh install && \
  $ENVIRONMENT ./make_iso.sh myos && \
  qemu-system-i386 -m 128 -monitor unix:qemu-monitor-socket,server,nowait -blockdev driver=file,node-name=f0,filename=$DISK -device floppy,drive=f0 -no-reboot -kernel isodir/boot/myos.bin
else 
  echo "Unknown param: clean, build, run"
fi






