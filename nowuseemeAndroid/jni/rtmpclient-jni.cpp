#ifndef _ANDROID_
    #include "rtmpclient-jni.h"
#endif

#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h> //usleep

#include "global-jni.h"

extern "C" {
    #include "log.h"
    #include "error_codes.h"
    #include "rtmpclient.h"
}

#ifdef _ANDROID_
#else
#include "RtmpClientCtrl.h"
#endif

/* Prototypes */
static void onErrorCallback(void* t);
static void *thrError(void *t);

/* Globals */
#ifdef _ANDROID_
static jobject global_jni_objRtmpClientCtrl= NULL;
#else
static void *global_jni_objRtmpClientCtrl= NULL;
#endif
static rtmpMuxerCtx muxer;
static pthread_t thread;

#ifdef _ANDROID_
/**
 * This native method is called at 'RtmpClientCtrl' initialization to globally
 * set, at JNI level, the 'jobject' corresponding to the RtmpClientCtrl class.
 * Later, this object may be used to call a method of
 * the "java layer" from JNI (typically used for events coming from JNI to
 * Java layer).
 */
extern "C" {
void Java_com_bbmlf_nowuseeme_RtmpClientCtrl_setJavaObj(JNIEnv* env, jobject thiz)
{
	LOGI("Executing 'Java_com_bbmlf_nowuseeme_RtmpClientCtrl_setJavaObj()...'\n");
	global_jni_objRtmpClientCtrl= env->NewGlobalRef(thiz);
	LOGI("Exiting 'Java_com_bbmlf_nowuseeme_RtmpClientCtrl_setJavaObj()...'\n");
}
}

/**
 * Call this native method at 'RtmpClientCtrl' de-initialization in order to
 * release globals previously allocated.
 */
extern "C" {
void Java_com_bbmlf_nowuseeme_RtmpClientCtrl_releaseJavaObj(JNIEnv* env, jobject thiz)
{
	LOGI("Executing 'Java_com_bbmlf_nowuseeme_RtmpClientCtrl_releaseJavaObj()...'\n");
    if(global_jni_objRtmpClientCtrl!= NULL)
    {
    	env->DeleteGlobalRef(global_jni_objRtmpClientCtrl);
    	global_jni_objRtmpClientCtrl= NULL;
    }
    LOGI("Exiting 'Java_com_bbmlf_nowuseeme_RtmpClientCtrl_releaseJavaObj()...'\n");
}
}

extern "C" {
JNIEnv* attach_current_thread()
{
    int status;
    JNIEnv *env= NULL;
	LOGI("Executing 'attach_current_thread()...'\n");

	/* Get current thread environment */
    if((status= global_jni_JavaVM->GetEnv((void**)&env, JNI_VERSION_1_6))< 0)
	{
        if((status= global_jni_JavaVM->AttachCurrentThread(&env, NULL))< 0)
        {
        	LOGE("Could not get thread environment!\n");
        	return NULL;
        }
    }
    LOGI("Exiting 'attach_current_thread()'.\n");
    return env;
}
}

extern "C" {
void detach_current_thread()
{
	LOGI("Executing 'detach_current_thread()...'\n");
	global_jni_JavaVM->DetachCurrentThread();
    LOGI("Exiting 'detach_current_thread()'.\n");
}
}
#else
void setRtmpClientCtrlObj(void *t)
{
    LOGI("Executing 'setRtmpClientCtrlObj()...'\n");
    global_jni_objRtmpClientCtrl= t;
    LOGI("Exiting 'setRtmpClientCtrlObj()...'\n");
}
void releaseRtmpClientCtrlObj()
{
    LOGI("Executing 'releaseRtmpClientCtrlObj()...'\n");
    if(global_jni_objRtmpClientCtrl!= NULL)
    {
        global_jni_objRtmpClientCtrl= NULL;
    }
    LOGI("Exiting 'releaseRtmpClientCtrlObj()...'\n");
}
#endif //_ANDROID_

