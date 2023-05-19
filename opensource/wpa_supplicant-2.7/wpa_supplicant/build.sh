#!/bin/sh
export PKG_CONFIG_PATH=/home/dong/Project/iMX6ULL/opensource/libnl-3.2.25/install/lib/pkgconfig:$PKG_CONFIG_PATH
make -j4 V=s
