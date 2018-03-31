include ../../config.common.mk
include ../../config.android-arm.mk

BUILD_DIR := "$(shell pwd)/build_dir"

CONFIGURE_PARAMS= --prefix=${INSTALL_DIR} \
--enable-shared \
--host=arm-android-linux \
--cross-prefix=$(PREBUILT)/bin/arm-linux-androideabi- \
--sysroot=$(PLATFORM) \
--extra-cflags=-fPIC -fpic\
--enable-pic

all: install

config:
	mkdir -p $(INSTALL_DIR)
	mkdir -p $(INSTALL_DIR)/bin
	mkdir -p $(INSTALL_DIR)/lib
	mkdir -p $(INSTALL_DIR)/lib/pkgconfig
	mkdir -p $(INSTALL_DIR)/include
	mkdir -p $(BUILD_DIR)
	if [ ! -f $(BUILD_DIR)/x264_config.h ] ; then \
		cd $(BUILD_DIR); \
		$(BUILD_DIR)/../../../3rdplibs/x264/x264-git/configure-android $(CONFIGURE_PARAMS); \
	fi

build: config
	cd $(BUILD_DIR); make

install: build
	cd $(BUILD_DIR); make install;

clean:
	if [ -f $(BUILD_DIR)/x264_config.h ] ; then \
		cd $(BUILD_DIR); make clean; make distclean; \
	fi
	rm -fr $(BUILD_DIR)