/* ============================================================================
 * Android wrapper methods for using RTMP client.
 *
 * The "calling-cycle" is as follows:
 *
 * 1.- rtmpclientOpen(): Called only once, for the full life of the application.
 *
 * 2.- Circular calls to:
 *
 *     2.1- rtmpclientInit(): Init RTMP client for encoding A/V frames.
 *          (NOTE: rtmpclientGetABufSize() may be used after rtmpclientInit()
 *          to obtain optimal audio-samples frame buffer size to be used)
 *
 *     2.2- Circular and individually threaded calls to:
 *          2.2.1- encodeVFrame()/encodeAFrame(): encodes video/audio frames
 *
 *     2.3- rtmpclientRelease(): Release RTMP client used for for encoding
 *     A/V frames.
 *
 * 3.- rtmpclientClose(): Called only once, just when application is being
 * destroyed.
 *  ===========================================================================
 */

/**
 * Globally initialize RTMP Client.
 * This method **MUST** be called prior to any other of the RTMP client and
 * only once, for the full life of the application.
 */
#ifdef _ANDROID_
extern "C" {
void Java_com_bbmlf_nowuseeme_RtmpClientCtrl_rtmpclientOpen(JNIEnv* env, jobject thiz)
#else
void rtmpclientOpen()
#endif
{
	LOGI("'rtmpclientOpen()...'\n");
	rtmpclient_open(&muxer);
}
#ifdef _ANDROID_
}
#endif

/**
 * Globally de-initialize RTMP Client.
 * This method **MUST** be called only once, as the last method called just
 * before finalizing the program.
 */
#ifdef _ANDROID_
extern "C" {
void Java_com_bbmlf_nowuseeme_RtmpClientCtrl_rtmpclientClose(JNIEnv* env, jobject thiz)
#else
void rtmpclientClose()
#endif
{
	LOGI("'rtmpclientClose()...'\n");
	rtmpclient_close(&muxer);
}
#ifdef _ANDROID_
}
#endif

/**
 * Init RTMP client for encoding A/V frames.
 */
#ifdef _ANDROID_
extern "C" {
jint Java_com_bbmlf_nowuseeme_RtmpClientCtrl_rtmpclientInit(JNIEnv* env,
		jobject thiz, jbyteArray array, jint vavailable, jint aavailable)
#else
int rtmpclientInit(const char* array, int vavailable, int aavailable)
#endif
{
	int ret= ERROR;
#ifdef _ANDROID_
	jboolean isCopy;
#endif

	pthread_mutex_lock(&muxer.audio_mutex);
	pthread_mutex_lock(&muxer.video_mutex);

#ifdef _ANDROID_
	jbyte* cmd= env->GetByteArrayElements(array, &isCopy);
#else
    char *cmd= (char*)array;
#endif

	/* Check if it is already initialized */
    if(muxer.oc!= NULL)
    {
    	LOGI("'rtmpclient_init()' has already been called!\n");
    	ret= RTMP_CLIENT_ALREADY_INITIALIZED;
    	goto end_jni_rtmpclientInit;
    }

    /* Parse JSON parameters passed by argument */
    if(cmd_parser(&muxer, (char const*)cmd)!= SUCCESS)
    {
    	LOGE("'cmd_parser()' failed\n");
    	ret= JSON_COMMAND_PARSER_ERROR;
    	goto end_jni_rtmpclientInit;
    }

	/* Re-initialize some other client context parameters */

    // Background thread related parameters
    muxer.inBackground= 0; // set to 'false'
	muxer.bckground_width= 352;
	muxer.bckground_height= 288;

	// Interleaveing circular buffer initialization
	muxer.af= 0;
	muxer.ab= 0;
	muxer.level= 0;
	muxer.alevel= 0;
	muxer.vlevel= 0;
#ifndef _ANDROID_
    muxer.sample_fifo_level= 0;
#endif
	// Container setting
	muxer.o_mux_format= (char*)((strncmp(muxer.uri, "rtmp://", 7)== 0)? "flv": "mpegts");
	muxer.v_available= vavailable;
	muxer.a_available= aavailable;
	// On error callback
	muxer.on_error= onErrorCallback;
	muxer.error_code= SUCCESS;
	muxer.persistant_error_usecs= 0;

	/* Init RTMP client */
	{
		int retcode= rtmpclient_init(&muxer);
		if(retcode!= SUCCESS || muxer.oc== NULL)
		{
			LOGI("'rtmpclient_init()' failed\n");
			ret= retcode;
			goto end_jni_rtmpclientInit;
		}
	}

	/* Success */
	ret= SUCCESS;

end_jni_rtmpclientInit:
#ifdef _ANDROID_
	env->ReleaseByteArrayElements(array, cmd, JNI_ABORT);
#endif
	pthread_mutex_unlock(&muxer.audio_mutex);
	pthread_mutex_unlock(&muxer.video_mutex);
	return ret;
}
#ifdef _ANDROID_
}
#endif

