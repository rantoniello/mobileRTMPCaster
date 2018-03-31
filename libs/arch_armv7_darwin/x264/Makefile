include ../../config.common.mk
include ../../config.darwin.mk

BUILD_DIR := "$(shell pwd)/build_dir"
CROSS_COMPILE=$(PLATFORMS)/../Toolchains/XcodeDefault.xctoolchain/usr/bin/

export PATH := $(INSTALL_DIR)/bin:$(PATH)
export PKG_CONFIG_P/Users/ral/workspace/rtmpclient/libs/arch_armv7_darwin/ffmpeg/MakefileATH :=$(INSTALL_DIR)/lib/pkgconfig:$(PKG_CONFIG_PATH)
export LD_LIBRARY_PATH :=$(INSTALL_DIR)/lib:$(LD_LIBRARY_PATH)
export C_INCLUDE_PATH :=$(INSTALL_DIR)/include:$(C_INCLUDE_PATH)
export CPLUS_INCLUDE_PATH :=$(INSTALL_DIR)/include:$(CPLUS_INCLUDE_PATH)
export CFLAGS
export LDFLAGS
export CC
export CPP
export NM="${CROSS_COMPILE}nm"
export STRIP="${CROSS_COMPILE}strip"
export RANLIB="${CROSS_COMPILE}ranlib"
export AR="${CROSS_COMPILE}ar"

CONFIGURE_PARAMS= --prefix=${INSTALL_DIR} \
--host=$(CROSS_ARCH)-$(CROSS_PLATFORM) \
--enable-static \
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
	mkdir -p $(BUILD_DIR)/extras
	cp -a $(CONFIG_LINUX_MK_PATH)/gas-preprocessor.pl $(INSTALL_DIR)/bin
	cp -a $(CONFIG_LINUX_MK_PATH)/gas-preprocessor.pl $(BUILD_DIR)/extras
	if [ ! -f $(BUILD_DIR)/x264_config.h ] ; then \
		cd $(BUILD_DIR); \
		$(BUILD_DIR)/../../../3rdplibs/x264/x264-git/configure $(CONFIGURE_PARAMS); \
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
