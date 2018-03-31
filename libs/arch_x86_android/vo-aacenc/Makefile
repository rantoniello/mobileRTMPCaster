include ../../config.common.mk
include ../../config.android-x86.mk

BUILD_DIR := "$(shell pwd)/build_dir"

CONFIGURE_PARAMS= --prefix=${INSTALL_DIR} --enable-shared --enable-static=no

all: install

config:
	mkdir -p $(INSTALL_DIR)
	mkdir -p $(INSTALL_DIR)/include
	mkdir -p $(BUILD_DIR)
	if [ ! -f $(BUILD_DIR)/Makefile ] ; then cd $(BUILD_DIR); \
		$(BUILD_DIR)/../../../3rdplibs/vo-aacenc/vo-aacenc-0.1.3/configure $(CONFIGURE_PARAMS); \
	fi

build: config
	cd $(BUILD_DIR); make

install: build
	cd $(BUILD_DIR); make install

clean:
	rm -fr $(BUILD_DIR)