/**
 * De-initialize RTMP client.
 */
#ifdef _ANDROID_
extern "C" {
void Java_com_bbmlf_nowuseeme_RtmpClientCtrl_rtmpclientRelease(JNIEnv* env, jobject thiz)
#else
void rtmpclientRelease()
#endif
{
	LOGD("rtmpclientRelease()... \n");
	pthread_mutex_lock(&muxer.audio_mutex);
	pthread_mutex_lock(&muxer.video_mutex);
	rtmpclient_release(&muxer.oc, &muxer);
	pthread_mutex_unlock(&muxer.audio_mutex);
	pthread_mutex_unlock(&muxer.video_mutex);
}
#ifdef _ANDROID_
}
#endif

/**
 * Obtain optimal audio-samples frame buffer size to be used
 * (call after 'rtmpclientInit()' method).
 */
#ifdef _ANDROID_
extern "C" {
jint Java_com_bbmlf_nowuseeme_AudioRecorder_rtmpclientGetABufSize(JNIEnv* env,
		jobject thiz)
#else
int rtmpclientGetABufSize()
#endif
{
	LOGI("FFmpeg selected muxer.adata_size= %d\n", muxer.adata_size);
	return (int)muxer.adata_size;
}
#ifdef _ANDROID_
}
#endif

/**
 *  Encodes video frame.
 *  Previously, RTMP client must be successfully initialized by calling
 *  'rtmpclientInit()' method.
 *  @param array Raw YUV planar 4:2:0 frame to be encoded.
 *  @return 0 if success, Error code if fails.
 */
