#ifndef _ANDROID_
    #include "player-jni.h"
#endif

#include <pthread.h>
#include <string.h>
#include <unistd.h> //usleep

#include "global-jni.h"

extern "C" {
#include "log.h"
#include "error_codes.h"
#include "demuxer.h"
}

#ifdef _ANDROID_
#include <android/native_window.h> // requires ndk r5 or newer
#include <android/native_window_jni.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#else
extern "C" {
#include "AVViewDelegate_ciface.h"
}
#include "PlayerCtrl.h"
#endif

/* Definitions */

typedef struct displayCtx
{
    #ifdef _ANDROID_
        ANativeWindow* window;
        ANativeWindow* next_window;
    #else
        void* uiview;
    #endif
} displayCtx;

typedef struct audioCtx
{
    #ifdef _ANDROID_
        // native audio engine interfaces
        SLObjectItf engineObject;
        SLEngineItf engineEngine;
        
        // native audio output mix interfaces
        SLObjectItf outputMixObject;
        
        // native audio buffer queue player interfaces
        SLObjectItf bqPlayerObject;
        SLPlayItf bqPlayerPlay;
        SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
    #else
    #endif
} audioCtx;

/* Prototypes */
#ifdef _ANDROID_
extern "C" {
void Java_com_bbmlf_nowuseeme_PlayerView_playerStop(JNIEnv* env, jobject thiz);
}
#endif

int createDisplayEngineAndResize(void* t);
void videoEngineRelease(void* t);
int nativeVideoRenderRequest(void* t);

static int createAudioEngineAndPlayer(void* t);
static void audioEngineRelease(void* t);
static int nativeAudioRenderRequest0(void* t);

static void onErrorCallback(void* t);
static void *thrError(void *t);
static void onEvent(void* t);
static void *thrEvent(void *t);

/* Globals */
#ifdef _ANDROID_
static jobject global_jni_objPlayerCtrl= NULL;
#else
static void *global_jni_objPlayerCtrl= NULL;
#endif
static demuxerCtx *demuxer= NULL;
static displayCtx* display_ctx= NULL;
static audioCtx* audio_ctx= NULL;
static pthread_t thread, thread_event;
static int global_persistant_error_msecs_limit=
		PERSISTANT_ERROR_TOLERANCE_RFRAME_MSECS; // default value

#ifdef _ANDROID_
/**
 * This native method is called at 'PlayerCtrl' initialization to globally
 * set, at JNI level, the 'jobject' corresponding to the PlayerCtrl class.
 * Later, this object may be used to call a method of
 * the "java layer" from JNI (typically used for events coming from JNI to
 * Java layer).
 */
extern "C" {
void Java_com_bbmlf_nowuseeme_PlayerCtrl_setJavaObj(JNIEnv* env, jobject thiz)
{
	LOGI("Executing 'Java_com_bbmlf_nowuseeme_PlayerCtrl_setJavaObj()...'\n");
	global_jni_objPlayerCtrl= env->NewGlobalRef(thiz);
	LOGI("Exiting 'Java_com_bbmlf_nowuseeme_PlayerCtrl_setJavaObj()...'\n");
}
}

/**
 * Call this native method at 'PlayerCtrl' de-initialization in order to
 * release globals previously allocated.
 */
