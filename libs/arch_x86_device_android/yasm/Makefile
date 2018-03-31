include ../../config.common.mk
include ../../config.android-x86-device.mk

BUILD_DIR := "$(shell pwd)/build_dir"

export PATH := $(INSTALL_DIR)/bin:$(PATH)
export PKG_CONFIG_PATH :=$(INSTALL_DIR)/lib/pkgconfig:$(PKG_CONFIG_PATH)
export LD_LIBRARY_PATH :=$(INSTALL_DIR)/lib:$(LD_LIBRARY_PATH)
export C_INCLUDE_PATH :=$(INSTALL_DIR)/include:$(C_INCLUDE_PATH)
export CPLUS_INCLUDE_PATH :=$(INSTALL_DIR)/include:$(CPLUS_INCLUDE_PATH)

    CROSS_COMPILE=$(PREBUILT)/bin/i686-linux-android-
    CFLAGS=$(OPTIMIZE_CFLAGS)
    export CXX=$(CROSS_COMPILE)g++ --sysroot=$(PLATFORM)
    export CC=$(CROSS_COMPILE)gcc --sysroot=$(PLATFORM)
    export NM="${CROSS_COMPILE}nm"
    export STRIP="${CROSS_COMPILE}strip"
    export RANLIB="${CROSS_COMPILE}ranlib"
    export AR="${CROSS_COMPILE}ar"

CONFIGURE_PARAMS= --prefix=${INSTALL_DIR} \
--includedir=${INSTALL_DIR}/include --disable-python --enable-static=yes --enable-shared=yes \
--host=i686-android-linux \
--enable-pic

all: install

config:
	mkdir -p $(INSTALL_DIR)
	mkdir -p $(INSTALL_DIR)/include
	mkdir -p $(BUILD_DIR)
	if [ ! -f $(BUILD_DIR)/Makefile ] ; then cd $(BUILD_DIR); $(BUILD_DIR)/../../../3rdplibs/yasm/yasm-1.2.0/configure $(CONFIGURE_PARAMS); fi

build: config
	cd $(BUILD_DIR); make

install: build
	cd $(BUILD_DIR); make install

clean:
	rm -fr $(BUILD_DIR)

