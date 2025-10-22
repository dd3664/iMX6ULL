include common.mk

TOPDIR:=$(shell pwd)/..
BUILD_DIR:=$(TOPDIR)/build_dir
OUTPUT_DIR:=$(TOPDIR)/output
TOOLCHAIN_SOURCE_DIR:=$(TOPDIR)/toolchains
TOOLCHAIN:=gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf
TOOLCHAIN_STAGING_DIR:=$(BUILD_DIR)/toolchain
TARGET:=arm-linux-gnueabihf
TARGET_CROSS:=$(TARGET)-
ARCH:=arm
CROSS_MAKE:=make ARCH=$(ARCH) CROSS_COMPILE=$(TARGET_CROSS)
MAKE:=$(CROSS_MAKE)

export PATH:=$(PATH):$(TOOLCHAIN_STAGING_DIR)/$(TOOLCHAIN)/bin

all: prepare bsp opensource rootfs

include bsp.mk
include rootfs.mk
include opensource.mk

prepare:
	[ -d $(BUILD_DIR) ] || mkdir -p $(BUILD_DIR)
	[ -d $(TOOLCHAIN_STAGING_DIR) ] || (mkdir -p $(TOOLCHAIN_STAGING_DIR) && tar -xf $(TOOLCHAIN_SOURCE_DIR)/$(TOOLCHAIN).tar.xz -C $(TOOLCHAIN_STAGING_DIR))

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all

