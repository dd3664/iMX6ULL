#!/bin/sh
cd libpcap && ./build.sh
cd .. 
export CC=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
make V=s
