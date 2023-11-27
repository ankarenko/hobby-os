cd $SRC && mkdir -p $BUILD/newlib-$NEWLIB_VERSION && \
cd $BUILD/newlib-$NEWLIB_VERSION && mkdir -p $SYSROOT/usr/include && mkdir -p $SYSROOT/usr/lib && \
make all && make DESTDIR=$SYSROOT install
