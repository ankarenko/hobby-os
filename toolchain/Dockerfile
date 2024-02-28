# this docker file represents a toolchain for my os
FROM ubuntu:18.04

RUN \
  apt update && \
  apt install wget -y && \
  apt install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo libisl-0.18-dev libcloog-isl-dev -y && \
  apt install xorriso grub-pc-bin -y && \
  apt install nano -y

ENV OS_NAME=myos
ENV PREFIX="/opt/cross"
ENV OLD_TARGET=i686-elf
ENV TARGET=i686-$OS_NAME
ENV PATH="$PREFIX/bin:$PATH"
ENV BINUTILS_VERSION="2.39"
ENV GCC_VERSION="12.2.0"
ENV SRC=/src
ENV BUILD=$SRC/build
ENV NEWLIB_VERSION="2.5.0"
ENV SYSROOT=/os/sysroot
# switch betweeen versions
# export PATH=/root/bin/automake-1.15.1/bin:/root/bin/autoconf-2.69/bin:$PATH
# export PATH=/root/bin/automake-1.11/bin:/root/bin/autoconf-2.65/bin:$PATH

WORKDIR $SRC
RUN mkdir -p /root/bin
RUN mkdir -p $BUILD

# download newlib
RUN wget -c ftp://sourceware.org/pub/newlib/newlib-$NEWLIB_VERSION.tar.gz -O - | tar -xz

