#!/bin/sh
export PKG_CONFIG_PATH=/home/dong/Project/iMX6ULL/opensource/libnl-3.2.25/install/lib/pkgconfig:$PKG_CONFIG_PATH
make CC=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc -j4 V=s

