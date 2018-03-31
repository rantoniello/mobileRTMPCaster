#include <stdlib.h>
#include "global-jni.h"

#ifdef _ANDROID_
JavaVM* global_jni_JavaVM= NULL;
/**
 * This native method is called internally when the native library is loaded.
 * Our aim is to initialize a global 'JavaVM*' object pointer.
 * Later, this global pointer may be used to call a method of
 * the "java layer" from JNI (typically used for events coming from JNI to
 * Java layer).
 */
extern "C" {
JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
	LOGI("'JNI_OnLoad()...'\n");
	global_jni_JavaVM= vm;
	return JNI_VERSION_1_6;
}
}
#endif //_ANDROID_
