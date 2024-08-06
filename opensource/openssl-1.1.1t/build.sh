#!/bin/bash
CROSS=arm-linux-gnueabihf-

rm $(pwd)/install
mkdir $(pwd)/install

./config shared no-asm CROSS_COMPILE=$CROSS --prefix=$(pwd)/install
#配置成功后生成Makefile，手动删除Makefile中"-m64"
sed -i '/-m64/d' Makefile

make V=s -j12
make install

