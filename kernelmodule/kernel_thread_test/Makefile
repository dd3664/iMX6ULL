# Makefile for kernel module

# Name of the kernel module
MODULE_NAME := thread_test

# Src
SRC_FILES := 

# Obj
OBJ_FILES := $(patsubst %.c,%.o,$(SRC_FILES))

# Additional C flags (if needed)
EXTRA_CFLAGS :=

# Kernel module object files
obj-m += $(MODULE_NAME).o
$(MODULE_NAME)-objs := $(OBJ_FILES)

# Path to the kernel source directory
KERNEL_SRC := /home/dong/Project/iMX6ULL/kernel/linux-imx-rel_imx_4.1.15_2.1.0_ga

# arch
ARCH := arm

# gcc
CC := /usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc

# ld
LD := /usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-ld

# Build target
all:
	make -C $(KERNEL_SRC) M=$(shell pwd) ARCH=$(ARCH) $(CC) $(LD) modules

clean:
	make -C $(KERNEL_SRC) M=$(shell pwd) clean

install:
	make -C $(KERNEL_SRC) M=$(shell pwd) modules_install
	depmod -a

uninstall:
	rm -f /lib/modules/$(shell uname -r)/kernel/$(MODULE_NAME).ko
	depmod -a

.PHONY: all clean install uninstall

