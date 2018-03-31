#ifeq ($(LBITS),64)
## 64 bit stuff here
#INSTALL_DIR = "$(CONFIG_LINUX_MK_PATH)/_install_dir_$(CROSS_ARCH)_android_64"
#NDK := $(CONFIG_LINUX_MK_PATH)/../android/adt-bundle-linux-x86_64/android-ndk-r10c
#PREBUILT := $(NDK)/toolchains/x86-4.9/prebuilt/linux-x86_64
#PLATFORM := $(NDK)/platforms/android-9/arch-x86
#else
INSTALL_DIR = "$(CONFIG_LINUX_MK_PATH)/_install_dir_$(CROSS_ARCH)_android_32"
NDK := $(CONFIG_LINUX_MK_PATH)/../android/adt-bundle-linux-x86/android-ndk-r9c
PREBUILT := $(NDK)/toolchains/x86-4.8/prebuilt/linux-x86
PLATFORM := $(NDK)/platforms/android-9/arch-x86
#endif

# ******** GLOBAL CONFIGURATIONS ******** #
