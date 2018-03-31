
LOCAL_PATH := $(call my-dir)

ifeq ($(TARGET_ARCH),x86)
	LOCAL_CFLAGS   := -mfpu=vfp -mfloat-abi=softfp $(COMMON_FLAGS_LIST)
	INSTALL_DIR := _install_dir_x86_device_android_32
else ifeq ($(TARGET_ARCH),mips)
	LOCAL_CFLAGS   := $(COMMON_FLAGS_LIST)
	INSTALL_DIR := _install_dir_mips_android
else
	LOCAL_CFLAGS   := $(COMMON_FLAGS_LIST)
	INSTALL_DIR := _install_dir_arm_android
endif

#//-------------------------------------------------------------------------//
#include $(CLEAR_VARS)
#LOCAL_ALLOW_UNDEFINED_SYMBOLS=false
#LOCAL_MODULE := ffmpeg-prebuilt
#LOCAL_SRC_FILES := ../../libs/$(INSTALL_DIR)/lib/libffmpeg.so
#LOCAL_EXPORT_C_INCLUDES := ../libs/$(INSTALL_DIR)/include
#include $(PREBUILT_SHARED_LIBRARY)

#//-------------------------------------------------------------------------//
include $(CLEAR_VARS)
LOCAL_ALLOW_UNDEFINED_SYMBOLS=false
LOCAL_MODULE := vo-aacenc-prebuilt
LOCAL_SRC_FILES := ../../libs/$(INSTALL_DIR)/lib/libvo-aacenc.so
LOCAL_LDLIBS += -lc -lm -lz -ldl
LOCAL_EXPORT_C_INCLUDES := ../libs/$(INSTALL_DIR)/include
include $(PREBUILT_SHARED_LIBRARY)

#//-------------------------------------------------------------------------//
include $(CLEAR_VARS)
LOCAL_ALLOW_UNDEFINED_SYMBOLS=false
LOCAL_MODULE := mp3lame-prebuilt
LOCAL_SRC_FILES := ../../libs/$(INSTALL_DIR)/lib/libmp3lame.so
LOCAL_LDLIBS += -lc -lm -lz -ldl
LOCAL_EXPORT_C_INCLUDES := ../libs/$(INSTALL_DIR)/include
include $(PREBUILT_SHARED_LIBRARY)

#//-------------------------------------------------------------------------//
include $(CLEAR_VARS)
LOCAL_ALLOW_UNDEFINED_SYMBOLS=false
LOCAL_MODULE := x264-prebuilt
LOCAL_SRC_FILES := ../../libs/$(INSTALL_DIR)/lib/libx264.so
LOCAL_LDLIBS += -lc -lm -lz -ldl
LOCAL_EXPORT_C_INCLUDES := ../libs/$(INSTALL_DIR)/include
include $(PREBUILT_SHARED_LIBRARY)

#//-------------------------------------------------------------------------//
include $(CLEAR_VARS)
LOCAL_ALLOW_UNDEFINED_SYMBOLS=false
LOCAL_MODULE := avdevice-prebuilt
LOCAL_SRC_FILES := ../../libs/$(INSTALL_DIR)/lib/libavdevice.so
LOCAL_LDLIBS += -lc -lm -lz -ldl
LOCAL_EXPORT_C_INCLUDES := ../libs/$(INSTALL_DIR)/include
include $(PREBUILT_SHARED_LIBRARY)

#//-------------------------------------------------------------------------//
include $(CLEAR_VARS)
LOCAL_ALLOW_UNDEFINED_SYMBOLS=false
LOCAL_MODULE := avformat-prebuilt
LOCAL_SRC_FILES := ../../libs/$(INSTALL_DIR)/lib/libavformat.so
LOCAL_LDLIBS += -lc -lm -lz -ldl
LOCAL_EXPORT_C_INCLUDES := ../libs/$(INSTALL_DIR)/include
include $(PREBUILT_SHARED_LIBRARY)

#//-------------------------------------------------------------------------//
include $(CLEAR_VARS)
LOCAL_ALLOW_UNDEFINED_SYMBOLS=false
LOCAL_MODULE := avfilter-prebuilt
LOCAL_SRC_FILES := ../../libs/$(INSTALL_DIR)/lib/libavfilter.so
LOCAL_LDLIBS += -lc -lm -lz -ldl
LOCAL_EXPORT_C_INCLUDES := ../libs/$(INSTALL_DIR)/include
include $(PREBUILT_SHARED_LIBRARY)

#//-------------------------------------------------------------------------//
include $(CLEAR_VARS)
LOCAL_ALLOW_UNDEFINED_SYMBOLS=false
LOCAL_MODULE := avcodec-prebuilt
LOCAL_SRC_FILES := ../../libs/$(INSTALL_DIR)/lib/libavcodec.so
LOCAL_LDLIBS += -lc -lm -lz -ldl
LOCAL_EXPORT_C_INCLUDES := ../libs/$(INSTALL_DIR)/include
include $(PREBUILT_SHARED_LIBRARY)

#//-------------------------------------------------------------------------//
include $(CLEAR_VARS)
LOCAL_ALLOW_UNDEFINED_SYMBOLS=false
LOCAL_MODULE := swscale-prebuilt
LOCAL_SRC_FILES := ../../libs/$(INSTALL_DIR)/lib/libswscale.so
LOCAL_LDLIBS += -lc -lm -lz -ldl
LOCAL_EXPORT_C_INCLUDES := ../libs/$(INSTALL_DIR)/include
include $(PREBUILT_SHARED_LIBRARY)