# install headers, so libgcc can be compiled properly
RUN \
  mkdir -p $SYSROOT && cd /os && \
  mkdir -p sysroot/usr/include && \
  cp -r $SRC/newlib-$NEWLIB_VERSION/newlib/libc/include/* $SYSROOT/usr/include/

# install automake and autoconf
ARG AUTOMAKE_VER=1.11
ARG AUTOCONF_VER=2.65
# make autoconf and automake of these versions default 
# as it will be used heavily while rebuilding newlib
ENV PATH="$PATH:/root/bin/autoconf-$AUTOCONF_VER/bin:/root/bin/automake-$AUTOMAKE_VER/bin"

RUN \
  wget -c https://ftp.gnu.org/gnu/automake/automake-$AUTOMAKE_VER.tar.gz -O - | tar -xz && \
  wget -c https://ftp.gnu.org/gnu/autoconf/autoconf-$AUTOCONF_VER.tar.gz -O - | tar -xz

RUN mkdir /root/bin/autoconf-$AUTOCONF_VER && mkdir /root/bin/automake-$AUTOMAKE_VER

RUN \
  mkdir $BUILD/autoconf-$AUTOCONF_VER && cd $BUILD/autoconf-$AUTOCONF_VER && \
  $SRC/autoconf-$AUTOCONF_VER/configure --prefix=/root/bin/autoconf-$AUTOCONF_VER && \
  make && make install 

RUN \
  mkdir $BUILD/automake-$AUTOMAKE_VER && cd $BUILD/automake-$AUTOMAKE_VER && \
  $SRC/automake-$AUTOMAKE_VER/configure --prefix=/root/bin/automake-$AUTOMAKE_VER && \
  make && make install

# fix of Unescaped left brace in regex is illegal here in regex; marked by <-- HERE in m/\${ <-- HERE ([^ \t=:+{}]+)}/ at /root/bin/automake line 4113.
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82856#c3
RUN sed -i \
  '/$text =~ s\/\\${(\[\^ \\t=:+{}\]+)}\/&substitute_ac_subst_variables_worker ($1)\/ge;/c $text =~ s/\\$\\{([^ \t=:+{}]+)\\}/&substitute_ac_subst_variables_worker ($1)/ge;' \
  /root/bin/automake-$AUTOMAKE_VER/bin/automake

# starting to build our os specific toolchain
# install automake and autoconf compitible with our gcc
# they are not default, so we will refer them explicitly using 'export'
ARG AUTOMAKE_VER=1.15.1
ARG AUTOCONF_VER=2.69

RUN \
  wget -c https://ftp.gnu.org/gnu/automake/automake-$AUTOMAKE_VER.tar.gz -O - | tar -xz && \
  wget -c https://ftp.gnu.org/gnu/autoconf/autoconf-$AUTOCONF_VER.tar.gz -O - | tar -xz

RUN mkdir /root/bin/autoconf-$AUTOCONF_VER && mkdir /root/bin/automake-$AUTOMAKE_VER

RUN \
  mkdir $BUILD/autoconf-$AUTOCONF_VER && cd $BUILD/autoconf-$AUTOCONF_VER && \
  $SRC/autoconf-$AUTOCONF_VER/configure --prefix=/root/bin/autoconf-$AUTOCONF_VER && \
  make && make install 

RUN \
  mkdir $BUILD/automake-$AUTOMAKE_VER && cd $BUILD/automake-$AUTOMAKE_VER && \
  $SRC/automake-$AUTOMAKE_VER/configure --prefix=/root/bin/automake-$AUTOMAKE_VER && \
  make && make install

RUN \
  wget -c https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.gz -O - | tar -xz && \
  wget -c https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz -O - | tar -xz

# configure binutils to be used with our $OS_NAME and build it
RUN \
  export PATH=/root/bin/automake-1.15.1/bin:/root/bin/autoconf-2.69/bin:$PATH && \
  sed -i '1720,+10s/gnu\* |/& '"$OS_NAME"'* |/' binutils-$BINUTILS_VERSION/config.sub && \
  sed -i 's/# START OF targmatch.h/&\n  i[3-7]86-*-'"$OS_NAME"'*)\n    targ_defvec=i386_elf32_vec\n    targ_selvecs=\n    targ64_selvecs=x86_64_elf64_vec\n    ;;\n#ifdef BFD64\n  x86_64-*-'"$OS_NAME"'*)\n    targ_defvec=x86_64_elf64_vec\n    targ_selvecs=i386_elf32_vec\n    want64=true\n    ;;\n#endif/1' binutils-$BINUTILS_VERSION/bfd/config.bfd && \
  sed -i 's/case ${generic_target} in/&\n  i386-*-'"$OS_NAME"'*)    fmt=elf;;/' binutils-$BINUTILS_VERSION/gas/configure.tgt && \
  sed -i '80,+10s/case "${targ}" in/&\ni[3-7]86-*-'"$OS_NAME"'*)\n      targ_emul=elf_i386_'"$OS_NAME"'\n      targ_extra_emuls=elf_i386\n      targ_extra_emuls=elf_i386\n      targ64_extra_emuls="elf_x86_64_'"$OS_NAME"' elf_x86_64"\n      ;;\nx86_64-*-'"$OS_NAME"'*)\n      targ_emul=elf_x86_64_'"$OS_NAME"'\n      targ_extra_emuls="elf_i386_'"$OS_NAME"' elf_x86_64 elf_i386"\n      ;;/1' binutils-$BINUTILS_VERSION/ld/configure.tgt && \
  touch binutils-$BINUTILS_VERSION/ld/emulparams/elf_i386_$OS_NAME.sh && \
  printf "source_sh \${srcdir}/emulparams/elf_i386.sh\nTEXT_START_ADDR=0x00400000" > binutils-$BINUTILS_VERSION/ld/emulparams/elf_i386_$OS_NAME.sh && \
  touch binutils-$BINUTILS_VERSION/ld/emulparams/elf_x86_64_$OS_NAME.sh && \
  printf "source_sh \${srcdir}/emulparams/elf_x86_64.sh\nTEXT_START_ADDR=0x00400000" > binutils-$BINUTILS_VERSION/ld/emulparams/elf_x86_64_$OS_NAME.sh && \
  sed -i 's/@AMDEP_TRUE@@am__include@ @am__quote@.\/\$(DEPDIR)\/eelf_i386.Pc@am__quote@/&\n@AMDEP_TRUE@@am__include@ @am__quote@.\/\$(DEPDIR)\/eelf_i386_'"$OS_NAME"'.Pc@am__quote@/1' binutils-$BINUTILS_VERSION/ld/Makefile.am && \
  sed -i 's/@AMDEP_TRUE@@am__include@ @am__quote@.\/\$(DEPDIR)\/eelf_x86_64.Pc@am__quote@/&\n@AMDEP_TRUE@@am__include@ @am__quote@.\/\$(DEPDIR)\/eelf_x86_64_'"$OS_NAME"'.Pc@am__quote@/1' binutils-$BINUTILS_VERSION/ld/Makefile.am && \
  sed -i 's/ALL_EMULATION_SOURCES = \\/&\n  eelf_i386_'"$OS_NAME"'.c\\/1' binutils-$BINUTILS_VERSION/ld/Makefile.am && \ 
  sed -i 's/ALL_64_EMULATION_SOURCES = \\/&\n  eelf_x86_64_'"$OS_NAME"'.c\\/1' binutils-$BINUTILS_VERSION/ld/Makefile.am

# build binutils
RUN \
  export PATH=/root/bin/automake-1.15.1/bin:/root/bin/autoconf-2.69/bin:$PATH && \
  cd binutils-$BINUTILS_VERSION/ld && automake && cd ../../ && \
  mkdir $BUILD/binutils-$OS_NAME-$BINUTILS_VERSION && cd $BUILD/binutils-$OS_NAME-$BINUTILS_VERSION && \
  $SRC/binutils-$BINUTILS_VERSION/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot=$SYSROOT --disable-werror && \
  make && make install

COPY ./toolchain/$OS_NAME.h ./

# configure gcc to be used with our $OS_NAME
RUN \
  export PATH=/root/bin/automake-1.15.1/bin:/root/bin/autoconf-2.69/bin:$PATH && \
  sed -i '1720,+10s/gnu\* |/& '"$OS_NAME"'* |/' gcc-$GCC_VERSION/config.sub && \
  sed -i '673,+10s/case ${target} in/&\n*-*-'"$OS_NAME"'*)\n  gas=yes\n  gnu_ld=yes\n  default_use_cxa_atexit=yes\n  use_gcc_stdint=provide\n  ;;/' gcc-$GCC_VERSION/gcc/config.gcc && \
  sed -i 's/aarch64\*-\*-elf | aarch64\*-\*-fuchsia\* | aarch64\*-\*-rtems\*)/i[34567]86-*-'"$OS_NAME"'*)\n  tm_file="\${tm_file} i386\/unix.h i386\/att.h dbxelf.h elfos.h glibc-stdint.h i386\/i386elf.h '"$OS_NAME"'.h"\n  ;;\nx86_64-\*-'"$OS_NAME"'\*)\n  tm_file="\${tm_file} i386\/unix.h i386\/att.h dbxelf.h elfos.h glibc-stdint.h i386\/i386elf.h i386\/x86-64.h '"$OS_NAME"'.h"\n  ;;\n&/' gcc-$GCC_VERSION/gcc/config.gcc && \
  touch gcc-$GCC_VERSION/gcc/config/$OS_NAME.h && cat $OS_NAME.h > gcc-$GCC_VERSION/gcc/config/$OS_NAME.h && \
  sed -i '377,+10s/case ${host} in/&\ni[34567]86-\*-'"$OS_NAME"'\*)\n  extra_parts=\"\$extra_parts crti.o crtbegin.o crtend.o crtn.o\"\n  tmake_file="\$tmake_file i386\/t-crtstuff t-crtstuff-pic t-libgcc-pic"\n  ;;\nx86_64-\*-'"$OS_NAME"'\*)\n  extra_parts=\"\$extra_parts crti.o crtbegin.o crtend.o crtn.o\"\n  tmake_file=\"\$tmake_file i386\/t-crtstuff t-crtstuff-pic t-libgcc-pic"\n  ;;/' gcc-$GCC_VERSION/libgcc/config.host && \
  sed -i 's/case \$machine in/&\n    \*-'"$OS_NAME"'\* | \\\n    \*-\*-'"$OS_NAME"'\* | \\/' gcc-$GCC_VERSION/fixincludes/mkfixinc.sh

# build gcc
RUN \
  export PATH=/root/bin/automake-1.15.1/bin:/root/bin/autoconf-2.69/bin:$PATH && \
  mkdir $BUILD/gcc-$OS_NAME-$GCC_VERSION && cd $BUILD/gcc-$OS_NAME-$GCC_VERSION &&\
  $SRC/gcc-$GCC_VERSION/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot=$SYSROOT --enable-languages=c && \
  make all-gcc && \
  make all-target-libgcc && \
  make install-gcc && \   
  make install-target-libgcc

RUN mkdir newlib-$NEWLIB_VERSION/newlib/libc/sys/$OS_NAME
COPY ./ports/newlib/$OS_NAME newlib-$NEWLIB_VERSION/newlib/libc/sys/$OS_NAME

# configure newlib
# add $OS_NAME to config files
# source: https://wiki.osdev.org/Porting_Newlib

#  -DHAVE_OPENDIR
RUN \
  sed -i '1375,+10s/-gnu\* |/& -'"$OS_NAME"'* |/' newlib-$NEWLIB_VERSION/config.sub && \
  sed -i 's/case "${host_cpu}" in/&\ni[3-7]86-*-'"$OS_NAME"'*)\nsys_dir='"$OS_NAME"';;/' newlib-$NEWLIB_VERSION/newlib/configure.host && \
  echo "sys_dir='"$OS_NAME"'\nnewlib_cflags='${newlib_cflags} -DMISSING_SYSCALL_NAMES -DHAVE_OPENDIR'" >> newlib-$NEWLIB_VERSION/newlib/configure.host && \
  sed -i 's/case ${sys_dir} in/&\n'"$OS_NAME"') AC_CONFIG_SUBDIRS('"$OS_NAME"') ;;/' newlib-$NEWLIB_VERSION/newlib/libc/sys/configure.in

COPY ./toolchain/build_libc.sh ./
COPY ./toolchain/configure_libc.sh ./
COPY ./toolchain/libc.sh ./

RUN \
  chmod +x ./build_libc.sh && \
  chmod +x ./configure_libc.sh && \
  chmod +x ./libc.sh

# build newlib with our customized gcc
RUN \
  sh ./libc.sh rebuild && sh ./libc.sh install


# ./configure --bindir=$PWD/bin --mandir=$PWD/man --host=i686-myos CPPFLAGS=-DHAVE_KILLPG=1
# dash port
ENV DASH=dash-0.5.12
WORKDIR $SRC
RUN \
  wget http://gondor.apana.org.au/\~herbert/dash/files/$DASH.tar.gz && \
  tar xvf $DASH.tar.gz && \
  rm -rf $DASH.tar.gz && \
  cd $DASH && \
  ./configure --bindir=$BUILD/$DASH/bin --mandir=$BUILD/$DASH/man --host=i686-myos CPPFLAGS=-DHAVE_KILLPG=1 && \
  make && make install

RUN apt install git -y

# figlet port
ENV FIGLET=figlet
RUN \
  git clone https://github.com/cmatsuoka/$FIGLET && \
  cd $FIGLET && \
  sed -i 's/gcc/i686-myos-gcc/g' Makefile #&& \
  # fix not found arpa/inet.h error
  sed -i 's/include <arpa\/inet.h>/define htonl(l) ((((l)\&0xFF) << 24) | (((l)\&0xFF00) << 8) | (((l)\&0xFF0000) >> 8) | (((l)\&0xFF000000) >> 24))/g' utf8.c && \
  make prefix $BUILD/$FIGLET

VOLUME /os
#VOLUME /os/sysroot
VOLUME $BUILD

WORKDIR /os

CMD bash