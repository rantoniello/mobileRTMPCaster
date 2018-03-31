include ../../config.common.mk
include ../../config.android-x86.mk

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
			--extra-cflags="-I${INSTALL_DIR}/include -I${INSTALL_DIR}/include -I$(BUILD_DIR)/../../libfreetype/build_dir" \
			--extra-ldflags="-L${INSTALL_DIR}/lib" \
			--extra-cxxflags="-I${INSTALL_DIR}/include -I${INSTALL_DIR}/include" \
			--yasmexe=${INSTALL_DIR}/bin/yasm \
			--enable-gpl --enable-libx264 --enable-encoder=aac --enable-version3 --enable-libvo-aacenc \
			--disable-ffprobe --disable-ffserver --disable-static \
			--enable-libmp3lame --enable-ffplay
CONFIGURE_PARAMS+= --enable-shared

all: install

config:
	export
	mkdir -p $(INSTALL_DIR)
	mkdir -p $(INSTALL_DIR)/include
	mkdir -p $(BUILD_DIR)
	if [ ! -f $(BUILD_DIR)/Makefile ] ; then cd $(BUILD_DIR); $(BUILD_DIR)/../../../3rdplibs/ffmpeg/ffmpeg-2.4.2/configure $(CONFIGURE_PARAMS); fi

build: config
	cd $(BUILD_DIR); make

install: build
	cd $(BUILD_DIR); make install

clean:
	rm -fr $(BUILD_DIR)

