IMAGE:=imx6ull.img
IMAGE_DIR:=$(OUTPUT_DIR)/image

#------------------------------分区信息------------------------------------
#----分区序号----起始地址--------大小----------内容------------文件系统----
#		1			0			 10M		 u-boot.imx			无	
#		2		10x1024x1024	 500M			dtb				fat
#		3		600x1024x1024	 200M		   rootfs			ext4
BOOTFS:=imx6ull.boot
ROOTFS:=rootfs.ext4
BOOTFS_OFFSET=10240#单位:KB,即10MB
ROOTFS_OFFSET=614400#单位KB,即600MB
BOOTFS_SIZE=512000#单位:KB,即500MB
BOOTFS_OFFSET_IN_SECTOR=$(shell expr $(BOOTFS_OFFSET) \* 1024 / 512)
BOOTFS_END_IN_SECTOR=$(shell expr $(BOOTFS_OFFSET_IN_SECTOR) + $(BOOTFS_SIZE) \* 1024 / 512 - 1)
ROOTFS_SIZE=204800#单位KB，即200MB
ROOTFS_OFFSET_IN_SECTOR=$(shell expr $(ROOTFS_OFFSET) \* 1024 / 512)
ROOTFS_END_IN_SECTOR=$(shell expr $(ROOTFS_OFFSET_IN_SECTOR) + $(ROOTFS_SIZE) \* 1024 / 512 - 1)

image:
	[ -d $(IMAGE_DIR) ] || mkdir -p $(IMAGE_DIR)

	rm -f $(IMAGE_DIR)/$(BOOTFS)
	mkfs.fat $(IMAGE_DIR)/$(BOOTFS) -C $(BOOTFS_SIZE)
	mcopy -i $(IMAGE_DIR)/$(BOOTFS) $(OUTPUT_DIR)/topeet_emmc_hdmi.dtb ::topeet_emmc_hdmi.dtb
	mcopy -i $(IMAGE_DIR)/$(BOOTFS) $(OUTPUT_DIR)/zImage ::zImage

	rm -f $(IMAGE_DIR)/$(ROOTFS)
	make_ext4fs -L rootfs \
		-l $(ROOTFS_SIZE)KB \
		-b 4096 \
		-m 0 \
		-J \
		$(IMAGE_DIR)/$(ROOTFS) \
		$(ROOTFS_DIR)

	rm -f $(IMAGE_DIR)/$(IMAGE)
	dd if=/dev/zero of=$(IMAGE_DIR)/$(IMAGE) bs=1M count=1024
	parted $(IMAGE_DIR)/$(IMAGE) --script -- mklabel msdos
	parted $(IMAGE_DIR)/$(IMAGE) --script -- mkpart primary fat32 $(BOOTFS_OFFSET_IN_SECTOR)s $(BOOTFS_END_IN_SECTOR)s
	parted $(IMAGE_DIR)/$(IMAGE) --script -- mkpart primary ext4 $(ROOTFS_OFFSET_IN_SECTOR)s $(ROOTFS_END_IN_SECTOR)s
	dd bs=512 if=$(OUTPUT_DIR)/u-boot.imx of=$(IMAGE_DIR)/$(IMAGE) seek=2 conv=fsync
	dd bs=512 if=$(IMAGE_DIR)/$(BOOTFS) of=$(IMAGE_DIR)/$(IMAGE) seek=$(BOOTFS_OFFSET_IN_SECTOR) conv=fsync
	dd bs=512 if=$(IMAGE_DIR)/$(ROOTFS) of=$(IMAGE_DIR)/$(IMAGE) seek=$(ROOTFS_OFFSET_IN_SECTOR) conv=fsync

