#!/bin/sh
mkdir -p $(pwd)/libnl

./configure CC=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc \
    --prefix=$(pwd)/libnl \
    --host=arm-linux-gnueabihf \
    CFLAGS=-I/usr/include/libnl3

    make -j4 V=s  && make install
