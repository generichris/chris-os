#!/bin/bash

set -e

export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

echo "=== Installing dependencies ==="
apt update
apt install -y build-essential bison flex libgmp-dev libmpc-dev libmpfr-dev texinfo wget

echo "=== Creating cross-compiler directory ==="
mkdir -p $HOME/opt/cross

echo "=== Building binutils ==="
cd /tmp
wget -nc https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.gz
tar -xf binutils-2.41.tar.gz

mkdir -p build-binutils
cd build-binutils
../binutils-2.41/configure --target=$TARGET --prefix=$PREFIX --with-sysroot --disable-nls --disable-werror
make -j$(nproc)
make install

echo "=== Building GCC ==="
cd /tmp
wget -nc https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.gz
tar -xf gcc-13.2.0.tar.gz

mkdir -p build-gcc
cd build-gcc
../gcc-13.2.0/configure --target=$TARGET --prefix=$PREFIX --disable-nls --enable-languages=c --without-headers
make all-gcc -j$(nproc)
make all-target-libgcc -j$(nproc)
make install-gcc
make install-target-libgcc

echo "=== Adding toolchain to PATH ==="
echo 'export PATH="$HOME/opt/cross/bin:$PATH"' >> ~/.bashrc

echo "=== Toolchain build complete! ==="
echo "Compiler: $(which i686-elf-gcc)"
i686-elf-gcc --version
