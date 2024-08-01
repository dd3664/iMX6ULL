#!/bin/bash
HOST=arm-linux-gnueabihf

rm $(pwd)/install
mkdir $(pwd)/install

autoreconf -i
CFLAGS="-std=c99" ./configure --host=$HOST --prefix=$(pwd)/install CC=$HOST-gcc CXX=$HOST-g++ AR=$HOST-ar LD=$HOST-ld RANLIB=$HOST-ranlib STRIP=$HOST-stip
make -j12 V=s
make install