#ifdef _ANDROID_
extern "C" {
jint Java_com_bbmlf_nowuseeme_VideoRecorder_encodeVFrame(JNIEnv* env,
		jobject thiz, jbyteArray array)
#else
int encodeVFrame(char* array)
#endif
{
	int ret= ERROR, ret_2;
    uint64_t t;
#ifdef _ANDROID_
    int stride, Y_size, c_stride, C_size, cr_offset, cb_offset;
	jboolean isCopy;
	jbyte* buffer= env->GetByteArrayElements(array, &isCopy);
#else
    char *buffer= array;
    struct SwsContext *img_convert_ctx= NULL;
    const uint8_t* srcSlice[1]= {(uint8_t*)buffer};
    int srcStride[1]= {muxer.v_width* 4}; // RGBA stride
    iOS_data_buffer *rtmpclient_data; //FIXME!!
#endif

	pthread_mutex_lock(&muxer.video_mutex);

	/* Sanity check */
    if(muxer.oc== NULL)
    {
    	LOGI("Error: muxer was not initialized!\n");
    	ret= RETRY; //RTMP_CLIENT_NOT_PREV_INITIALIZED; // Better to preserve causing error
    	goto end_jni_encodevframe;
    }
    if(muxer.picture->data[0]== NULL)
    {
    	LOGI("Error: picture buffer is not still initialized!\n");
    	ret= VIDEO_FRAME_BUFFER_ALLOC_ERROR;
    	goto end_jni_encodevframe;
    }
    /* Important: it is essential to avoid sending video frames while being in
     * background. In Java layer, video capturing thread has its own "inertia";
     * this way we avoid mixing trailing camera images and "background" images.
     * */
    if(muxer.inBackground!= 0)
    {
    	LOGI("Error: Already running in background mode!\n");
    	ret= ALREADY_IN_BACKGROUND_ERROR;
    	goto end_jni_encodevframe;
    }

#ifdef _ANDROID_
	/* Fill YUV buffers:
	 *  stride = ALIGN(width, 16)
     *  y_size = stride * height
 	 *  c_stride = ALIGN(stride/2, 16)
 	 *  c_size = c_stride * height/2
 	 *  size = y_size + c_size * 2
 	 *  cr_offset = y_size
	 *  cb_offset = y_size + c_size
     */
#define ALIGN(x,a)              __ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)    (((x)+(mask))&~(mask))
	stride= ALIGN(muxer.v_width, 16);
	Y_size= stride*muxer.v_height;
	c_stride= ALIGN((stride/2), 16);
	C_size= c_stride*(muxer.v_height/2);
	cr_offset= Y_size;
	cb_offset= Y_size+C_size;
	//Hack to fix buggy android implementations (buffers must be aligned to 1024*2)
	int h_idx;
	for(h_idx= 0; h_idx< muxer.v_height; h_idx++)
	{
		memcpy(&muxer.picture->data[0][h_idx*muxer.v_width], &buffer[h_idx*stride], muxer.v_width);
	}
	for(h_idx= 0; h_idx< (muxer.v_height/2); h_idx++)
	{
		memcpy(&muxer.picture->data[2][h_idx*(muxer.v_width/2)], &buffer[cr_offset+(h_idx*c_stride)], muxer.v_width/2);
		memcpy(&muxer.picture->data[1][h_idx*(muxer.v_width/2)], &buffer[cb_offset+(h_idx*c_stride)], muxer.v_width/2);
	}
#else
#if 0 //FIXME!!
    // Change video frame RGB-> YUV420P
    //LOGD("Creating conversion context...\n");
    img_convert_ctx= sws_getContext(muxer.v_width, muxer.v_height, AV_PIX_FMT_BGRA,
                                    muxer.v_width, muxer.v_height, (AVPixelFormat)muxer.v_pix_format,
                                    0, NULL, NULL, NULL);
    if(img_convert_ctx== NULL)
    {
        LOGE("Cannot initialize the conversion context!\n");
        goto end_jni_encodevframe;
    }

    //LOGD("Re-formatting... [%dx%d]->[%dx%d]\n", src_w, src_h, dst_w, dst_h);
    //if (option & VIDEO_FLIP) {
    //srcSlice[0]+= srcStride[0]*(muxer.v_height-1);
    //srcStride[0] = -srcStride[0];
    //}
    sws_scale(img_convert_ctx,
              srcSlice, srcStride, 0, muxer.v_height,
              muxer.picture->data, muxer.picture->linesize);

    if(img_convert_ctx!= NULL)
    {
        sws_freeContext(img_convert_ctx);
        img_convert_ctx= NULL;
    }
    //LOGD("O.K.\n");
#endif
    rtmpclient_data= (iOS_data_buffer*)buffer;
    // TODO: Check sizes with MIN()...
    memcpy(muxer.picture->data[0], rtmpclient_data->ybuf, muxer.v_width* muxer.v_height);
    memcpy(muxer.picture->data[1], rtmpclient_data->cbbuf, muxer.v_width* muxer.v_height/4);
    memcpy(muxer.picture->data[2], rtmpclient_data->crbuf, muxer.v_width* muxer.v_height/4);
#endif

	/* get monotonic clock time */
	t= get_monotonic_clock_time(&muxer);

	/* Encode video frame */
	if((ret_2= rtmpclient_encode_vframe(&muxer, t))!= SUCCESS)
	{
		ret= ret_2;
		goto end_jni_encodevframe;
    }

	/* Success */
	ret= SUCCESS;

end_jni_encodevframe:

	pthread_mutex_unlock(&muxer.video_mutex);
#ifdef _ANDROID_
	env->ReleaseByteArrayElements(array, buffer, JNI_ABORT);
#else
    /* Release conversion context */
    if(img_convert_ctx!= NULL)
    {
        sws_freeContext(img_convert_ctx);
        img_convert_ctx= NULL;
    }

#endif

	/* We add a sleep here to avoid CPU consuming loops in case of error */
	if(ret!= SUCCESS)
	{
		usleep(500*1000); //sleep some milliseconds
	}
	return ret;
}
#ifdef _ANDROID_
}
#endif

