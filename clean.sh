#!/bin/sh
set -e
. ./config.sh
 
for PROJECT in $PROJECTS; do
  (cd $PROJECT && make clean)
done
 
#rm -rf sysroot/usr/lib/*
rm -rf sysroot/usr/include/kernel
rm -rf isodir
rm -rf myos.iso