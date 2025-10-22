OPENSOURCE_SOURCE_DIR:=$(TOPDIR)/opensource
OPENSOURCE_BUILD_DIR:=$(BUILD_DIR)/opensource
BUSYBOX:=busybox-1.29.0
IPTABLES:=iptables-1.4.19
LIBNL:=libnl-3.2.25
OPENSSL:=openssl-1.1.1t
WPA_SUPPLICANT:=wpa_supplicant-2.7
HOSTAPD:=hostapd-2.9
WIRELESS_TOOLS:=wireless_tools.29
LIBUNWIND:=libunwind-1.5
RPCAPD-LINUX:=rpcapd-linux

opensource: opensource_prepare busybox-install iptables-install libnl-install openssl-install wpa_supplicant-install hostapd-install wireless_tools-install libunwind-install rpcapd-linux-install

opensource_prepare:
	[ -d $(OPENSOURCE_BUILD_DIR) ] || mkdir -p $(OPENSOURCE_BUILD_DIR)

busybox-prepare:
	$(call copy_osr_if_not_exit,$(BUSYBOX))
busybox-compile: busybox-prepare
	cd $(OPENSOURCE_BUILD_DIR)/$(BUSYBOX) && \
		cp configs/imx6ull_defconfig .config && \
		$(MAKE) V=s && \
		mkdir -p rootfs && \
		$(MAKE) install
busybox-install: busybox-compile
	cp -rf $(OPENSOURCE_BUILD_DIR)/$(BUSYBOX)/rootfs $(BUILD_DIR)/	
busybox-clean:
	rm -rf $(OPENSOURCE_BUILD_DIR)/$(BUSYBOX)
busybox-rebuild: busybox-clean busybox-install

iptables-prepare:
	$(call copy_osr_if_not_exit,$(IPTABLES))
iptables-compile: iptables-prepare
	cd $(OPENSOURCE_BUILD_DIR)/$(IPTABLES) && \
		[ -d install ] || mkdir install && \
		./autogen.sh && \
		./configure --host=$(TARGET) --prefix=$(OPENSOURCE_BUILD_DIR)/$(IPTABLES)/install CC=$(TARGET_CROSS)gcc CXX=$(TARGET_CROSS)g++ AR=$(TARGET_CROSS)ar LD=$(TARGET_CROSS)ld RANLIB=$(TARGET_CROSS)ranlib STRIP=$(TARGET_CROSS)stip --enable-static --disable-shared && \
		$(MAKE)  V=s && \
		$(MAKE) install
