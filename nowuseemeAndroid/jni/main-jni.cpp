#ifndef _ANDROID_
#include "main-jni.h"
#endif

#include <stdlib.h>

#include "global-jni.h"

extern "C" {
    #include "log.h"
    #include "error_codes.h"
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

#ifdef _ANDROID_
extern "C" {
void Java_com_bbmlf_nowuseeme_MainActivity_initFFmpeg(JNIEnv* env, jobject thiz)
#else
void initFFmpeg()
#endif
{
	LOGV("executing 'initFFmpeg()...'\n");

	// FFmpeg registration
	av_register_all();

	LOGV("'initFFmpeg()' successfully executed\n");
}
#ifdef _ANDROID_
}
#endif
