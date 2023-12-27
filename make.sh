#!/bin/sh
BOOT_DIR=isodir/boot

#echo 'MYOS_QEMU_PARAMS="-m 128 -blockdev driver=file,node-name=f0,filename=./fat32_floppy.img -device floppy,drive=f0 -no-reboot"' >> ~/.bashrc


if alias os-machine >/dev/null 2>&1; then 
  ENVIRONMENT="os-machine"
else 
  ENVIRONMENT="docker run -it --rm -v $PWD:/os -v $PWD/sysroot:/os/sysroot -v $PWD/ports/newlib/myos:/src/newlib-2.5.0/newlib/libc/sys/myos -v $PWD/build:/src/build os_tools"; 
fi

if [ "$1" = "clean" ] 
then
  $ENVIRONMENT ./clean.sh
elif [ "$1" = "build_libc" ]
then
  $ENVIRONMENT sh /src/libc.sh build && $ENVIRONMENT sh /src/libc.sh install
elif [ "$1" = "rebuild_libc" ]
then
  $ENVIRONMENT sh /src/libc.sh rebuild && $ENVIRONMENT sh /src/libc.sh install
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
  #$ENVIRONMENT ./clean.sh && \
  $ENVIRONMENT ./build.sh test && \
  $ENVIRONMENT ./make_iso.sh test && \
  qemu-system-i386 -monitor unix:qemu-monitor-socket,server,nowait -chardev stdio,id=char0,logfile=uart1.log -serial chardev:char0 $MYOS_QEMU_PARAMS -kernel isodir/boot/test.bin
elif [ "$1" = "run" ] 
then
  $ENVIRONMENT ./build.sh install && \
  $ENVIRONMENT ./make_iso.sh myos && \
  qemu-system-i386 -monitor unix:qemu-monitor-socket,server,nowait -chardev stdio,id=char0,logfile=uart1.log -serial chardev:char0 $MYOS_QEMU_PARAMS -kernel isodir/boot/myos.bin
else 
  echo "Unknown param: clean, build, run"
fi






