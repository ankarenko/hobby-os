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

COPY ./build_binutils.sh ./
COPY ./build_gcc.sh ./

RUN \
  chmod +x ./build_binutils.sh && \
  chmod +x ./build_gcc.sh

RUN sh ./build_binutils.sh
RUN sh ./build_gcc.sh

#check
RUN $TARGET-gcc --version

VOLUME /os
WORKDIR /os

RUN apt install xorriso grub-pc-bin -y

CMD bash