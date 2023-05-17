mkdir -p $(pwd)/install
./config shared no-asm --prefix=$(pwd)/install CROSS_COMPILE=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
#配置成功后生成Makefile，手动删除Makefile中"-m64"
make -j4 V=s
make install

