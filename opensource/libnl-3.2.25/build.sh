mkdir -p $(pwd)/install
./configure --host=arm-linux-gnueabihf --prefix=$(pwd)/install CC=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
make -j4
make install
