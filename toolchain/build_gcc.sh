cd $HOME/src

which -- $OLD_TARGET-as || echo $OLD_TARGET-as is not in the PATH
 
mkdir build-gcc
cd build-gcc
../gcc-$GCC_VERSION/configure --target=$OLD_TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers 
make all-gcc
make all-target-libgcc
make install-gcc
make install-target-libgcc