mkdir $BUILD/binutils && cd $BUILD/binutils && \
$SRC/binutils-$BINUTILS_VERSION/configure --target=$OLD_TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror && \
make && make install