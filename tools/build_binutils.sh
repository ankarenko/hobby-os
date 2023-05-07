cd $HOME/src
mkdir build-binutils 
cd build-binutils 
../binutils-$BINUTILS_VERSION/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror 
make
make install