/**
 *  Encodes audio frame.
 *  Previously, RTMP client must be successfully initialized by calling
 *  'rtmpclientInit()' method.
 *  @param array Signed 16-bit little-endian valued audio frame.
 *  @param framesize Audio frame size.
 *  @return 0 if success, Error code if fails.
 */
#ifdef _ANDROID_
extern "C" {
jint Java_com_bbmlf_nowuseeme_AudioRecorder_encodeAFrame(JNIEnv* env,
		jobject thiz, jshortArray array, int framesize)
#else
int encodeAFrame(int16_t *array, int framesize)
#endif
{
    uint64_t t;
	int ret= -1, ret_2;
#ifdef _ANDROID_
	jboolean isCopy;
	jshort* buffer= env->GetShortArrayElements(array, &isCopy);
	jsize length= env->GetArrayLength(array);
#else
    int16_t *buffer= array;
#endif

	pthread_mutex_lock(&muxer.audio_mutex);

	/* Sanity check */
    if(muxer.oc== NULL)
    {
    	LOGI("Error: muxer was not initialized!\n");
    	ret= RETRY; //RTMP_CLIENT_NOT_PREV_INITIALIZED; // Better to preserve causing error
    	goto end_jni_encodeaframe;
    }
    if(muxer.adata_size== 0)
    {
    	LOGE("Error: audio buffer is not still initialized!\n");
    	ret= AUDIO_SAMPLE_BUFFER_ALLOC_ERROR;
    	goto end_jni_encodeaframe;
    }
    if(buffer== NULL || framesize== 0)
    {
    	LOGE("Error: audio buffer is empty!\n");
    	ret= EMPTY_AUDIO_FRAME_ERROR;
    	goto end_jni_encodeaframe;
    }

	/* Get a frame of audio samples */
#ifndef _ANDROID_ //HACK: prologue
    //on Android: frame size is set by FFmpeg for optimal operation
    //LOGV("muxer.adata_size= %d\n", muxer.adata_size);
    if(muxer.adata_size> framesize && SAMLE_FIFO_MAX_SIZE>= (muxer.adata_size* 2))
    {
        memcpy(&muxer.sample_fifo[muxer.sample_fifo_level],
               buffer,
               framesize* sizeof(uint16_t));
        muxer.sample_fifo_level+= framesize;

        if(muxer.sample_fifo_level< muxer.adata_size)
        {
            ret= SUCCESS;
            goto end_jni_encodeaframe;
        }
        muxer.adata= (int16_t*)muxer.sample_fifo;
    }
    else{
        muxer.adata= buffer;
    }
#else
    muxer.adata= buffer;
#endif

	/* get monotonic clock time */
    t= get_monotonic_clock_time(&muxer);

	/* Encode audio frame */
    ret_2= rtmpclient_encode_aframe(&muxer, t);

#ifndef _ANDROID_ //HACK: epilogue
    if(muxer.adata_size> framesize && SAMLE_FIFO_MAX_SIZE>= (muxer.adata_size* 2))
    {
        if(muxer.sample_fifo_level>= muxer.adata_size)
        {
            muxer.sample_fifo_level-= muxer.adata_size;
            if(muxer.sample_fifo_level> 0)
                memmove(muxer.sample_fifo, &muxer.sample_fifo[muxer.adata_size], muxer.sample_fifo_level* sizeof(uint16_t));
        }
        //LOGV("Fifo level: %d\n", muxer.sample_fifo_level); // Comment-me
    }
#endif
    if(ret_2!= SUCCESS)
    {
        ret= ret_2;
        goto end_jni_encodeaframe;
    }

	/* Success */
	ret= SUCCESS;

end_jni_encodeaframe:

	pthread_mutex_unlock(&muxer.audio_mutex);

#ifdef _ANDROID_
	env->ReleaseShortArrayElements(array, buffer, JNI_ABORT);
#endif

	/* We add a sleep here to avoid CPU consuming loops in case of error */
	if(ret!= SUCCESS)
	{
		usleep(5*1000); //sleep some milliseconds
	}
	return ret;
}
#ifdef _ANDROID_
}
#endif

