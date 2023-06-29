#!/bin/sh
set -e
. ./headers.sh

for PROJECT in $PROJECTS; do
  (cd $PROJECT && make $1)
done