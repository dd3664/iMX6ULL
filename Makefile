TOPDIR:=$(shell pwd)
BUILD_DIR:=$(TOPDIR)/build_dir
OUTPUT_DIR:=$(TOPDIR)/output
TOOLCHAIN_SOURCE_DIR:=$(TOPDIR)/toolchains
TOOLCHAIN:=gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf
TOOLCHAIN_STAGING_DIR:=$(BUILD_DIR)/toolchain
TOOLS_DIR:=$(TOPDIR)/tools
STAGING_DIR:=$(BUILD_DIR)/staging_dir
TARGET:=arm-linux-gnueabihf
TARGET_CROSS:=$(TARGET)-
ARCH:=arm
CROSS_MAKE:=make ARCH=$(ARCH) CROSS_COMPILE=$(TARGET_CROSS)
MAKE:=$(CROSS_MAKE)

export PATH:=$(PATH):$(TOOLCHAIN_STAGING_DIR)/$(TOOLCHAIN)/bin:$(TOOLS_DIR)


all: prepare staging_rootfs bsp opensource rootfs image

include $(TOPDIR)/include/common.mk
include $(TOPDIR)/include/bsp.mk
include $(TOPDIR)/include/rootfs.mk
include $(TOPDIR)/include/opensource.mk
include $(TOPDIR)/include/image.mk

prepare:
	[ -d $(BUILD_DIR) ] || mkdir -p $(BUILD_DIR)
	[ -d $(TOOLCHAIN_STAGING_DIR) ] || (mkdir -p $(TOOLCHAIN_STAGING_DIR) && tar -xf $(TOOLCHAIN_SOURCE_DIR)/$(TOOLCHAIN).tar.xz -C $(TOOLCHAIN_STAGING_DIR))
	[ -d $(STAGING_DIR) ] || mkdir -p $(STAGING_DIR)

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(OUTPUT_DIR)

.PHONY: all


