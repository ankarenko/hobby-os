#!/bin/sh
set -e
. ./headers.sh

for PROJECT in $PROJECTS; do
  echo $PROJECT
  if [ $PROJECT = "libc" ] 
  then # compile kernel and userspace version
    #(cd $PROJECT && make $1 LIBC_FLAG=-D__is_libk OUTPUT=libk.a)
    (cd $PROJECT && make $1 LIBC_FLAG=-D__is_libc OUTPUT=libc.a)
  else
    (cd $PROJECT && make $1)
  fi
  
done