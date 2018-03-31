#!/bin/bash

CONFIG_LINUX_MK_PATH=$(pwd)
INSTALL_DIR="$CONFIG_LINUX_MK_PATH/_install_dir_arm"

if [ `getconf LONG_BIT` = "64" ]
then
	#echo "I'm 64-bit"
	VER=4.9
	NDK=$CONFIG_LINUX_MK_PATH/../android/adt-bundle-linux-x86_64/android-ndk-r10c
	PREBUILT=$NDK/toolchains/arm-linux-androideabi-$VER/prebuilt/linux-x86_64
	PLATFORM=$NDK/platforms/android-9/arch-arm
else
	#echo "I'm 32-bit"
	VER=4.8
	NDK=$CONFIG_LINUX_MK_PATH/../android/adt-bundle-linux-x86/android-ndk-r9c
	PREBUILT=$NDK/toolchains/arm-linux-androideabi-$VER/prebuilt/linux-x86
	PLATFORM=$NDK/platforms/android-9/arch-arm
fi

SONAME=libffmpeg.so
OUT_LIBRARY=$INSTALL_DIR/lib/libffmpeg.so
EABIARCH=arm-linux-androideabi

$PREBUILT/bin/arm-linux-androideabi-ld -rpath-link=$PLATFORM/usr/lib -L$PLATFORM/usr/lib -L$INSTALL_DIR/lib -soname $SONAME -shared -nostdlib -z noexecstack -Bsymbolic --whole-archive --no-undefined -o $OUT_LIBRARY -lvo-aacenc -lmp3lame -lx264 -lavdevice -lavformat -lavfilter -lavcodec -lswscale -lavutil -lpostproc -lswresample -lc -lm -lz -ldl --dynamic-linker=/system/bin/linker -zmuldefs $PREBUILT/lib/gcc/$EABIARCH/$VER/libgcc.a || exit 1
