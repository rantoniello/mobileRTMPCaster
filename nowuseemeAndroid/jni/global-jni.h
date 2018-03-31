#ifndef GLOBAL_JNI_HH_
#define GLOBAL_JNI_HH_

/* Includes */
#ifdef _ANDROID_
#include <jni.h>
#endif

/* Definitions */
#ifdef _X86_TESTING_LOG
	#define LOGV(...) printf("%s %s %d ", __FILE__, __func__, __LINE__); printf(__VA_ARGS__); fflush(stdout);
	#define LOGD(...) printf("%s %s %d ", __FILE__, __func__, __LINE__); printf(__VA_ARGS__); fflush(stdout);
	#define LOGI(...) printf("%s %s %d ", __FILE__, __func__, __LINE__); printf(__VA_ARGS__); fflush(stdout);
	#define LOGW(...) printf("%s %s %d ", __FILE__, __func__, __LINE__); printf(__VA_ARGS__); fflush(stdout);
	#define LOGE(...) printf("%s %s %d ", __FILE__, __func__, __LINE__); printf(__VA_ARGS__); fflush(stdout);
#endif
#ifdef _ANDROID_LOG
	#include <jni.h>
	#include <android/log.h>
	#define TAG "JNI"
	#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
	#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG  , TAG, __VA_ARGS__)
	#define LOGI(...) __android_log_print(ANDROID_LOG_INFO   , TAG, __VA_ARGS__)
	#define LOGW(...) __android_log_print(ANDROID_LOG_WARN   , TAG, __VA_ARGS__)
	#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR  , TAG, __VA_ARGS__)
#endif
#ifdef _NO_LOG
	#define LOGV(...)
	#define LOGD(...)
	#define LOGI(...)
	#define LOGW(...)
	#define LOGE(...)
#endif

/* Global externs */
#ifdef _ANDROID_
extern JavaVM* global_jni_JavaVM;
#endif

#endif /* GLOBAL_JNI_HH_ */
