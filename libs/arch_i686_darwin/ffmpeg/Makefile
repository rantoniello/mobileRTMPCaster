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
--target-os=darwin \
--enable-cross-compile \
--enable-runtime-cpudetect \
--disable-shared \
--enable-static \
--arch=$(CROSS_ARCH) \
--sysroot=$(PLATFORM) \
--enable-nonfree \
--enable-version3 \
--enable-gpl \
--disable-doc \
--disable-ffserver \
--disable-ffprobe \
--disable-ffmpeg \
--disable-ffplay \
--enable-demuxer=*mpeg* \
--enable-demuxer=*swf* \
--enable-demuxer=*aac* \
--enable-demuxer=*ac3* \
--enable-demuxer=*adts* \
--enable-demuxer=*flv* \
--enable-demuxer=*pcm* \
--enable-demuxer=*264* \
--enable-demuxer=*vc1* \
--enable-muxer=*mpeg* \
--enable-muxer=*swf* \
--enable-muxer=*aac* \
--enable-muxer=*ac3* \
--enable-muxer=*adts* \
--enable-muxer=*flv* \
--enable-muxer=*pcm* \
--enable-muxer=*264* \
--enable-demuxer=*vc1* \
--enable-encoder=*264* \
--enable-encoder=*flv* \
--enable-encoder=*aac* \
--enable-encoder=*ac3* \
--enable-encoder=*pcm* \
--enable-encoder=*vc1* \
--enable-decoder=*264* \
--enable-decoder=*flv* \
--enable-decoder=*aac* \
--enable-decoder=*ac3* \
--enable-decoder=*pcm* \
--enable-decoder=*vc1* \
--enable-protocol=*rtmp* \
--enable-protocol=*udp* \
--enable-protocol=*tcp* \
--enable-parser=*aac* \
--enable-parser=*ac3* \
--enable-parser=*264* \
--enable-parser=*mpeg* \
--enable-parser=*vc1* \
--disable-encoder=*dvbsub* \
--disable-decoder=*dvbsub* \
--disable-iconv \
--disable-demuxer=matroska \
--disable-muxer=matroska \
--enable-libx264 \
--enable-libvo-aacenc \
--enable-libmp3lame \
--enable-hwaccel=* \
--enable-filter=* \
--extra-cflags="-I$(INSTALL_DIR)/include -arch $(CROSS_ARCH) -miphoneos-version-min=$(MIPHONEOS_VERSION_MIN)" \
--extra-ldflags="-lm -L$(INSTALL_DIR)/lib -arch $(CROSS_ARCH) -isysroot $(PLATFORM) -miphoneos-version-min=$(MIPHONEOS_VERSION_MIN)" \
--disable-asm 
#--cpu=cortex-a9 --enable-pic
#--disable-everything \

all: install

config:
	mkdir -p $(INSTALL_DIR)/include
	mkdir -p $(BUILD_DIR)
	if [ ! -f $(BUILD_DIR)/Makefile ] ; then cd $(BUILD_DIR); $(BUILD_DIR)/../../../3rdplibs/ffmpeg/ffmpeg-2.4.2/configure $(CONFIGURE_PARAMS); fi

build: config
	cd $(BUILD_DIR); make

install: build
	cd $(BUILD_DIR); make install

clean:
	rm -fr $(BUILD_DIR)

