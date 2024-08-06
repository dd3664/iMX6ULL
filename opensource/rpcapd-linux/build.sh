#!/bin/sh
CROSS=arm-linux-gnueabihf

cd libpcap && ./build.sh
cd .. 
make CC=$CROSS-gcc V=s -j12
