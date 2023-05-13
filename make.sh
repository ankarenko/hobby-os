#!/bin/sh
BOOT_DIR=isodir/boot

if alias os-machine >/dev/null 2>&1; then 
  ENVIRONMENT="os-machine"
else 
  ENVIRONMENT="docker run -it --rm -v $PWD:/os os_tools"; 
fi

if [ "$1" = "clean" ] 
then
  $ENVIRONMENT ./clean.sh
elif [ "$1" = "rebuild" ] 
then 
  $ENVIRONMENT ./clean.sh && \
  $ENVIRONMENT ./build.sh && \
  $ENVIRONMENT ./make_iso.sh
elif [ "$1" = "build" ] 
then 
  $ENVIRONMENT ./build.sh && \
  $ENVIRONMENT ./make_iso.sh
elif [ "$1" = "run" ] 
then
  $ENVIRONMENT ./build.sh && \
  $ENVIRONMENT ./make_iso.sh && \
  qemu-system-i386 -m 4096 -monitor unix:qemu-monitor-socket,server,nowait -no-reboot -kernel isodir/boot/myos.bin
else 
  echo "Unknown param: clean, build, run"
fi






