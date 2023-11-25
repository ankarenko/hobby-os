cd $SRC && mkdir -p $BUILD/newlib && \
cd $BUILD/newlib && mkdir -p $SYSROOT/usr/include && mkdir -p $SYSROOT/usr/lib && \
make all && make DESTDIR=$SYSROOT install && \
cp -ar $SYSROOT/usr/i686-myos/* $SYSROOT/usr/ && \
rm -rf $SYSROOT/usr/i686-myos
