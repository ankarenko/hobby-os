cd $SRC && mkdir $BUILD/gcc && cd $BUILD/gcc && \
$SRC/gcc-$GCC_VERSION/configure --target=$OLD_TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers && \
make all-gcc && make all-target-libgcc && \
make install-gcc && make install-target-libgcc