#!/bin/sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub
 
cp sysroot/boot/$1.bin isodir/boot/$1.bin
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "myos" {
	multiboot /boot/$1.bin
}
EOF
grub-mkrescue -o $1.iso isodir