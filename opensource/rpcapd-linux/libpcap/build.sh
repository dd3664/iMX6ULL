#!/bin/sh
./configure --host=arm-linux-gnueabihf --with-pcap=linux
export CC=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
make V=s
