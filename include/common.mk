define copy_osr_if_not_exit
	@if [ -e $(OPENSOURCE_BUILD_DIR)/$(1) ]; then \
		echo ">>>$(OPENSOURCE_BUILD_DIR)/$(1) already exit, skip copy"; \
	else \
		echo ">>>$(OPENSOURCE_BUILD_DIR)/$(1) not found, copy form $(OPENSOURCE_SOURCE_DIR)/$(1)"; \
		cp -rf $(OPENSOURCE_SOURCE_DIR)/$(1) $(OPENSOURCE_BUILD_DIR)/; \
	fi
endef
