mkdir -p ../libopenssl
./config shared no-asm --prefix=/home/dong//Project/iMX6ULL/opensource/openssl-1.1.1t/libopenssl/ CROSS_COMPILE=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
#配置成功后生成Makefile，手动删除Makefile中"-m64"
make -j4 V=s
make install
