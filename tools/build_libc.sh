cd /src && \
mkdir -p build-newlib && \
cd build-newlib && mkdir -p $SYSROOT/usr/include && mkdir -p $SYSROOT/usr/lib && \
make all && \
make DESTDIR=$SYSROOT install && \
cp -ar $SYSROOT/usr/i686-myos/* $SYSROOT/usr/ && \
rm -rf $SYSROOT/usr/i686-myos

#../newlib-2.5.0/configure --prefix=/usr --target=i686-myos