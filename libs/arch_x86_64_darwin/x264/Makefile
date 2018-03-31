include ../../config.common.mk
include ../../config.darwin.mk

BUILD_DIR := "$(shell pwd)/build_dir"

export PATH := $(INSTALL_DIR)/bin:$(PATH)
export PKG_CONFIG_PATH :=$(INSTALL_DIR)/lib/pkgconfig:$(PKG_CONFIG_PATH)
export LD_LIBRARY_PATH :=$(INSTALL_DIR)/lib:$(LD_LIBRARY_PATH)
export C_INCLUDE_PATH :=$(INSTALL_DIR)/include:$(C_INCLUDE_PATH)
export CPLUS_INCLUDE_PATH :=$(INSTALL_DIR)/include:$(CPLUS_INCLUDE_PATH)
export CFLAGS
export LDFLAGS
export CC
export CPP

CONFIGURE_PARAMS= --prefix=${INSTALL_DIR} \
--host=$(CROSS_ARCH)-$(CROSS_PLATFORM) \
--enable-static \
--sysroot=$(PLATFORM) \
--extra-cflags=-fPIC -fpic\
--enable-pic

all: install

config:
	mkdir -p $(INSTALL_DIR)/include
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/extras
	cp -a ../gas-preprocessor.pl $(BUILD_DIR)/extras
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
