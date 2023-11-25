#Building environment for our OS
FROM ubuntu:18.04

ENV OS_NAME=myos
ENV PREFIX="$HOME/opt/cross"
ENV OLD_TARGET=i686-elf
ENV TARGET=i686-$OS_NAME
ENV PATH="$PREFIX/bin:$PATH"
ENV BINUTILS_VERSION="2.39"
ENV GCC_VERSION="12.2.0"

# switch betweeen versions
# export PATH=/root/bin/automake-1.15.1/bin:/root/bin/autoconf-2.69/bin:$PATH
# export PATH=/root/bin/automake-1.11/bin:/root/bin/autoconf-2.65/bin:$PATH

RUN \
  apt update && \
  apt install wget -y && \
  apt install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo libisl-0.18-dev libcloog-isl-dev -y

WORKDIR $HOME/src
RUN \
  wget -c https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.gz -O - | tar -xz && \
  wget -c https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz -O - | tar -xz

COPY ./toolchain/build_binutils.sh ./
COPY ./toolchain/build_gcc.sh ./

RUN \
  chmod +x ./build_binutils.sh && \
  chmod +x ./build_gcc.sh

RUN sh ./build_binutils.sh
RUN sh ./build_gcc.sh

RUN apt install xorriso grub-pc-bin -y
RUN apt install nano -y

# making preparation to port newlib
RUN wget -c ftp://sourceware.org/pub/newlib/newlib-2.5.0.tar.gz -O - | tar -xz

RUN mkdir /root/bin

# install automake and autoconf
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

# add $OS_NAME to config files
# source: https://wiki.osdev.org/Porting_Newlib
# add $OS_NAME to config files
# source: https://wiki.osdev.org/Porting_Newlib
RUN \
  sed -i '1375,+10s/-gnu\* |/& -'"$OS_NAME"'* |/' newlib-2.5.0/config.sub && \
  sed -i 's/case "${host_cpu}" in/&\ni[3-7]86-*-'"$OS_NAME"'*)\nsys_dir='"$OS_NAME"';;/' newlib-2.5.0/newlib/configure.host && \
  echo "sys_dir='"$OS_NAME"'" >> newlib-2.5.0/newlib/configure.host && \
  sed -i 's/case ${sys_dir} in/&\n'"$OS_NAME"') AC_CONFIG_SUBDIRS('"$OS_NAME"') ;;/' newlib-2.5.0/newlib/libc/sys/configure.in && \
  autoconf --version && cd newlib-2.5.0/newlib/libc/sys && autoconf

RUN mkdir newlib-2.5.0/newlib/libc/sys/$OS_NAME
COPY ./ports/newlib/$OS_NAME newlib-2.5.0/newlib/libc/sys/$OS_NAME

# newlib setup
# make symlinks (a bad hack) to make newlib work
# this is where the bootstrapped generic cross compiler toolchain ($OLD_TARGET-xxx) is installed in,
# change this based on your development environment.
RUN \
  cd /opt/cross/bin/ && \
  ln $OLD_TARGET-ar $TARGET-ar && \
  ln $OLD_TARGET-as $TARGET-as && \
  ln $OLD_TARGET-gcc $TARGET-gcc && \
  ln $OLD_TARGET-gcc $TARGET-cc && \
  ln $OLD_TARGET-ranlib $TARGET-ranlib

ENV SYSROOT=$HOME/os/sysroot

COPY ./toolchain/build_libc.sh ./
RUN chmod +x ./build_libc.sh

COPY ./toolchain/configure_libc.sh ./
RUN chmod +x ./configure_libc.sh
RUN sh ./configure_libc.sh && sh ./build_libc.sh

# remove symlinks
RUN \
  cd /opt/cross/bin/ && \
  rm $TARGET-ar && \
  rm $TARGET-as && \
  rm $TARGET-gcc && \
  rm $TARGET-cc && \
  rm $TARGET-ranlib

# build toolchain
# install automake and autoconf
WORKDIR $HOME/src
ARG AUTOMAKE_VER=1.15.1
ARG AUTOCONF_VER=2.69

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
  sed -i 's/ALL_64_EMULATION_SOURCES = \\/&\n  eelf_x86_64_'"$OS_NAME"'.c\\/1' binutils-$BINUTILS_VERSION/ld/Makefile.am && \ 
  cd binutils-$BINUTILS_VERSION/ld && automake && cd ../../ && \
  mkdir build-binutils-$OS_NAME-$BINUTILS_VERSION && cd build-binutils-$OS_NAME-$BINUTILS_VERSION && \
  ../binutils-$BINUTILS_VERSION/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot=$SYSROOT --disable-werror && \
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
  mkdir build-gcc-$OS_NAME-$GCC_VERSION && cd build-gcc-$OS_NAME-$GCC_VERSION &&\
  ../gcc-$GCC_VERSION/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot=$SYSROOT --enable-languages=c && \
  make all-gcc

RUN \
  export PATH=/root/bin/automake-1.15.1/bin:/root/bin/autoconf-2.69/bin:$PATH && \
  cd build-gcc-$OS_NAME-$GCC_VERSION && \
  make all-target-libgcc && \
  make install-gcc && \   
  make install-target-libgcc

# rebuild newlib with our customized gcc
RUN \
  rm -rf build-newlib && rm -rf $SYSROOT/usr/lib/* && \ 
  sh ./configure_libc.sh && sh ./build_libc.sh

RUN \
  cp -r build-newlib/$TARGET/newlib/libc/sys/$OS_NAME/crt0.o $SYSROOT/usr/lib/

#VOLUME /os
#VOLUME /src/newlib-2.5.0/newlib/libc/sys/$OS_NAME
#VOLUME /src/build-newlib
WORKDIR /os

CMD bash