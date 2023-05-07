#!/bin/sh
set -e
. ./config.sh
 
mkdir -p "$SYSROOT"
 
for PROJECT in $PROJECTS; do
  (cd $PROJECT && make install-headers)
done
