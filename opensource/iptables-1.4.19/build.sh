#!/bin/sh
mkdir -p install
./configure --host=arm-linux-gnueabihf --prefix=$(pwd)/install --enable-static --disable-shared
make CC=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc -j4 V=s && make install
