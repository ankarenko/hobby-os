mkdir $BUILD/newlib && \
cd $SRC/newlib-2.5.0/newlib/libc/sys && autoconf && \
cd myos && autoreconf && cd $BUILD/newlib && \
$SRC/newlib-2.5.0/configure --prefix=/usr --target=$TARGET
