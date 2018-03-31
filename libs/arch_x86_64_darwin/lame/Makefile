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
--enable-shared=no \
--disable-dependency-tracking \
--with-pic \
--disable-frontend \
--host=$(CROSS_ARCH)

all: install

config:
	mkdir -p $(INSTALL_DIR)
	mkdir -p $(INSTALL_DIR)/include
	mkdir -p $(BUILD_DIR)
	if [ ! -f $(BUILD_DIR)/Makefile ] ; then cd $(BUILD_DIR); \
		$(BUILD_DIR)/../../../3rdplibs/lame/lame-3.99.5/configure $(CONFIGURE_PARAMS); \
	fi

build: config
	cd $(BUILD_DIR); make

install: build
	cd $(BUILD_DIR); make install

clean:
	rm -fr $(BUILD_DIR)
