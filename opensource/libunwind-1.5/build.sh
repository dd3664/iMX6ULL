#!/bin/bash
CROSS=arm-linux-gnueabihf

rm $(pwd)/install
mkdir $(pwd)/install

autoreconf -i
CFLAGS="-std=c99" ./configure --host=$CROSS --prefix=$(pwd)/install CC=$CROSS-gcc CXX=$CROSS-g++ AR=$CROSS-ar LD=$CROSS-ld RANLIB=$CROSS-ranlib STRIP=$CROSS-stip
make -j12 V=s
make install