#//-------------------------------------------------------------------------//
include $(CLEAR_VARS)
LOCAL_ALLOW_UNDEFINED_SYMBOLS=false
LOCAL_MODULE := avutil-prebuilt
LOCAL_SRC_FILES := ../../libs/$(INSTALL_DIR)/lib/libavutil.so
LOCAL_LDLIBS += -lc -lm -lz -ldl
LOCAL_EXPORT_C_INCLUDES := ../libs/$(INSTALL_DIR)/include
include $(PREBUILT_SHARED_LIBRARY)

#//-------------------------------------------------------------------------//
include $(CLEAR_VARS)
LOCAL_ALLOW_UNDEFINED_SYMBOLS=false
LOCAL_MODULE := postproc-prebuilt
LOCAL_SRC_FILES := ../../libs/$(INSTALL_DIR)/lib/libpostproc.so
LOCAL_LDLIBS += -lc -lm -lz -ldl
LOCAL_EXPORT_C_INCLUDES := ../libs/$(INSTALL_DIR)/include
include $(PREBUILT_SHARED_LIBRARY)

#//-------------------------------------------------------------------------//
include $(CLEAR_VARS)
LOCAL_ALLOW_UNDEFINED_SYMBOLS=false
LOCAL_MODULE := swresample-prebuilt
LOCAL_SRC_FILES := ../../libs/$(INSTALL_DIR)/lib/libswresample.so
LOCAL_LDLIBS += -lc -lm -lz -ldl
LOCAL_EXPORT_C_INCLUDES := ../libs/$(INSTALL_DIR)/include
include $(PREBUILT_SHARED_LIBRARY)

#//-------------------------------------------------------------------------//
include $(CLEAR_VARS)
LOCAL_CFLAGS := -D_ANDROID_LOG -D__STDC_CONSTANT_MACROS
LOCAL_CFLAGS += -D_ANDROID_
LOCAL_MODULE := main-jni
LOCAL_SRC_FILES := ./global-jni.cpp
LOCAL_SRC_FILES += ./main-jni.cpp
LOCAL_SHARED_LIBRARIES := vo-aacenc-prebuilt mp3lame-prebuilt x264-prebuilt avdevice-prebuilt \
avformat-prebuilt avfilter-prebuilt avcodec-prebuilt swscale-prebuilt avutil-prebuilt \
postproc-prebuilt swresample-prebuilt
LOCAL_LDLIBS += -llog -ljnigraphics -lz -lm -landroid
LOCAL_C_INCLUDES := ../libs/$(INSTALL_DIR)/include
LOCAL_C_INCLUDES += ../libs/src/
include $(BUILD_SHARED_LIBRARY)

#//-------------------------------------------------------------------------//
include $(CLEAR_VARS)
#LOCAL_ALLOW_UNDEFINED_SYMBOLS=false
LOCAL_CFLAGS := -D_ANDROID_LOG -D__STDC_CONSTANT_MACROS
LOCAL_CFLAGS += -D_ANDROID_
LOCAL_MODULE := rtmpclient-jni
LOCAL_SRC_FILES := ../../libs/src/rtmpclient.c
LOCAL_SRC_FILES += ./global-jni.cpp
LOCAL_SRC_FILES += ./rtmpclient-jni.cpp
LOCAL_SHARED_LIBRARIES := vo-aacenc-prebuilt mp3lame-prebuilt x264-prebuilt avdevice-prebuilt \
avformat-prebuilt avfilter-prebuilt avcodec-prebuilt swscale-prebuilt avutil-prebuilt \
postproc-prebuilt swresample-prebuilt
LOCAL_LDLIBS += -llog -ljnigraphics -lz -lm -landroid
LOCAL_C_INCLUDES := ../libs/$(INSTALL_DIR)/include
LOCAL_C_INCLUDES += ../libs/src/
include $(BUILD_SHARED_LIBRARY)

#//-------------------------------------------------------------------------//
include $(CLEAR_VARS)
LOCAL_CFLAGS := -D_ANDROID_LOG -D__STDC_CONSTANT_MACROS
#LOCAL_CFLAGS := -D_NO_LOG
LOCAL_CFLAGS += -D_ANDROID_
LOCAL_CFLAGS += -D GL_GLEXT_PROTOTYPES
LOCAL_MODULE := player-jni
LOCAL_SRC_FILES := ../../libs/src/demuxer.c
LOCAL_SRC_FILES += ./global-jni.cpp
LOCAL_SRC_FILES += ./player-jni.cpp
LOCAL_SHARED_LIBRARIES := vo-aacenc-prebuilt mp3lame-prebuilt x264-prebuilt avdevice-prebuilt \
avformat-prebuilt avfilter-prebuilt avcodec-prebuilt swscale-prebuilt avutil-prebuilt \
postproc-prebuilt swresample-prebuilt
LOCAL_LDLIBS := -llog -landroid -lGLESv1_CM -lOpenSLES -lEGL
LOCAL_C_INCLUDES := ../libs/$(INSTALL_DIR)/include
LOCAL_C_INCLUDES += ../libs/src/
include $(BUILD_SHARED_LIBRARY)
