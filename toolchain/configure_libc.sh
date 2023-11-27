mkdir $BUILD/newlib-$NEWLIB_VERSION && \
cd $SRC/newlib-$NEWLIB_VERSION/newlib/libc/sys && autoconf && \
cd myos && autoreconf && cd $BUILD/newlib-$NEWLIB_VERSION && \
$SRC/newlib-$NEWLIB_VERSION/configure --prefix=/usr --target=$TARGET
