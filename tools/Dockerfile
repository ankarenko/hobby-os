#Building environment for our OS
FROM ubuntu:18.04

ENV PREFIX="$HOME/opt/cross"
ENV TARGET=i686-elf
ENV PATH="$PREFIX/bin:$PATH"
ENV BINUTILS_VERSION="2.39"
ENV GCC_VERSION="12.2.0"

RUN \
  apt update && \
  apt install wget -y && \
  apt install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo libisl-0.18-dev libcloog-isl-dev -y

WORKDIR $HOME/src
RUN \
  wget -c https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.gz -O - | tar -xz && \
  wget -c https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz -O - | tar -xz

COPY ./tools/build_binutils.sh ./
COPY ./tools/build_gcc.sh ./

RUN \
  chmod +x ./build_binutils.sh && \
  chmod +x ./build_gcc.sh

RUN sh ./build_binutils.sh
RUN sh ./build_gcc.sh

#check
RUN $TARGET-gcc --version

RUN apt install xorriso grub-pc-bin -y
RUN apt install nano -y

# making preparation to port newlib
WORKDIR $HOME/src
RUN wget -c ftp://sourceware.org/pub/newlib/newlib-2.5.0.tar.gz -O - | tar -xz

RUN mkdir /root/bin

# install automake and autoconf
WORKDIR $HOME/src
ARG AUTOMAKE_VER=1.11
ARG AUTOCONF_VER=2.65
ENV PATH="$PATH:/root/bin/autoconf-$AUTOCONF_VER/bin:/root/bin/automake-$AUTOMAKE_VER/bin"

RUN \
  wget -c https://ftp.gnu.org/gnu/automake/automake-$AUTOMAKE_VER.tar.gz -O - | tar -xz && \
  wget -c https://ftp.gnu.org/gnu/autoconf/autoconf-$AUTOCONF_VER.tar.gz -O - | tar -xz

RUN mkdir /root/bin/autoconf-$AUTOCONF_VER && mkdir /root/bin/automake-$AUTOMAKE_VER

RUN \
  mkdir build-autoconf-$AUTOCONF_VER && cd build-autoconf-$AUTOCONF_VER && \
  ../autoconf-$AUTOCONF_VER/configure --prefix=/root/bin/autoconf-$AUTOCONF_VER && \
  make && make install 

RUN \
  mkdir build-automake-$AUTOMAKE_VER && cd build-automake-$AUTOMAKE_VER && \
  ../automake-$AUTOMAKE_VER/configure --prefix=/root/bin/automake-$AUTOMAKE_VER && \
  make && make install

# fix of Unescaped left brace in regex is illegal here in regex; marked by <-- HERE in m/\${ <-- HERE ([^ \t=:+{}]+)}/ at /root/bin/automake line 4113.
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82856#c3
RUN sed -i \
  '/$text =~ s\/\\${(\[\^ \\t=:+{}\]+)}\/&substitute_ac_subst_variables_worker ($1)\/ge;/c $text =~ s/\\$\\{([^ \t=:+{}]+)\\}/&substitute_ac_subst_variables_worker ($1)/ge;' \
  /root/bin/automake-$AUTOMAKE_VER/bin/automake

# add myos to config files
# source: https://wiki.osdev.org/Porting_Newlib
RUN \
  sed -i '1000,$s/-gnu\* |/& -myos* |/' newlib-2.5.0/config.sub && \
  sed -i 's/case "${host_cpu}" in/&\ni[3-7]86-*-myos*)\nsys_dir=myos;;/' newlib-2.5.0/newlib/configure.host && \
  echo "sys_dir=myos" >> newlib-2.5.0/newlib/configure.host && \
  sed -i 's/case ${sys_dir} in/&\nmyos) AC_CONFIG_SUBDIRS(myos) ;;/' newlib-2.5.0/newlib/libc/sys/configure.in && \
  autoconf --version && cd newlib-2.5.0/newlib/libc/sys && autoconf

RUN mkdir newlib-2.5.0/newlib/libc/sys/myos
COPY ./ports/newlib/myos newlib-2.5.0/newlib/libc/sys/myos

# newlib setup
ENV CURRDIR=$HOME/src
 
# make symlinks (a bad hack) to make newlib work
# this is where the bootstrapped generic cross compiler toolchain (i686-elf-xxx) is installed in,
# change this based on your development environment.
RUN \
  cd /opt/cross/bin/ && \
  ln i686-elf-ar i686-myos-ar && \
  ln i686-elf-as i686-myos-as && \
  ln i686-elf-gcc i686-myos-gcc && \
  ln i686-elf-gcc i686-myos-cc && \
  ln i686-elf-ranlib i686-myos-ranlib

WORKDIR $CURRDIR

RUN mkdir /sysroot
ENV SYSROOT=/os/sysroot

COPY ./tools/build_libc.sh ./
RUN chmod +x ./build_libc.sh

COPY ./tools/configure_libc.sh ./
RUN chmod +x ./configure_libc.sh


# build toolchain
# install automake and autoconf
WORKDIR $HOME/src
ARG AUTOMAKE_VER=1.16.5
ARG AUTOCONF_VER=2.71

RUN \
  wget -c https://ftp.gnu.org/gnu/automake/automake-$AUTOMAKE_VER.tar.gz -O - | tar -xz && \
  wget -c https://ftp.gnu.org/gnu/autoconf/autoconf-$AUTOCONF_VER.tar.gz -O - | tar -xz

RUN mkdir /root/bin/autoconf-$AUTOCONF_VER && mkdir /root/bin/automake-$AUTOMAKE_VER

RUN \
  mkdir build-autoconf-$AUTOCONF_VER && cd build-autoconf-$AUTOCONF_VER && \
  ../autoconf-$AUTOCONF_VER/configure --prefix=/root/bin/autoconf-$AUTOCONF_VER && \
  make && make install 

RUN \
  mkdir build-automake-$AUTOMAKE_VER && cd build-automake-$AUTOMAKE_VER && \
  ../automake-$AUTOMAKE_VER/configure --prefix=/root/bin/automake-$AUTOMAKE_VER && \
  make && make install

# switch betweeen versions
# export PATH=/root/bin/automake-1.16.5/bin:/root/bin/autoconf-2.71/bin:$PATH
# export PATH=/root/bin/automake-1.11/bin:/root/bin/autoconf-2.65/bin:$PATH

VOLUME /os
VOLUME /src/newlib-2.5.0/newlib/libc/sys/myos
VOLUME /src/build-newlib
WORKDIR /os

CMD bash