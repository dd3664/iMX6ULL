ROOTFS_DIR:=$(BUILD_DIR)/rootfs
EXT_ROOTFS_DIR:=$(TOPDIR)/rootfs

rootfs: rootfs-install

rootfs-prepare: busybox-install
	[ -d $(ROOTFS_DIR)/dev ] || mkdir -p $(ROOTFS_DIR)/dev
	[ -d $(ROOTFS_DIR)/etc ] || mkdir -p $(ROOTFS_DIR)/etc
	[ -d $(ROOTFS_DIR)/lib ] || mkdir -p $(ROOTFS_DIR)/lib
	[ -d $(ROOTFS_DIR)/mnt ] || mkdir -p $(ROOTFS_DIR)/mnt
	[ -d $(ROOTFS_DIR)/proc ] || mkdir -p $(ROOTFS_DIR)/proc
	[ -d $(ROOTFS_DIR)/root ] || mkdir -p $(ROOTFS_DIR)/root
	[ -d $(ROOTFS_DIR)/sys ] || mkdir -p $(ROOTFS_DIR)/sys
	[ -d $(ROOTFS_DIR)/tmp ] || mkdir -p $(ROOTFS_DIR)/tmp 
rootfs-install: rootfs-prepare
	cp -d $(TOOLCHAIN_STAGING_DIR)/$(TOOLCHAIN)/arm-linux-gnueabihf/libc/lib/*so* $(ROOTFS_DIR)/lib/
	cp -d $(TOOLCHAIN_STAGING_DIR)/$(TOOLCHAIN)/arm-linux-gnueabihf/libc/lib/*.a $(ROOTFS_DIR)/lib/
	rm $(ROOTFS_DIR)/lib/ld-linux-armhf.so.3 && cp $(TOOLCHAIN_STAGING_DIR)/$(TOOLCHAIN)/arm-linux-gnueabihf/libc/lib/ld-linux-armhf.so.3 $(ROOTFS_DIR)/lib/
	cp -d $(TOOLCHAIN_STAGING_DIR)/$(TOOLCHAIN)/arm-linux-gnueabihf/lib/*so* $(ROOTFS_DIR)/lib
	cp -d $(TOOLCHAIN_STAGING_DIR)/$(TOOLCHAIN)/arm-linux-gnueabihf/lib/*.a $(ROOTFS_DIR)/lib

	[ -d $(ROOTFS_DIR)/usr/lib ] || mkdir -p $(ROOTFS_DIR)/usr/lib
	cp -d $(TOOLCHAIN_STAGING_DIR)/$(TOOLCHAIN)/arm-linux-gnueabihf/libc/usr/lib/*so* $(ROOTFS_DIR)/usr/lib
	cp -d $(TOOLCHAIN_STAGING_DIR)/$(TOOLCHAIN)/arm-linux-gnueabihf/libc/usr/lib/*.a $(ROOTFS_DIR)/usr/lib

	cp -rf $(EXT_ROOTFS_DIR)/* $(ROOTFS_DIR)/

	tar -cjf $(OUTPUT_DIR)/rootfs.tar.bz2 -C $(ROOTFS_DIR) ./
rootfs-clean:
	rm -rf $(ROOTFS_DIR)
	rm -rf $(OUTPUT_DIR)/rootfs.tar.bz2
rootfs-rebuild: rootfs-clean rootfs-install