iptables-install: iptables-compile
	cp -rf $(OPENSOURCE_BUILD_DIR)/$(IPTABLES)/install/sbin/* $(ROOTFS_DIR)/sbin/
iptables-clean:
	rm -rf $(OPENSOURCE_BUILD_DIR)/$(IPTABLES)
iptables-rebuild: iptables-clean iptables-install

libnl-prepare:
	$(call copy_osr_if_not_exit,$(LIBNL))
libnl-compile: libnl-prepare
	cd $(OPENSOURCE_BUILD_DIR)/$(LIBNL) && \
		[ -d install ] || mkdir install && \
		./configure --host=$(TARGET) --prefix=$(OPENSOURCE_BUILD_DIR)/$(LIBNL)/install CC=$(TARGET_CROSS)gcc CXX=$(TARGET_CROSS)g++ AR=$(TARGET_CROSS)ar LD=$(TARGET_CROSS)ld RANLIB=$(TARGET_CROSS)ranlib STRIP=$(TARGET_CROSS)stip && \
		$(MAKE) V=s && \
		$(MAKE) install
libnl-install: libnl-compile
	cp -rf $(OPENSOURCE_BUILD_DIR)/$(LIBNL)/install/lib/* $(ROOTFS_DIR)/lib/
libnl-clean:
	rm -rf $(OPENSOURCE_BUILD_DIR)/$(LIBNL)
libnl-rebuild: libnl-clean libnl-install

openssl-prepare:
	$(call copy_osr_if_not_exit,$(OPENSSL))
openssl-compile: openssl-prepare
	cd $(OPENSOURCE_BUILD_DIR)/$(OPENSSL) && \
		[ -d install ] || mkdir install && \
		./config shared no-asm CROSS_COMPILE=$(TARGET_CROSS) --prefix=$(OPENSOURCE_BUILD_DIR)/$(OPENSSL)/install && \
		sed -i '/-m64/d' Makefile && \
		$(MAKE) V=s && \
		$(MAKE) install
openssl-install: openssl-compile
	cp $(OPENSOURCE_BUILD_DIR)/$(OPENSSL)/install/bin/openssl $(ROOTFS_DIR)/lib/
	cp -rf $(OPENSOURCE_BUILD_DIR)/$(OPENSSL)/install/lib/* $(ROOTFS_DIR)/lib/
openssl-clean:
	rm -rf $(OPENSOURCE_BUILD_DIR)/$(OPENSSL)
openssl-rebuild: openssl-clean openssl-install

wpa_supplicant-prepare:
	$(call copy_osr_if_not_exit,$(WPA_SUPPLICANT))
wpa_supplicant-compile: wpa_supplicant-prepare
	export PKG_CONFIG_PATH=$(PKG_CONFIG_PATH):$(OPENSOURCE_BUILD_DIR)/$(LIBNL)/install/lib/pkgconfig && \
		$(MAKE) -C $(OPENSOURCE_BUILD_DIR)/$(WPA_SUPPLICANT)/wpa_supplicant \
		CC=$(TARGET_CROSS)gcc \
		EXTRA_CFLAGS="-I$(OPENSOURCE_BUILD_DIR)/$(LIBNL)/install/include/libnl3 -I$(OPENSOURCE_BUILD_DIR)/$(OPENSSL)/install/include" \
		LDFLAGS="-L$(OPENSOURCE_BUILD_DIR)/$(LIBNL)/install/lib -L$(OPENSOURCE_BUILD_DIR)/$(OPENSSL)/install/lib -lssl -lcrypto"
wpa_supplicant-install: wpa_supplicant-compile
	cp $(OPENSOURCE_BUILD_DIR)/$(WPA_SUPPLICANT)/wpa_supplicant/wpa_supplicant $(ROOTFS_DIR)/sbin/
	cp $(OPENSOURCE_BUILD_DIR)/$(WPA_SUPPLICANT)/wpa_supplicant/wpa_cli $(ROOTFS_DIR)/sbin/
	cp $(OPENSOURCE_BUILD_DIR)/$(WPA_SUPPLICANT)/wpa_supplicant/wpa_passphrase $(ROOTFS_DIR)/sbin/
wpa_supplicant-clean:
	rm -rf $(OPENSOURCE_BUILD_DIR)/$(WPA_SUPPLICANT)
wpa_supplicant-rebuild: wpa_supplicant-clean wpa_supplicant-install

hostapd-prepare:
	$(call copy_osr_if_not_exit,$(HOSTAPD))
hostapd-compile: hostapd-prepare
	export PKG_CONFIG_PATH=$(PKG_CONFIG_PATH):$(OPENSOURCE_BUILD_DIR)/$(LIBNL)/install/lib/pkgconfig && \
		$(MAKE) -C $(OPENSOURCE_BUILD_DIR)/$(HOSTAPD)/hostapd \
		CC=$(TARGET_CROSS)gcc \
		EXTRA_CFLAGS="-I$(OPENSOURCE_BUILD_DIR)/$(LIBNL)/install/include/libnl3 -I$(OPENSOURCE_BUILD_DIR)/$(OPENSSL)/install/include" \
		LDFLAGS="-L$(OPENSOURCE_BUILD_DIR)/$(LIBNL)/install/lib -L$(OPENSOURCE_BUILD_DIR)/$(OPENSSL)/install/lib -lssl -lcrypto"
hostapd-install: hostapd-compile
	cp $(OPENSOURCE_BUILD_DIR)/$(HOSTAPD)/hostapd/hostapd $(ROOTFS_DIR)/sbin/
	cp $(OPENSOURCE_BUILD_DIR)/$(HOSTAPD)/hostapd/hostapd_cli $(ROOTFS_DIR)/sbin/
hostapd-clean:
	rm -rf $(OPENSOURCE_BUILD_DIR)/$(HOSTAPD)
hostapd-rebuild: hostapd-clean hostapd-install

wireless_tools-prepare:
	$(call copy_osr_if_not_exit,$(WIRELESS_TOOLS))
wireless_tools-compile: wireless_tools-prepare
	$(MAKE) -C $(OPENSOURCE_BUILD_DIR)/$(WIRELESS_TOOLS) \
		CC=$(TARGET_CROSS)gcc \
		AR=$(TARGET_CROSS)ar \
		RANLIB=$(TARGET_CROSS)ranlib
wireless_tools-install: wireless_tools-compile
	cp $(OPENSOURCE_BUILD_DIR)/$(WIRELESS_TOOLS)/ifrename $(ROOTFS_DIR)/sbin/
	cp $(OPENSOURCE_BUILD_DIR)/$(WIRELESS_TOOLS)/iwconfig $(ROOTFS_DIR)/sbin/
	cp $(OPENSOURCE_BUILD_DIR)/$(WIRELESS_TOOLS)/iwevent $(ROOTFS_DIR)/sbin/
	cp $(OPENSOURCE_BUILD_DIR)/$(WIRELESS_TOOLS)/iwgetid $(ROOTFS_DIR)/sbin/
	cp $(OPENSOURCE_BUILD_DIR)/$(WIRELESS_TOOLS)/iwlist $(ROOTFS_DIR)/sbin/
	cp $(OPENSOURCE_BUILD_DIR)/$(WIRELESS_TOOLS)/iwpriv $(ROOTFS_DIR)/sbin/
	cp $(OPENSOURCE_BUILD_DIR)/$(WIRELESS_TOOLS)/iwspy $(ROOTFS_DIR)/sbin/
	cp $(OPENSOURCE_BUILD_DIR)/$(WIRELESS_TOOLS)/libiw.so.29 $(ROOTFS_DIR)/lib/
wireless_tools-clean:
	rm -rf $(OPENSOURCE_BUILD_DIR)/$(WIRELESS_TOOLS)
wireless_tools-rebuild: wireless_tools-clean wireless_tools-install

libunwind-prepare:
	$(call copy_osr_if_not_exit,$(LIBUNWIND))
libunwind-compile: libunwind-prepare
	cd $(OPENSOURCE_BUILD_DIR)/$(LIBUNWIND) && \
		[ -d install ] || mkdir install && \
		autoreconf -i && \
		CFLAGS="-std=c99" ./configure --host=$(TARGET) --prefix=$(OPENSOURCE_BUILD_DIR)/$(LIBUNWIND)/install CC=$(TARGET_CROSS)gcc CXX=$(TARGET_CROSS)g++ AR=$(TARGET_CROSS)ar LD=$(TARGET_CROSS)ld RANLIB=$(TARGET_CROSS)ranlib STRIP=$(TARGET_CROSS)stip && \
		$(MAKE) V=s && \
		$(MAKE) install
libunwind-install: libunwind-compile
	cp -rf $(OPENSOURCE_BUILD_DIR)/$(LIBUNWIND)/install/lib/* $(ROOTFS_DIR)/lib/
libunwind-clean:
	rm -rf $(OPENSOURCE_BUILD_DIR)/$(LIBUNWIND)
libunwind-rebuild: libunwind-clean libunwind-install

rpcapd-linux-prepare:
	$(call copy_osr_if_not_exit,$(RPCAPD-LINUX))
rpcapd-linux-compile: rpcapd-linux-prepare
	cd $(OPENSOURCE_BUILD_DIR)/$(RPCAPD-LINUX)/libpcap && \
		./configure --host=$(TARGET) --with-pcap=linux CC=$(TARGET_CROSS)gcc CXX=$(TARGET_CROSS)g++ AR=$(TARGET_CROSS)ar LD=$(TARGET_CROSS)ld RANLIB=$(TARGET_CROSS)ranlib STRIP=$(TARGET_CROSS)stip
	$(MAKE) -C $(OPENSOURCE_BUILD_DIR)/$(RPCAPD-LINUX)/libpcap
	$(MAKE) -C $(OPENSOURCE_BUILD_DIR)/$(RPCAPD-LINUX) CC=$(TARGET_CROSS)gcc
rpcapd-linux-install: rpcapd-linux-compile
	cp $(OPENSOURCE_BUILD_DIR)/$(RPCAPD-LINUX)/rpcapd $(ROOTFS_DIR)/sbin/
rpcapd-linux-clean:
	rm -rf $(OPENSOURCE_BUILD_DIR)/$(RPCAPD-LIBNL)
rpcapd-linux-rebuild: rpcapd-linux-clean rpcapd-linux-install	