/**
 * Run so-called background thread.
 * When Main-Activity is paused and just before the calling application goes to
 * background execution, this method may be called. A new thread is fired, in
 * which a 'dummy' video showing a still image ("background image") will run
 * along with audio capturing.
 * @return Error code on fail, '0' on success.
 */
#ifdef _ANDROID_
extern "C" {
jint Java_com_bbmlf_nowuseeme_RtmpClientCtrl_runBackground(JNIEnv* env,
		jobject thiz)
#else
int runBackground()
#endif
{
	int ret= ERROR;
	LOGI("'runBackground()' ...\n");

	pthread_mutex_lock(&muxer.video_mutex);
	pthread_mutex_lock(&muxer.bckground_mutex);

	/* Sanity check */
    if(muxer.oc== NULL)
    {
    	LOGI("Error: muxer was not initialized!\n");
    	ret= RETRY; //RTMP_CLIENT_NOT_PREV_INITIALIZED;
    	goto end_jni_runBackground;
    }
    if(muxer.picture->data[0]== NULL)
    {
    	LOGI("Error: picture buffer is not still initialized!\n");
    	ret= VIDEO_FRAME_BUFFER_ALLOC_ERROR;
    	goto end_jni_runBackground;
    }
    if(muxer.inBackground!= 0)
    {
    	LOGI("Error: Already running in background mode!\n");
    	ret= ALREADY_IN_BACKGROUND_ERROR;
    	goto end_jni_runBackground;
    }

    /* Run background thread */
    rtmp_client_runBackground(&muxer);

	/* Success */
	ret= SUCCESS;

end_jni_runBackground:

	pthread_mutex_unlock(&muxer.video_mutex);
	pthread_mutex_unlock(&muxer.bckground_mutex);
	LOGI("'runBackground()' exit O.K.\n");
	return ret;
}
#ifdef _ANDROID_
}
#endif

/**
 * Set background image to be used in background thread.
 * Image provided should be in planar YUV 4:2:0 format.
 * @param array Byte buffer containing image.
 * @return Error code on fail, '0' on success.
 */
