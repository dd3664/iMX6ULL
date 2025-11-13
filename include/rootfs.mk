STAGING_ROOTFS:=$(STAGING_DIR)/rootfs
ROOTFS_DIR:=$(BUILD_DIR)/rootfs
EXT_ROOTFS_DIR:=$(TOPDIR)/rootfs

staging_rootfs:
	[ -d $(STAGING_ROOTFS) ] || mkdir -p $(STAGING_ROOTFS)
	[ -d $(STAGING_ROOTFS)/dev ] || mkdir -p $(STAGING_ROOTFS)/dev
	[ -d $(STAGING_ROOTFS)/etc ] || mkdir -p $(STAGING_ROOTFS)/etc
	[ -d $(STAGING_ROOTFS)/lib ] || mkdir -p $(STAGING_ROOTFS)/lib
	[ -d $(STAGING_ROOTFS)/mnt ] || mkdir -p $(STAGING_ROOTFS)/mnt
	[ -d $(STAGING_ROOTFS)/proc ] || mkdir -p $(STAGING_ROOTFS)/proc
	[ -d $(STAGING_ROOTFS)/root ] || mkdir -p $(STAGING_ROOTFS)/root
	[ -d $(STAGING_ROOTFS)/sys ] || mkdir -p $(STAGING_ROOTFS)/sys
	[ -d $(STAGING_ROOTFS)/tmp ] || mkdir -p $(STAGING_ROOTFS)/tmp
	[ -d $(STAGING_ROOTFS)/usr/lib ] || mkdir -p $(STAGING_ROOTFS)/usr/lib

	cp -d $(TOOLCHAIN_STAGING_DIR)/$(TOOLCHAIN)/arm-linux-gnueabihf/libc/lib/*so* $(STAGING_ROOTFS)/lib/
	cp -d $(TOOLCHAIN_STAGING_DIR)/$(TOOLCHAIN)/arm-linux-gnueabihf/libc/lib/*.a $(STAGING_ROOTFS)/lib/
	rm $(STAGING_ROOTFS)/lib/ld-linux-armhf.so.3 && cp $(TOOLCHAIN_STAGING_DIR)/$(TOOLCHAIN)/arm-linux-gnueabihf/libc/lib/ld-linux-armhf.so.3 $(STAGING_ROOTFS)/lib/
	cp -d $(TOOLCHAIN_STAGING_DIR)/$(TOOLCHAIN)/arm-linux-gnueabihf/lib/*so* $(STAGING_ROOTFS)/lib
	cp -d $(TOOLCHAIN_STAGING_DIR)/$(TOOLCHAIN)/arm-linux-gnueabihf/lib/*.a $(STAGING_ROOTFS)/lib
	cp -d $(TOOLCHAIN_STAGING_DIR)/$(TOOLCHAIN)/arm-linux-gnueabihf/libc/usr/lib/*so* $(STAGING_ROOTFS)/usr/lib
	cp -d $(TOOLCHAIN_STAGING_DIR)/$(TOOLCHAIN)/arm-linux-gnueabihf/libc/usr/lib/*.a $(STAGING_ROOTFS)/usr/lib

rootfs: rootfs-install

rootfs-prepare:
	[ -d $(ROOTFS_DIR) ] || mkdir -p $(ROOTFS_DIR)
rootfs-install: rootfs-prepare
	cp -rf $(STAGING_ROOTFS)/* $(ROOTFS_DIR)/	
	cp -rf $(EXT_ROOTFS_DIR)/* $(ROOTFS_DIR)/
	tar -cjf $(OUTPUT_DIR)/rootfs.tar.bz2 -C $(ROOTFS_DIR) ./
rootfs-clean:
	rm -rf $(ROOTFS_DIR)
	rm -rf $(OUTPUT_DIR)/rootfs.tar.bz2
rootfs-rebuild: rootfs-clean rootfs-install
