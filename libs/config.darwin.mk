INSTALL_DIR = $(CONFIG_LINUX_MK_PATH)/_install_dir_$(CROSS_ARCH)_darwin
PLATFORMS := /Applications/Xcode.app/Contents/Developer/Platforms
MIPHONEOS_VERSION_MIN := 6.1

CROSS_PLATFORM=apple-darwin
#CROSS_ARCH=armv7 # NOTE: passed as argument (using target) in Makefile call

ifeq ($(LBITS),64)
# 64 bit stuff here
	ifneq (,$(findstring 86,$(CROSS_ARCH)))
		PLATFORM := $(PLATFORMS)/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk
		GCC := $(PLATFORMS)/iPhoneSimulator.platform/Developer/usr/bin/gcc
		CC := $(GCC)
		CC+= -arch $(CROSS_ARCH) -miphoneos-version-min=$(MIPHONEOS_VERSION_MIN)
		LDFLAGS+= -arch $(CROSS_ARCH) -isysroot $(PLATFORM)  -lm -L$(INSTALL_DIR)/lib -L$(PLATFORMS)/iPhoneSimulator.platform/Developer/usr/lib -lSystem
	else
		PLATFORM := $(PLATFORMS)/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk
		GCC := $(PLATFORMS)/../Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
		CC := $(GCC)
		CC+= -arch $(CROSS_ARCH) -isysroot $(PLATFORM) -miphoneos-version-min=$(MIPHONEOS_VERSION_MIN)
		CPP := $(PLATFORMS)/../Toolchains/XcodeDefault.xctoolchain/usr/bin/cpp
		LDFLAGS+= -arch $(CROSS_ARCH) -isysroot $(PLATFORM)
	endif
else
# 32 bit
endif

CFLAGS+= -isysroot $(PLATFORM)
