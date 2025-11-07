U-BOOT:=uboot-imx-rel_imx_4.1.15_2.1.0_ga
LINUX_KERNEL:=linux-imx-rel_imx_4.1.15_2.1.0_ga

bsp: u-boot-install kernel-install

u-boot-prepare:
	[ -d $(BUILD_DIR)/$(U-BOOT) ] || cp -rf $(TOPDIR)/uboot/$(U-BOOT) $(BUILD_DIR)/
u-boot-compile: u-boot-prepare
	cd $(BUILD_DIR)/$(U-BOOT) && \
		$(MAKE) mx6ul_topeet_emmc_defconfig && \
		$(MAKE)
u-boot-install: u-boot-compile
	mkdir -p $(OUTPUT_DIR)	
	cp $(BUILD_DIR)/$(U-BOOT)/u-boot.imx $(OUTPUT_DIR)/
u-boot-clean:
	rm -rf $(BUILD_DIR)/$(U-BOOT)	
u-boot-rebuild: u-boot-clean u-boot-install
	

kernel-prepare:
	[ -d $(BUILD_DIR)/$(LINUX_KERNEL) ] || cp -rf $(TOPDIR)/kernel/$(LINUX_KERNEL) $(BUILD_DIR)/
kernel-compile: kernel-prepare
	cd $(BUILD_DIR)/$(LINUX_KERNEL) && \
		$(MAKE) imx_v7_defconfig && \
		$(MAKE) uImage LOADADDR=0x10008000 -j8 && \
		$(MAKE) modules && \
		$(MAKE) topeet_emmc_4_3.dtb && \
		$(MAKE) topeet_emmc_5_0.dtb && \
		$(MAKE) topeet_emmc_7_0.dtb && \
		$(MAKE) topeet_emmc_1024x600.dtb && \
		$(MAKE) topeet_emmc_9_7.dtb && \
		$(MAKE) topeet_emmc_10_1.dtb && \
		$(MAKE) topeet_emmc_hdmi.dtb

kernel-install: kernel-compile
	mkdir -p $(OUTPUT_DIR)
	cp $(BUILD_DIR)/$(LINUX_KERNEL)/arch/arm/boot/zImage $(OUTPUT_DIR)/
	cp $(BUILD_DIR)/$(LINUX_KERNEL)/arch/arm/boot/dts/topeet_emmc*.dtb $(OUTPUT_DIR)/
kernel-clean:
	rm -rf $(BUILD_DIR)/$(LINUX_KERNEL)
kernel-rebuild:	kernel-clean kernel-install 