#ifdef _ANDROID_
extern "C" {
jint Java_com_bbmlf_nowuseeme_RtmpClientCtrl_setBackgroundImage(JNIEnv* env,
		jobject thiz, jbyteArray array)
#else
int setBackgroundImage(char *array, int length)
#endif
{
	int ret= ERROR;
	LOGI("'setBackgroundImage()' ...\n");
#ifdef _ANDROID_
	jboolean isCopy;
	jbyte* buffer= env->GetByteArrayElements(array, &isCopy);
	jsize length= env->GetArrayLength(array);
#else
    char *buffer= array;
#endif

	pthread_mutex_lock(&muxer.video_mutex);
	pthread_mutex_lock(&muxer.bckground_mutex);

	/* Sanity check */
    if(muxer.oc== NULL)
    {
    	LOGI("Error: muxer was not initialized!");
    	ret= RETRY; //RTMP_CLIENT_NOT_PREV_INITIALIZED;
    	goto end_setBackgroundImage;
    }
    if(muxer.picture->data[0]== NULL)
    {
    	LOGI("Error: picture buffer is not still initialized!\n");
    	ret= VIDEO_FRAME_BUFFER_ALLOC_ERROR;
    	goto end_setBackgroundImage;
    }
    if(buffer== NULL)
    {
    	LOGE("Error: background-logo buffer is empty!\n");
    	ret= BAD_ARGUMENT_OR_INITIALIZATION_ERROR;
    	goto end_setBackgroundImage;
    }

    /* Allocate background image buffer */
    if(muxer.bckgroundLogo!= NULL)
    {
    	free(muxer.bckgroundLogo);
    	muxer.bckgroundLogo= NULL;
    }
	muxer.bckgroundLogo= (char*)malloc(length);
	if(muxer.bckgroundLogo== NULL)
	{
		LOGE("Failed to allocate background image buffer\n");
		ret= ERROR;
		goto end_setBackgroundImage;
	}

    /* Copy background image to JNI local buffer */
   	memcpy(muxer.bckgroundLogo, buffer, length);

	/* Success */
	ret= SUCCESS;

end_setBackgroundImage:

	pthread_mutex_unlock(&muxer.video_mutex);
	pthread_mutex_unlock(&muxer.bckground_mutex);
#ifdef _ANDROID_
	env->ReleaseByteArrayElements(array, buffer, JNI_ABORT);
#endif
	return ret;
}
#ifdef _ANDROID_
}
#endif

/**
 * Stop background thread.
 * @return Error code on fail, '0' on success.
 */
#ifdef _ANDROID_
extern "C" {
jint Java_com_bbmlf_nowuseeme_RtmpClientCtrl_stopBackground(JNIEnv* env,
		jobject thiz)
#else
int stopBackground()
#endif
{
	int ret= ERROR;
	LOGI("'stopBackground()' ...\n");
	pthread_mutex_lock(&muxer.video_mutex);
	pthread_mutex_lock(&muxer.bckground_mutex);

	/* Sanity check */
    if(muxer.inBackground== 0)
    {
    	LOGI("Error: muxer should be running in background!\n");
    	ret=  NOT_IN_BACKGROUND_ERROR;
    	goto end_jni_stopBackground;
    }

    rtmp_client_stopBackground(&muxer);

	/* Success */
	ret= SUCCESS;

end_jni_stopBackground:

	pthread_mutex_unlock(&muxer.video_mutex);
	pthread_mutex_unlock(&muxer.bckground_mutex);
	return ret;
}
#ifdef _ANDROID_
}
#endif

/**
 * Set connection time-out (connection error "tolerance" in seconds)
 */
#ifdef _ANDROID_
extern "C" {
void Java_com_bbmlf_nowuseeme_RtmpClientCtrl_connectionTimeOutSet(JNIEnv* env,
		jobject thiz, jint tout)
#else
void rtmpClientConnectionTimeOutSet(int tout)
#endif
{
	muxer.persistant_error_usecs_limit= tout*1000*1000;
}
#ifdef _ANDROID_
}
#endif

/**
 * Call-back
 */
