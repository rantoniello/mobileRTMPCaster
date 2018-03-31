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
    export AS_PATH := $(INSTALL_DIR)/bin
#    export LDFLAGS="-Wl,-rpath-link=$(PLATFORM)/usr/lib -L$(PLATFORM)/usr/lib -nostdlib -lc -lm -ldl -llog"

CONFIGURE_PARAMS= --prefix=${INSTALL_DIR} \
--enable-shared \
--host=i686-android-linux \
--cross-prefix=$(PREBUILT)/bin/i686-linux-android- \
--sysroot=$(PLATFORM) \
--disable-asm \
--extra-cflags=-fPIC \
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
