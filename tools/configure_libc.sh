mkdir /src/build-newlib && \
cd /src/newlib-2.5.0/newlib/libc/sys && autoconf && \
cd myos && autoreconf && cd /src/build-newlib && \
../newlib-2.5.0/configure --prefix=/usr --target=i686-myos
