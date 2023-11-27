# clean
# rebuild
# build

BUILD_DIR=$BUILD/newlib-$NEWLIB_VERSION

if [ "$1" = "clean" ] 
then
  rm -rf $BUILD_DIR/* && \
  rm -rf $SYSROOT/*
elif [ "$1" = "build" ]
then
  sh ./build_libc.sh && 
  cp -r $BUILD_DIR/$TARGET/newlib-$NEWLIB_VERSION/libc/sys/$OS_NAME/crt0.o $SYSROOT/usr/lib/
elif [ "$1" = "rebuild" ] 
then
  rm -rf $BUILD_DIR && \
  sh ./configure_libc.sh && ./build_libc.sh
elif [ "$1" = "install" ] 
then
  rm -rf $SYSROOT/usr/include/* && rm -rf $SYSROOT/usr/lib/* && \
  cp -ar $SYSROOT/usr/$TARGET/* $SYSROOT/usr/ && \
  rm -rf $SYSROOT/usr/$TARGET && \
  cp -r $BUILD_DIR/$TARGET/newlib/libc/sys/$OS_NAME/crt0.o $SYSROOT/usr/lib/
else 
  echo "Unknown param, use: clean, build, rebuild"
fi
