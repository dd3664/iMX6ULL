#!/bin/sh
CROSS=arm-linux-gnueabihf

rm $(pwd)/install
mkdir $(pwd)/install

./autogen.sh
./configure --host=$CROSS --prefix=$(pwd)/install CC=$CROSS-gcc CXX=$CROSS-g++ AR=$CROSS-ar LD=$CROSS-ld RANLIB=$CROSS-ranlib STRIP=$CROSS-stip --enable-static --disable-shared
make V=s -j12
make install
