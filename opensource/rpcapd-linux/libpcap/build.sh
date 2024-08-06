#!/bin/sh
CROSS=arm-linux-gnueabihf

./configure --host=$CROSS --with-pcap=linux CC=$CROSS-gcc CXX=$CROSS-g++ AR=$CROSS-ar LD=$CROSS-ld RANLIB=$CROSS-ranlib STRIP=$CROSS-stip
make V=s -j12
