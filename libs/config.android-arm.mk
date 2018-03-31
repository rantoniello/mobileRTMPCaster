# COMMON PATHS
INSTALL_DIR = "$(CONFIG_LINUX_MK_PATH)/_install_dir_$(CROSS_ARCH)_android"
ifeq ($(LBITS),64)
# 64 bit stuff here
NDK := $(CONFIG_LINUX_MK_PATH)/../android/adt-bundle-linux-x86_64/android-ndk-r10c
PREBUILT := $(NDK)/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64
PLATFORM := $(NDK)/platforms/android-9/arch-arm
else
NDK := $(CONFIG_LINUX_MK_PATH)/../android/adt-bundle-linux-x86/android-ndk-r9c
PREBUILT := $(NDK)/toolchains/arm-linux-androideabi-4.8/prebuilt/linux-x86
PLATFORM := $(NDK)/platforms/android-9/arch-arm
endif

# ******** GLOBAL CONFIGURATIONS ******** #
CC := $(PREBUILT)/bin/arm-linux-androideabi-gcc
GCC:= $(CC)
