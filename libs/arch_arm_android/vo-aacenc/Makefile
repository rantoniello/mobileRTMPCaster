include ../../config.common.mk
include ../../config.android-arm.mk

BUILD_DIR := "$(shell pwd)/build_dir"

export PATH := $(INSTALL_DIR)/bin:$(PATH)
export PKG_CONFIG_PATH :=$(INSTALL_DIR)/lib/pkgconfig:$(PKG_CONFIG_PATH)
export LD_LIBRARY_PATH :=$(INSTALL_DIR)/lib:$(LD_LIBRARY_PATH)
export C_INCLUDE_PATH :=$(INSTALL_DIR)/include:$(C_INCLUDE_PATH)
export CPLUS_INCLUDE_PATH :=$(INSTALL_DIR)/include:$(CPLUS_INCLUDE_PATH)

    CROSS_COMPILE=$(PREBUILT)/bin/arm-linux-androideabi-
    CFLAGS=$(OPTIMIZE_CFLAGS)
    export CXX=$(CROSS_COMPILE)g++ --sysroot=$(PLATFORM)
    export CC=$(CROSS_COMPILE)gcc --sysroot=$(PLATFORM)
    export NM="${CROSS_COMPILE}nm"
    export STRIP="${CROSS_COMPILE}strip"
    export RANLIB="${CROSS_COMPILE}ranlib"
    export AR="${CROSS_COMPILE}ar"

CONFIGURE_PARAMS= --prefix=${INSTALL_DIR} \
--disable-static \
--enable-shared=yes \
--host=arm-linux \
--disable-dependency-tracking \
--with-pic

all: install

config:
	mkdir -p $(INSTALL_DIR)
	mkdir -p $(INSTALL_DIR)/include
	mkdir -p $(INSTALL_DIR)/lib
	mkdir -p $(BUILD_DIR)
	if [ ! -f $(BUILD_DIR)/Makefile ] ; then cd $(BUILD_DIR); \
		$(BUILD_DIR)/../../../3rdplibs/vo-aacenc/vo-aacenc-0.1.3/configure-android $(CONFIGURE_PARAMS); \
	fi

build: config
	cd $(BUILD_DIR); make

install: build
	cd $(BUILD_DIR); make install

clean:
	rm -fr $(BUILD_DIR)