#if 0 //RAL: not-used
extern "C" {
Java_com_bbmlf_nowuseeme_RtmpClientCtrl_onAMFMessage(const char* amf_msg)
{
    int status;
    JNIEnv *env;
    int isAttached= 0;
	LOGI("'onAMFMessage()...'\n");

	pthread_mutex_lock(&muxer.bckground_mutex); // mutual exclusion with background activity

	/* If application is running in background, virtual machine (VM) thread
	 * is stopped so we can not make JNI calls (it would cause the JNI error:
	 * "non-VM thread making JNI calls").
	 * */
	if(muxer.inBackground!= 0)
	{
		LOGW("We can not make JNI calls running in background\n");
		goto end_onAMFMessage;
	}

	/* Get global 'jobject' */
	if(!global_jni_objRtmpClientCtrl)
	{
		LOGE("Global 'jobject' not initialized!\n");
		goto end_onAMFMessage;
	}
	jobject obj= global_jni_objRtmpClientCtrl;

	/* Get global 'env' */
    if((status= global_jni_JavaVM->GetEnv((void**)&env, JNI_VERSION_1_6))< 0)
    {
        if((status= global_jni_JavaVM->AttachCurrentThread(&env, NULL))< 0)
        {
        	LOGE("Could not get global 'env'!\n");
        	goto end_onAMFMessage;
        }
        isAttached= 1;
    }

    /* Get Java Class */
    jclass clazz= env->FindClass("com/bbmlf/nowuseeme/RtmpClientCtrl");
    if(!clazz)
    {
    	LOGE("jclass not found!!\n");
    	goto end_onAMFMessage;
    }

    /* Get method */
    jmethodID onAMFMessage= env->GetMethodID(
    				clazz, "onAMFMessage", "(Ljava/lang/String;)V"
    				);
    if(!onAMFMessage)
    {
    	LOGE("jmethodID 'onAMFMessage' not found!\n");
    	goto end_onAMFMessage;
    }

    /* Get Java String */
    jstring jstr= env->NewStringUTF(amf_msg);
    if(!jstr)
    {
    	LOGE("Could not create jstring!\n");
    	goto end_onAMFMessage;
    }

    /* Call method */
    env->CallVoidMethod(obj, onAMFMessage, jstr);

end_onAMFMessage:

	if(isAttached!= 0) (*global_jni_JavaVM)->DetachCurrentThread(global_jni_JavaVM);
    pthread_mutex_unlock(&muxer.bckground_mutex); // mutex
    return;
}
}
#endif

static void onErrorCallback(void* t)
{
	pthread_attr_t attr;

	/* Get muxer context */
	rtmpMuxerCtx* muxer= (rtmpMuxerCtx*)t;

	// Initialize on-error thread in a detached state
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	// Launch thread
	LOGV("Launching onErrorCallback thread\n");
	pthread_create(&thread, &attr, thrError, (void*)muxer);

	pthread_attr_destroy(&attr);
}

static void *thrError(void *t)
{
#ifdef _ANDROID_
    JNIEnv *env= NULL;
    jclass clazz= NULL;
    jmethodID onError= NULL;
#endif
	LOGD("Executing 'thrError()'...\n");

	/* Get muxer context */
	rtmpMuxerCtx* muxer= (rtmpMuxerCtx*)t;

#ifdef _ANDROID_
	/* Attach current thread environment in order to use JNI->Java bridge */
	env= attach_current_thread();
	if(env== NULL)
	{
    	LOGE("Could not attach current thread\n");
    	goto end_thrError;
	}

	/* Get Java object class 'RtmpClientCtrl' to invoke java method */
	if(!global_jni_objRtmpClientCtrl)
	{
		LOGE("Global jobject for class 'RtmpClientCtrl' not initialized!\n");
		goto end_thrError;
	}
    if((clazz= env->GetObjectClass(global_jni_objRtmpClientCtrl))== NULL)
    {
    	LOGE("Jobject for class 'RtmpClientCtrl' not found\n");
    	goto end_thrError;
    }

    /* Get the java method 'RtmpClientCtrl.onError(int)' reference */
    if((onError= env->GetMethodID(clazz, "onError", "(I)V"))== NULL)
    {
    	LOGE("jmethodID 'RtmpClientCtrl.onError(int)' not found\n");
    	goto end_thrError;
    }

    /* Call 'RtmpClientCtrl.onError(int)' method */
    env->CallVoidMethod(global_jni_objRtmpClientCtrl, onError, muxer->error_code);
#else
    RtmpClientCtrl *rtmpclientctrl= (RtmpClientCtrl*)global_jni_objRtmpClientCtrl;
    rtmpclientctrl->onError(muxer->error_code);
#endif

end_thrError:

#ifdef _ANDROID_
	/* Detach current thread */
	if(env!= NULL)
	{
		detach_current_thread();
	}
#endif
	LOGD("Exiting 'thrError()'.\n");
	pthread_exit(NULL);
}
