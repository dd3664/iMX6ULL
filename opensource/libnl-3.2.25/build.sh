#!/bin/bash
CROSS=arm-linux-gnueabihf

rm $(pwd)/install
mkdir $(pwd)/install

./configure --host=$CROSS --prefix=$(pwd)/install CC=$CROSS-gcc CXX=$CROSS-g++ AR=$CROSS-ar LD=$CROSS-ld RANLIB=$CROSS-ranlib STRIP=$CROSS-stip
make V=s -j12
make install