extern "C" {
void Java_com_bbmlf_nowuseeme_PlayerCtrl_releaseJavaObj(JNIEnv* env, jobject thiz)
{
	LOGI("Executing 'Java_com_bbmlf_nowuseeme_PlayerCtrl_releaseJavaObj()...'\n");
    if(global_jni_objPlayerCtrl!= NULL)
    {
    	env->DeleteGlobalRef(global_jni_objPlayerCtrl);
    	global_jni_objPlayerCtrl= NULL;
    }
    LOGI("Exiting 'Java_com_bbmlf_nowuseeme_PlayerCtrl_releaseJavaObj()...'\n");
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
void setPlayerCtrlObj(void *t)
{
    LOGI("Executing 'setPlayerCtrlObj()...'\n");
    global_jni_objPlayerCtrl= t;
    LOGI("Exiting 'setPlayerCtrlObj()...'\n");
}
void releasePlayerCtrlObj()
{
    LOGI("Executing 'releasePlayerCtrlObj()...'\n");
    if(global_jni_objPlayerCtrl!= NULL)
    {
        global_jni_objPlayerCtrl= NULL;
    }
    LOGI("Exiting 'releasePlayerCtrlObj()...'\n");
}
#endif //#ifdef _ANDROID_

/* ===========================================================================
 * VIDEO RELATED METHODS
 * ===========================================================================
 */

/* Initialize video rendering context for the current display and resize */
int createDisplayEngineAndResize(void* t)
{
    displayCtx* c;
	int ret= COULD_NOT_START_PLAYER;

	// Get demuxer context
	demuxerCtx* demuxer= (demuxerCtx*)t;
    if(demuxer== NULL)
    {
        LOGE("\n");
        goto end_createDisplayEngineAndResize;
    }
    
    // Get display context structure
    c= (displayCtx*)demuxer->presentat_ctx[MM_VIDEO]->opaque;
    if(c== NULL)
    {
        LOGE("\n");
        goto end_createDisplayEngineAndResize;
    }


	LOGD("Executing 'createDisplayEngineAndResize()'...\n");

#ifdef _ANDROID_
	    ANativeWindow_setBuffersGeometry(c->window,
	    		ANativeWindow_getWidth(c->window),
	    		ANativeWindow_getHeight(c->window),
	    		WINDOW_FORMAT_RGB_565);
#else
#endif

	LOGD("'createDisplayEngineAndResize()' successfully executed.\n");
	ret= SUCCESS;

end_createDisplayEngineAndResize:

	if(ret!= SUCCESS)
	{
		//videoEngineRelease(t);
		demuxer->error_code= ret;
		if(demuxer->on_error) demuxer->on_error((void*)demuxer);
	}
	return ret;
}

/* Release EGL context */
void videoEngineRelease(void *t)
{
    LOGD("Executing 'videoEngineRelease()...'\n");

	// Get demuxer context
	demuxerCtx* demuxer= (demuxerCtx*)t;
    if(demuxer== NULL)
    {
        LOGE("\n");
        return;
    }

	// Get display context structure
	displayCtx* c= (displayCtx*)demuxer->presentat_ctx[MM_VIDEO]->opaque;
    if(c== NULL)
    {
        LOGE("\n");
        return;
    }

    #ifdef _ANDROID_
        if(c->window!= NULL)
        {
            ANativeWindow_release(c->window);
            c->window= NULL;
        }
    #else
    #endif

    return;
}

/* Render a video frame using OpenGL library */
int nativeVideoRenderRequest(void* t)
{
    displayCtx* c;
	int end_code= ERROR;
	//LOGV("\n"); // comment-me

	// Get demuxer context
	demuxerCtx* demuxer= (demuxerCtx*)t;
    if(demuxer== NULL)
    {
        goto end_nativeVideoRenderRequest;
    }

	// Get display context structure
	c= (displayCtx*)demuxer->presentat_ctx[MM_VIDEO]->opaque;
    if(c== NULL)
    {
        goto end_nativeVideoRenderRequest;
    }

	if(demuxer->can_run_demux== 0 ||
	   demuxer->pFrameConverted== NULL)
	{
		LOGW("Video player input buffer still not initialized!\n");
		goto end_nativeVideoRenderRequest;
	}

	if(demuxer->presentat_ctx[MM_VIDEO]->reset== 1)
	{
		LOGD("Reseting presentation\n");
		demuxer->presentat_ctx[MM_VIDEO]->reset= 0;

#ifdef _ANDROID_
		if(demuxer->pFrameConverted!= NULL)
		{
			if(demuxer->pFrameConverted->data[0]!= NULL)
			{
				av_free(demuxer->pFrameConverted->data[0]);
				demuxer->pFrameConverted->data[0]= NULL;
			}
		}
		if(demuxer->img_convert_ctx!= NULL)
		{
			sws_freeContext(demuxer->img_convert_ctx);
			demuxer->img_convert_ctx= NULL;
		}
#endif

	    // Release previous player rendering engine instance
		videoEngineRelease(demuxer);

		// Set new player parameters
		demuxer->v_width= demuxer->presentat_ctx[MM_VIDEO]->next_width;
		demuxer->v_height= demuxer->presentat_ctx[MM_VIDEO]->next_height;
        #ifdef _ANDROID_
            c->window= c->next_window;
        #else
        #endif

#ifdef _ANDROID_
		// Allocate new video output frame buffer
        AVPixelFormat dst_pix_fmt= AV_PIX_FMT_RGB565;
    	avpicture_alloc(demuxer->pFrameConverted, dst_pix_fmt, demuxer->v_width, demuxer->v_height);
		LOGD("New video player resolution: %dx%d\n", demuxer->v_width, demuxer->v_height);
#endif

		// Create new player instance
		createDisplayEngineAndResize(demuxer);
		return SUCCESS;
	}

    #ifdef _ANDROID_
        ANativeWindow_Buffer buffer;
        if(ANativeWindow_lock(c->window, &buffer, NULL)== 0)
        {
            //LOGV("w: %d, h: %d, stride: %d, format: %d\n", buffer.width,
            //		buffer.height, buffer.stride, buffer.format); //comment-me
            if(buffer.width== demuxer->v_width && buffer.height== demuxer->v_height)
            {
                if(buffer.width== buffer.stride)
                {
                    memcpy(buffer.bits, demuxer->pFrameConverted->data[0],
                            demuxer->v_width* demuxer->v_height* 2);
                }
                else
                {
                    int i;
                    for(i= 0; i< demuxer->v_height; i++)
                        memcpy(&((uint8_t*)(buffer.bits))[i* buffer.stride* 2],
                               &demuxer->pFrameConverted->data[0][i* demuxer->v_width* 2],
                               demuxer->v_width* 2);
                }
            }
            if(ANativeWindow_unlockAndPost(c->window)< 0)
            {
                LOGE("Native Window unlock and post failed\n");
            }
        }
    #else
        //TODO: iOS
        //{ //deleteme: debugging purposes
        #if 0
            static int cnt= 0;
            if(cnt++>= 60)
            {
                LOGV("w: %d, h: %d, stride: %d, p: %p\n",
                     demuxer->v_width, demuxer->v_height,
                     demuxer->pFrameConverted->linesize[0], demuxer->pFrameConverted->data[0]); //comment-me
                cnt= 0;
            }
        #endif
        //}
        renderOnPlayerSublayer((void*)demuxer->pFrameConverted->data[0],
                               demuxer->v_width, demuxer->v_height,
                               (void*)demuxer->pFrameConverted->data[1],
                               (void*)demuxer->pFrameConverted->data[2],
                               &demuxer->presentat_ctx[MM_VIDEO]->mutex,
                               c->uiview);
    #endif

	//LOGV("\n"); // comment-me
	end_code= SUCCESS;

end_nativeVideoRenderRequest:

	return end_code;
}

/* ===========================================================================
 * AUDIO RELATED METHODS
 * ===========================================================================
 */
#ifdef _ANDROID_

#define FIRST_PASSES_VAL 2
static int first_passes= FIRST_PASSES_VAL;

// this callback handler is called every time a buffer finishes playing
static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    // Get demuxer context
    demuxerCtx* demuxer= (demuxerCtx*)context;
    
    // lock decoding thread
    pthread_mutex_lock(&demuxer->presentat_ctx[MM_AUDIO]->mutex);
    
    //LOGV("*** flush\n"); //comment-me
    demuxer->output_buffer_level--;
    
    //Hack: schedule
    if(first_passes-->= 0)
    {
        int sample_freq_usecs= (1000*1000)/ demuxer->audio_dec_ctx->sample_rate;
        sample_freq_usecs*= (demuxer->audio_buf_size/2);
        //LOGV("sample-freq: %d[msecs]\n", sample_freq_usecs/1000); //comment-me
        usleep(sample_freq_usecs);
    }
    
    // signal/release decoding thread
    demuxer->presentat_ctx[MM_AUDIO]->ready= 1;
    pthread_cond_signal(&demuxer->presentat_ctx[MM_AUDIO]->consume_cond);
    
    // unlock decoding thread
    pthread_mutex_unlock(&demuxer->presentat_ctx[MM_AUDIO]->mutex);
}

static SLresult createAudioEngineAndPlayer_Android(demuxerCtx* demuxer, audioCtx* c)
{
    SLresult result;
    int sample_rate= SL_SAMPLINGRATE_44_1; //default
    int ret= COULD_NOT_START_PLAYER;

    // create engine
    result= slCreateEngine(&c->engineObject, 0, NULL, 0, NULL, NULL);
    if(result!= SL_RESULT_SUCCESS) return ret;
    
    // realize the engine
    result= (*c->engineObject)->Realize(c->engineObject, SL_BOOLEAN_FALSE);
    if(result!= SL_RESULT_SUCCESS) return ret;
    
    // get the engine interface, which is needed in order to create other objects
    result= (*c->engineObject)->GetInterface(c->engineObject, SL_IID_ENGINE, &c->engineEngine);
    if(result!= SL_RESULT_SUCCESS) return ret;
    
    // create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids[1]= {SL_IID_VOLUME};
    const SLboolean req[1]= {SL_BOOLEAN_FALSE};
    result= (*c->engineEngine)->CreateOutputMix(c->engineEngine, &c->outputMixObject, 1, ids, req);
    if(result!= SL_RESULT_SUCCESS) return ret;
    
    // realize the output mix
    result= (*c->outputMixObject)->Realize(c->outputMixObject, SL_BOOLEAN_FALSE);
    if(result!= SL_RESULT_SUCCESS) return ret;
    
    //==== create buffer queue audio player ====
    
    // configure audio source
    switch(demuxer->audio_dec_ctx->sample_rate)
    {
        case 8000:
            sample_rate= SL_SAMPLINGRATE_8;
            break;
        case 11025:
            sample_rate= SL_SAMPLINGRATE_11_025;
            break;
        case 22050:
            sample_rate= SL_SAMPLINGRATE_22_05;
            break;
        case 44100:
            sample_rate= SL_SAMPLINGRATE_44_1;
            break;
        default:
            break;
    }
    LOGD("input sample rate is: %d\n", sample_rate);
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq= {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
        NUM_ABUFS
    };
    SLDataFormat_PCM format_pcm= {
        SL_DATAFORMAT_PCM,
        1,
        sample_rate,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_SPEAKER_FRONT_CENTER,
        SL_BYTEORDER_LITTLEENDIAN
    };
    SLDataSource audioSrc= {&loc_bufq, &format_pcm};
    
    // configure audio sink
    SLDataLocator_OutputMix loc_outmix= {SL_DATALOCATOR_OUTPUTMIX, c->outputMixObject};
    SLDataSink audioSnk= {&loc_outmix, NULL};
    
    const SLInterfaceID ids2[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req2[1] = {SL_BOOLEAN_TRUE};
    result= (*c->engineEngine)->CreateAudioPlayer(c->engineEngine, &c->bqPlayerObject, &audioSrc, &audioSnk, 1, ids2, req2);
    if(result!= SL_RESULT_SUCCESS) return ret;
    
    // realize the player
    result= (*c->bqPlayerObject)->Realize(c->bqPlayerObject, SL_BOOLEAN_FALSE);
    if(result!= SL_RESULT_SUCCESS) return ret;
    
    // get the play interface
    result= (*c->bqPlayerObject)->GetInterface(c->bqPlayerObject, SL_IID_PLAY, &c->bqPlayerPlay);
    if(result!= SL_RESULT_SUCCESS) return ret;
    
    // get the buffer queue interface
    result= (*c->bqPlayerObject)->GetInterface(c->bqPlayerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &c->bqPlayerBufferQueue);
    if(result!= SL_RESULT_SUCCESS) return ret;
    
    // register callback on the buffer queue
    result= (*c->bqPlayerBufferQueue)->RegisterCallback(c->bqPlayerBufferQueue, bqPlayerCallback, demuxer);
    if(result!= SL_RESULT_SUCCESS) return ret;
    
    // set the player's state to playing
    //result= (*c->bqPlayerPlay)->SetPlayState(c->bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    //if(result!= SL_RESULT_SUCCESS) goto end_createAudioEngine;

    return SUCCESS;
}
#else
static int createAudioEngineAndPlayer_Darwin(demuxerCtx* demuxer, audioCtx* c)
{
    return (avviewdelegate_createAudioEngineAndPlayer(demuxer)== 0)? SUCCESS: ERROR;
}
#endif

/* create the audio engine and create buffer queue audio player */
static int createAudioEngineAndPlayer(void *t)
{
	int ret;

	// Get demuxer context
	demuxerCtx* demuxer= (demuxerCtx*)t;

	// Get display context structure
	audioCtx* c= (audioCtx*)demuxer->presentat_ctx[MM_AUDIO]->opaque;

    LOGD("Executing 'createAudioEngineAndPlayer()'...\n");

#ifdef _ANDROID_
    ret= createAudioEngineAndPlayer_Android(demuxer, c);
#else
    ret= createAudioEngineAndPlayer_Darwin(demuxer, c);
#endif

	if(ret!= SUCCESS)
	{
		//audioEngineRelease(t);
		demuxer->error_code= ret;
		if(demuxer->on_error) demuxer->on_error((void*)demuxer);
	}
    else
    {
        LOGD("'createAudioEngineAndPlayer()' successfully executed.\n");
    }
	return ret;
}

#ifdef _ANDROID_
static void audioEngineRelease_Android(demuxerCtx* demuxer, audioCtx* c)
{
    if(c== NULL)
        return;

    // destroy buffer queue audio player object, and invalidate all associated interfaces
    if(c->bqPlayerObject!= NULL)
    {
        (*c->bqPlayerObject)->Destroy(c->bqPlayerObject);
        c->bqPlayerObject= NULL;
        c->bqPlayerPlay= NULL;
        c->bqPlayerBufferQueue= NULL;
    }
    
    // destroy output mix object, and invalidate all associated interfaces
    if(c->outputMixObject!= NULL)
    {
        (*c->outputMixObject)->Destroy(c->outputMixObject);
        c->outputMixObject= NULL;
    }
    
    // destroy engine object, and invalidate all associated interfaces
    if(c->engineObject!= NULL)
    {
        (*c->engineObject)->Destroy(c->engineObject);
        c->engineObject= NULL;
        c->engineEngine= NULL;
    }
}
#else
static void audioEngineRelease_Darwin(demuxerCtx* demuxer, audioCtx* c)
{
    avviewdelegate_audioEngineRelease();
}
#endif

// shut down the native audio system
static void audioEngineRelease(void* t)
{
	LOGD("Executing 'audioEngineRelease()...'\n");

	// Get demuxer context
	demuxerCtx* demuxer= (demuxerCtx*)t;

	// Get display context structure
	audioCtx* c= (audioCtx*)demuxer->presentat_ctx[MM_AUDIO]->opaque;

#ifdef _ANDROID_
    audioEngineRelease_Android(demuxer, c);
#else
    audioEngineRelease_Darwin(demuxer, c);
#endif
}

#ifdef _ANDROID_
static int nativeAudioRenderRequest(void* t)
{
    // Get demuxer context
    demuxerCtx* demuxer= (demuxerCtx*)t;
    
    // Get display context structure
    audioCtx* c= (audioCtx*)demuxer->presentat_ctx[MM_AUDIO]->opaque;
    
    // Update output audio-buffer level
    //LOGV("buffer number: %d; output level: %d\n",
    //		demuxer->num_abuf, demuxer->output_buffer_level); //comment-me
    
    /* Treat overflow case: wait for presentation to release one buffer */
    if(NUM_ABUFS>1 && demuxer->output_buffer_level>= NUM_ABUFS- 1)
    {
        //LOGW("**** SL_RESULT_BUFFER_INSUFFICIENT ****\n"); //comment-me
        pthread_mutex_lock(&demuxer->presentat_ctx[MM_AUDIO]->mutex);
        
        // wait for presentation call-back signal a buffer has been released
        while(demuxer->presentat_ctx[MM_AUDIO]->ready!= 1)
            pthread_cond_wait(&demuxer->presentat_ctx[MM_AUDIO]->consume_cond,
                              &demuxer->presentat_ctx[MM_AUDIO]->mutex);
        demuxer->presentat_ctx[MM_AUDIO]->ready= 0;
        
        pthread_mutex_unlock(&demuxer->presentat_ctx[MM_AUDIO]->mutex);
    }
    
    if(demuxer->audio_mute)
    {
        /* Put PCM samples to zero to mute audio */
        memset((void*)demuxer->audio_buf[demuxer->num_abuf][0], 0,
               demuxer->audio_buf_size);
    }
    while(
    		(*c->bqPlayerBufferQueue)->Enqueue(c->bqPlayerBufferQueue,
    				(void*)demuxer->audio_buf[demuxer->num_abuf][0],
    				demuxer->audio_buf_size)== SL_RESULT_BUFFER_INSUFFICIENT &&
    				demuxer->can_run_demux== 1
    )
    {
    	// paranoid
    	int sample_freq_usecs= (1000*1000)/ demuxer->audio_dec_ctx->sample_rate;
        sample_freq_usecs*= (demuxer->audio_buf_size/2);
        //LOGV("sample-freq: %d[msecs]\n", sample_freq_usecs/1000); //comment-me
        usleep(sample_freq_usecs);
    }
    demuxer->output_buffer_level++;
    
    /* Treat under-run case: refill buffers */
    if(demuxer->output_buffer_level<= 0)
    {
        LOGW("**** AUDIO BUFFER UNDERUN ****\n");
        
        // set the player's state to pause
        if(((*c->bqPlayerPlay)->SetPlayState(c->bqPlayerPlay,
                                             SL_PLAYSTATE_PAUSED))!= SL_RESULT_SUCCESS) {
            LOGE("Could not set player state to 'SL_PLAYSTATE_PAUSED'\n");
        }
        
        // Change to loop function
        demuxer->presentat_ctx[MM_AUDIO]->render= nativeAudioRenderRequest0;
    }
    return SUCCESS;
}

/* this function is only executed in first pass */
int nativeAudioRenderRequest0(void* t)
{
    // Get demuxer context
    demuxerCtx* demuxer= (demuxerCtx*)t;
    
    // Get display context structure
    audioCtx* c= (audioCtx*)demuxer->presentat_ctx[MM_AUDIO]->opaque;
    
    // Update output audio-buffer level
    demuxer->output_buffer_level++;
    //LOGV("buffer number: %d; output level: %d\n",
    //		demuxer->num_abuf, demuxer->output_buffer_level); //comment-me
    
    if(c->engineObject!=NULL && demuxer->audio_buf_size> 0 &&
       demuxer->output_buffer_level<= NUM_ABUFS)
    {
        if(demuxer->audio_mute)
        {
            /* Put PCM samples to zero to mute audio */
            memset((void*)demuxer->audio_buf[demuxer->num_abuf][0], 0,
            		demuxer->audio_buf_size);
        }
        (*c->bqPlayerBufferQueue)->Enqueue(c->bqPlayerBufferQueue,
        		(void*)demuxer->audio_buf[demuxer->num_abuf][0],
        		demuxer->audio_buf_size);
    }

    if(demuxer->output_buffer_level> NUM_ABUFS)
    {
    	// set the player's state to playing
        if(((*c->bqPlayerPlay)->SetPlayState(c->bqPlayerPlay,
                                             SL_PLAYSTATE_PLAYING))!= SL_RESULT_SUCCESS) {
            LOGE("Could not set player state to 'SL_PLAYSTATE_PLAYING', "
                 "try again...\n");
            return ERROR;
        }
        
        // Change to loop function
        first_passes= FIRST_PASSES_VAL; // HACK
        demuxer->presentat_ctx[MM_AUDIO]->render= nativeAudioRenderRequest;
    }
    return SUCCESS;
}
#else
// this callback handler is called every time a buffer finishes playing
void playerCallback(void* demuxer_ctx, void *rendering_buf, int expected_size)
{
    // Get demuxer context
    demuxerCtx* demuxer= (demuxerCtx*)demuxer_ctx;

    // lock decoding thread
    pthread_mutex_lock(&demuxer->presentat_ctx[MM_AUDIO]->mutex);

    /* Treat underrun cases:
     * - On underrun: enter buffering phase again
     */
    if(demuxer->audio_buf_levels[demuxer->num_oabuf]<= 0)
    {
        LOGW("**** AUDIO BUFFER UNDERUN ****\n");
        /*while(demuxer->output_buffer_level< (NUM_ABUFS- 1) && demuxer->can_run_demux)
        {
            // TODO: set the player's state to pause
            pthread_cond_wait(&demuxer->presentat_ctx[MM_AUDIO]->produce_cond,
                              &demuxer->presentat_ctx[MM_AUDIO]->mutex);
        }*/
        demuxer->underrun_flag= 1;
    }
    if(demuxer->underrun_flag && demuxer->output_buffer_level< (NUM_ABUFS- 1))
    {
        memset(rendering_buf, 0, expected_size); // render "silence"
    }
    else
    {
        //LOGV("Render buffer number: %d\n", demuxer->num_oabuf); //comment-me
        memcpy(rendering_buf,
               (const void*)demuxer->audio_buf[demuxer->num_oabuf][0],
               expected_size); //FIXME!!

        demuxer->audio_buf_levels[demuxer->num_oabuf]= 0;
        demuxer->output_buffer_level--;

        demuxer->num_oabuf= (++demuxer->num_oabuf)%NUM_ABUFS; // Circular output buffers
    }

    // signal/release decoding thread
    pthread_cond_signal(&demuxer->presentat_ctx[MM_AUDIO]->consume_cond);

    // unlock decoding thread
    pthread_mutex_unlock(&demuxer->presentat_ctx[MM_AUDIO]->mutex);
}

static int nativeAudioRenderRequest0(void* t)
{
    int next_num_abuf;

    // Get demuxer context
    demuxerCtx* demuxer= (demuxerCtx*)t;
    
    // Get display context structure
    //audioCtx* c= (audioCtx*)demuxer->presentat_ctx[MM_AUDIO]->opaque;

    pthread_mutex_lock(&demuxer->presentat_ctx[MM_AUDIO]->mutex);

    demuxer->audio_buf_levels[demuxer->num_abuf]= (int)demuxer->audio_buf_size;
    demuxer->output_buffer_level++;

    /* Treat overflow cases:
     * - On overflow: wait for presentation call-back to release a buffer
     */
    next_num_abuf= (demuxer->num_abuf+ 1)% NUM_ABUFS;
    while(demuxer->audio_buf_levels[next_num_abuf]> 0 && demuxer->can_run_demux)
    {
        //LOGW("**** AUDIO BUFFER OVERFLOW ****\n"); //comment-me
        pthread_cond_wait(&demuxer->presentat_ctx[MM_AUDIO]->consume_cond,
                          &demuxer->presentat_ctx[MM_AUDIO]->mutex);
    }
    //LOGV("Output buffer number: %d\n", demuxer->num_abuf); //comment-me

    pthread_cond_signal(&demuxer->presentat_ctx[MM_AUDIO]->produce_cond);

    pthread_mutex_unlock(&demuxer->presentat_ctx[MM_AUDIO]->mutex);

    // New output buffer was enqueued
    return SUCCESS;
}
#endif

/* ===========================================================================
 * PLAYER CONTROL METHODS
 * ===========================================================================
 */

/* Start player */
#ifdef _ANDROID_
extern "C" {
int Java_com_bbmlf_nowuseeme_PlayerView_playerStart(JNIEnv* env, jobject thiz,
		jbyteArray url, jint width, jint height, jint isMuteOn, jobject surface)
#else
int playerStart(const char *url, int width, int height, int isMuteOn, void *surface)
#endif
{
    demuxerInitCtx *demuxer_init_ctx;
    #ifdef _ANDROID_
        jboolean isCopy;
        jbyte* _url= env->GetByteArrayElements(url, &isCopy);
        jsize _url_length= env->GetArrayLength(url);
    #else
        const char *_url= url;
        size_t _url_length= strlen(url);
    #endif
    int end_code= COULD_NOT_START_PLAYER;

	LOGD("Executing '%s()' ...\n", __func__);

	// Sanity check
	if(demuxer!= NULL || display_ctx!= NULL || audio_ctx!= NULL)
	{
		// demuxer should be NULL (probably previous 'start' failed)
		// We better release before starting
        #ifdef _ANDROID_
            Java_com_bbmlf_nowuseeme_PlayerView_playerStop(env, thiz);
        #else
            playerStop();
        #endif
	}

	// Allocate demuxer context
	demuxer= (demuxerCtx*)calloc(1, sizeof(demuxerCtx));
	if(demuxer== NULL)
	{
		LOGE("Could not allocate demuxer context\n");
		goto end_playerStart;
	}

    // Initialize rendering context structures
    display_ctx= (displayCtx*)malloc(sizeof(displayCtx));
    if(display_ctx== NULL)
    {
        LOGE("Could not allocate video rendering context structure\n");
        goto end_playerStart;
    }
    #ifdef _ANDROID_
        display_ctx->window= (ANativeWindow*)ANativeWindow_fromSurface((JNIEnv*)env, (jobject)surface);
        display_ctx->next_window= NULL;
    #else
        display_ctx->uiview= surface;
    #endif
    
    audio_ctx= (audioCtx*)malloc(sizeof(audioCtx));
    if(audio_ctx== NULL)
    {
        LOGE("Could not allocate audio rendering context structure\n");
        goto end_playerStart;
    }
    #ifdef _ANDROID_
        audio_ctx->engineObject= NULL;
        audio_ctx->outputMixObject= NULL;
        audio_ctx->bqPlayerObject= NULL;
    #else
    #endif

    // Allocate demuxer initialization context
    demuxer_init_ctx= (demuxerInitCtx*)calloc(1, sizeof(demuxerInitCtx));
    if(demuxer_init_ctx== NULL)
    {
        LOGE("Could not allocate demuxer context\n");
        goto end_playerStart;
    }

    demuxer_init_ctx->url= (const char*)_url;
    demuxer_init_ctx->url_length= _url_length;
    demuxer_init_ctx->width= width;
    demuxer_init_ctx->height= height;
    demuxer_init_ctx->isMuteOn= isMuteOn? 1: 0;
    demuxer_init_ctx->callbacksVideo.prologue= createDisplayEngineAndResize;
    demuxer_init_ctx->callbacksVideo.epilogue= videoEngineRelease;
    demuxer_init_ctx->callbacksVideo.render= nativeVideoRenderRequest;
    demuxer_init_ctx->callbacksVideo.opaque= display_ctx;
    demuxer_init_ctx->callbacksAudio.prologue= createAudioEngineAndPlayer;
    demuxer_init_ctx->callbacksAudio.epilogue= audioEngineRelease;
    demuxer_init_ctx->callbacksAudio.render= nativeAudioRenderRequest0;
    demuxer_init_ctx->callbacksAudio.opaque= audio_ctx;
    demuxer_init_ctx->scale_video_frame= NULL; // Not used
    demuxer_init_ctx->on_error= onErrorCallback;
    demuxer_init_ctx->persistant_error_msecs_limit= global_persistant_error_msecs_limit;
    demuxer_init_ctx->on_event= onEvent;

	// Launch demuxer threads
	if(demuxer_init(demuxer, demuxer_init_ctx)!= SUCCESS)
	{
		goto end_playerStart;
	}

    // We don't need 'demuxer initialization context' anymore; release it.
    // NOTE: demuxer context and rendering context structures will be used by the
    // the demultiplexer threads and should be kept until the demultiplexer is
    // released.
    if(demuxer_init_ctx!= NULL)
    {
        free(demuxer_init_ctx);
        demuxer_init_ctx= NULL;
    }

	LOGD("Player successfully initialized\n");
	end_code= SUCCESS;

end_playerStart:

	if(end_code!= SUCCESS)
	{
		LOGE("Could not initialize player\n");
		//Java_com_bbmlf_nowuseeme_PlayerView_playerStop(env, thiz);
	}

	demuxer->error_code= end_code;
	if(demuxer->on_error) demuxer->on_error((void*)demuxer);

	// Release local resources
    #ifdef _ANDROID_
        env->ReleaseByteArrayElements(url, _url, JNI_ABORT);
    #endif
    return SUCCESS;
}
#ifdef _ANDROID_
}
#endif

/* Stop player */
#ifdef _ANDROID_
extern "C" {
void Java_com_bbmlf_nowuseeme_PlayerView_playerStop(JNIEnv* env, jobject thiz)
#else
void playerStop()
#endif
{
	LOGD("Executing 'playerStop()' ...\n");

	if(demuxer== NULL)
	{
		LOGW("Player was already stopped\n");
		return;
	}
	demuxer_release(demuxer);
	if(demuxer!= NULL)
	{
		free(demuxer);
		demuxer= NULL;
	}
    if(display_ctx!= NULL) {
        free(display_ctx);
        display_ctx= NULL;
    }
    if(audio_ctx!= NULL)
    {
        free(audio_ctx);
        audio_ctx= NULL;
    }
	LOGD("'playerStop()' succeed\n");
}
#ifdef _ANDROID_
}
#endif

/* Mute player audio */
#ifdef _ANDROID_
extern "C" {
void Java_com_bbmlf_nowuseeme_PlayerView_playerMute(JNIEnv* env, jobject thiz, jint mute)
#else
void playerMute(int mute)
#endif
{
	if(!demuxer)
	{
		return;
	}

	LOGV("'playerMute()' set to %s\n", mute? "true": "false");
	if(mute)
	{
		demuxer->audio_mute= 1;
	}
	else
	{
		demuxer->audio_mute= 0;
	}

}
#ifdef _ANDROID_
}
#endif

#ifdef _ANDROID_
extern "C" {
void Java_com_bbmlf_nowuseeme_PlayerView_resetSurface(JNIEnv* env, jobject thiz,
		jint width, jint height, jobject surface)
#else
void resetSurface(int width, int height, void *surface)
#endif
{
	LOGD("Executing 'resetSurface()' ...\n");

	/* Sanity check */
	if(!demuxer || !demuxer->presentat_ctx[MM_VIDEO] ||
			!demuxer->presentat_ctx[MM_VIDEO]->opaque)
	{
		//LOGV("demuxerp: %p; presentation ctx: %p; presentation private data: %p\n",
		//		demuxer,
		//		demuxer? demuxer->presentat_ctx[MM_VIDEO]: NULL,
		//		demuxer && demuxer->presentat_ctx[MM_VIDEO]?
		//		demuxer->presentat_ctx[MM_VIDEO]->opaque: NULL); //comment-me
		return;
	}

	/* Get context structures */
	thrPresentationCtx* pc= (thrPresentationCtx*)demuxer->presentat_ctx[MM_VIDEO];
	displayCtx* dc= (displayCtx*)pc->opaque;

#ifdef _ANDROID_
	dc->next_window= (ANativeWindow*)ANativeWindow_fromSurface((JNIEnv*)env, (jobject)surface);
#else
    //TODO: iOS
#endif
	pc->next_width= width;
	pc->next_height= height;
	pc->reset= 1;
	LOGD("Exiting 'resetSurface()'.\n");
}
#ifdef _ANDROID_
}
#endif

/**
 * Set connection time-out (connection error "tolerance" in seconds)
 */
#ifdef _ANDROID_
extern "C" {
void Java_com_bbmlf_nowuseeme_PlayerCtrl_connectionTimeOutSet(JNIEnv* env,
		jobject thiz, jint tout)
#else
void playerConnectionTimeOutSet(int tout)
#endif
{
	global_persistant_error_msecs_limit= tout*1000;
	if(demuxer) demuxer->persistant_error_msecs_limit=
			global_persistant_error_msecs_limit;

}
#ifdef _ANDROID_
}
#endif

static void onErrorCallback(void* t)
{
	pthread_attr_t attr;

	/* Get demuxer context */
	demuxerCtx* demuxer= (demuxerCtx*)t;

	// Initialize on-error thread in a detached state
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	// Launch thread
	LOGV("Launching onErrorCallback thread\n");
	if(demuxer->error_code!= SUCCESS)
		demuxer->can_run_demux= 0;
	pthread_create(&thread, &attr, thrError, (void*)demuxer);

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

	/* Get demuxer context */
	demuxerCtx* demuxer= (demuxerCtx*)t;

#ifdef _ANDROID_
	/* Attach current thread environment in order to use JNI->Java bridge */
	env= attach_current_thread();
	if(env== NULL)
	{
    	LOGE("Could not attach current thread\n");
    	goto end_thrError;
	}

	/* Get Java object class 'PlayerCtrl' to invoke java method */
	if(!global_jni_objPlayerCtrl)
	{
		LOGE("Global jobject for class 'PlayerCtrl' not initialized!\n");
		goto end_thrError;
	}
    if((clazz= env->GetObjectClass(global_jni_objPlayerCtrl))== NULL)
    {
    	LOGE("Jobject for class 'PlayerCtrl' not found\n");
    	goto end_thrError;
    }

    /* Get the java method 'PlayerCtrl.onError(int)' reference */
    if((onError= env->GetMethodID(clazz, "onError", "(I)V"))== NULL)
    {
    	LOGE("jmethodID 'PlayerCtrl.onError(int)' not found\n");
    	goto end_thrError;
    }

    /* Call 'PlayerCtrl.onError(int)' method */
    env->CallVoidMethod(global_jni_objPlayerCtrl, onError, demuxer->error_code);
#else
    PlayerCtrl *playerctrl= (PlayerCtrl*)global_jni_objPlayerCtrl;
    if(playerctrl)
        playerctrl->onError(demuxer->error_code);
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

static void onEvent(void* t)
{
	pthread_attr_t attr;

	/* Get demuxer context */
	demuxerCtx* demuxer= (demuxerCtx*)t;

	// Initialize on-event thread in a detached state
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	// Launch thread
	LOGV("Launching onEvent thread\n");
	pthread_create(&thread_event, &attr, thrEvent, (void*)demuxer);

	pthread_attr_destroy(&attr);
}

static void *thrEvent(void *t)
{
#ifdef _ANDROID_
    JNIEnv *env= NULL;
    jclass clazz= NULL;
#endif
	LOGD("Executing 'thrEvent()'...\n");

	/* Get demuxer context */
	demuxerCtx* demuxer= (demuxerCtx*)t;

#ifdef _ANDROID_
	/* Attach current thread environment in order to use JNI->Java bridge */
	env= attach_current_thread();
	if(env== NULL)
	{
    	LOGE("Could not attach current thread\n");
    	goto end_thrError;
	}

	/* Get Java object class 'PlayerCtrl' to invoke java method */
	if(!global_jni_objPlayerCtrl)
	{
		LOGE("Global jobject for class 'PlayerCtrl' not initialized!\n");
		goto end_thrError;
	}
    if((clazz= env->GetObjectClass(global_jni_objPlayerCtrl))== NULL)
    {
    	LOGE("Jobject for class 'PlayerCtrl' not found\n");
    	goto end_thrError;
    }
#else
    PlayerCtrl *playerctrl= (PlayerCtrl*)global_jni_objPlayerCtrl;
#endif

    switch(demuxer->event_code)
    {
    case EVENT_IRESOLUTION_CHANGED:
    {
#ifdef _ANDROID_
    	jmethodID onInputResolutionChanged= NULL;

        /* Get the java method 'PlayerCtrl.onError(int)' reference */
        if((onInputResolutionChanged=
        		env->GetMethodID(clazz, "onInputResolutionChanged", "(II)V"))== NULL)
        {
        	LOGE("jmethodID 'PlayerCtrl.onInputResolutionChanged(int,int)' not found\n");
        	goto end_thrError;
        }

        /* Call 'PlayerCtrl.onError(int)' method */
        env->CallVoidMethod(global_jni_objPlayerCtrl, onInputResolutionChanged,
        		demuxer->video_dec_ctx->width,
        		demuxer->video_dec_ctx->height);
#else
        if(playerctrl)
            playerctrl->onInputResolutionChanged(demuxer->video_dec_ctx->width,
                                                 demuxer->video_dec_ctx->height);
#endif
    	break;
    }
    default:
    {
    	LOGE("Unknown event launched\n");
    	break;
    }
    }

end_thrError:

#ifdef _ANDROID_
	/* Detach current thread */
	if(env!= NULL)
	{
		detach_current_thread();
	}
#endif
	LOGD("Exiting 'thrEvent()'.\n");
	pthread_exit(NULL);
}
