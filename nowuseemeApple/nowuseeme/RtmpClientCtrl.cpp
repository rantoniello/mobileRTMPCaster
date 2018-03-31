//
//  RtmpClientCtrl.cpp
//  nowuseeme
//
//  
//

#include "RtmpClientCtrl.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
extern "C" {
    #include "log.h"
    #include "MainViewController_ciface.h"
}
#include "ErrorCodes.h"
#include "MainActivity.h"
#include "DefaultLocalSettings.h"
#include "VideoRecorder.h"
#include "rtmpclient-jni.h"

/**
 * 'RtmpClientCtrl' class constructor
 */
RtmpClientCtrl::RtmpClientCtrl(MainActivity *context):
    /* General private attributes */
    m_context(context), // redundant
    m_parentActivity(context),
    m_this(this),
    //m_lock(),
    m_inBackground(false),
    m_defaultLocalSettings(NULL),

    /* Streaming multiplex related attributes */
    m_isConnected(false),

    /* Video coding related attributes */
    m_vrecorder(NULL),
    m_cameraId("null"), // Default value
    m_v_width(352),
    m_v_height(288),
    m_v_frame_rate(30),
    m_v_max_frame_rate(30),
    m_v_min_frame_rate(10),
    m_v_gop_size(30),
    m_v_bit_rate(256*1024),
    m_v_rc(-1), // "rate-control"; -1 or 0 means "no control"
    m_v_codec_id("h264"),
    m_v_codecs_list({"h264", "flv1"}),

    /* Audio coding related attributes */
    m_a_bit_rate(64000),
    m_a_sample_rate(22050), //[Hz]
    m_a_channels_num(1), // default to mono
    m_a_frame_size(1024),
    m_a_codec_id("aac"),
    m_a_codecs_list({"aac", "mp3"}),
    m_a_mute_on(false),

    /* Streaming multiplex related */
    m_streaming_url("null"),
    m_autoReconnectTimeOut(5), // [seconds]
    m_av_quality("q1"), // possible values: q1-q2-q3
    // Following object (supported range of qualities) can be extended/modified
    m_av_supported_q({"q1", "q2", "q3"}),

    /* Scanner related */
    m_onScannerActivityResult(false),

    // Following parameter is intended to be used for "background still-image"
    // initialization. This feature can be made "dynamic" in the future:
    // e.g. with a user wizard for choosing a personalized image. (//TODO)
    m_backgroundImageSize((352*288)+((352*288)/2))
{
}

/**
 * Open 'RtmpClientCtrl' class.
 */
void RtmpClientCtrl::open()
{
    m_lock.lock();
    
    /* Save Main-Activity environment and object handlers into globals
     * variables of JNI level
     */
    setRtmpClientCtrlObj(this);
    
    /* Initialize native rtmp-client context */
    rtmpclientOpen();
    
    /* Create A/V recorders
     */
    m_vrecorder= new VideoRecorder(m_context);

    /* Set an initial working settings (e.g. video camera, resolutions, etc.)
     * 'VideoRecorder' class must be already instantiated.
     */
    // Get user preferences //RAL: FIXME!!
//    SharedPreferences preferences= m_context->getSharedPreferences("nowuseeme", MainActivity.MODE_PRIVATE);
//    if(preferences!= NULL)
//    {
//        m_streaming_url= preferences.getString("defaultServerURL", NULL);
//        m_cameraId= preferences.getInt("defaultCameraID", -1);
//    }

    // Get default working settings for this specific device
    m_defaultLocalSettings= new DefaultLocalSettings(m_parentActivity, m_vrecorder);
    
    // Select preferred/default camera
    if((m_cameraId= m_defaultLocalSettings->cameraSelectDefault(m_cameraId))== "null")
    {
        LOGE("Error: No cameras available!\n");
        m_parentActivity->setLastErrorCode(OPEN_CAMERA_ERROR);
    }

    // Select initial working resolution
    this->selectValidResolution();
    
    // Select initial valid fps value
    this->selectValidFPS();

    // Select initial working audio default values
    double sampleRate= 0, *psampleRate= &sampleRate;
    unsigned long formatId= 0, *pformatId= &formatId,
    bytesPerFrame= 0, *pbytesPerFrame= &bytesPerFrame,
    channelsNum= 0, *pchannelsNum= &channelsNum,
    audioFrameSize= 0, *paudioFrameSize= &audioFrameSize,
    audioBitsPerChannel= 0, *paudioBitsPerChannel= &audioBitsPerChannel;
    m_defaultLocalSettings->audioSelectDefaults("null", psampleRate, pformatId,
                                                pbytesPerFrame, pchannelsNum,
                                                paudioFrameSize, paudioBitsPerChannel);
    if(formatId!= 'lpcm' || audioBitsPerChannel!= 16)
    {
        LOGE("Microphone device not supported\n");
        // TODO: we force unsuppoted conf.
        m_a_sample_rate= 0;
        m_a_channels_num= 0;
    }
    else{
        m_a_sample_rate= (int)sampleRate;
        m_a_channels_num= (int)channelsNum;
        m_a_frame_size= (int)audioFrameSize/2; // in short units!
        LOGD("Selected: audio sample-rate: %d, num. of channels: %d, frame-size: %d\n",
             m_a_sample_rate, m_a_channels_num, m_a_frame_size);
    }
    
    /* Immediately publish default connection status in initialization */
    m_parentActivity->onConnectStatusChanged(m_isConnected); //FIXME!!

    m_lock.unlock();
}

/**
 * Close 'RtmpClientCtrl' class.
 */
void RtmpClientCtrl::close()
{
    m_lock.lock();
    
    /* De-initialize native rtmp-client context */
    rtmpclientClose();
    
    /* Release global references at JNI level */
    releaseRtmpClientCtrlObj();

    /* Release A/V recorders */
    delete m_vrecorder;
    m_vrecorder= NULL;

    m_lock.unlock();
}

/**
 * Pause 'RtmpClientCtrl' class
 */
void RtmpClientCtrl::pause()
{
    m_lock2.lock();

    /* Run rtmp-client in background mode */
    if(!m_inBackground && m_isConnected)
    {
        LOGD("Rtmp-client running in background mode\n");
        int retcode= runBackground();
        if(retcode<= 0)
        {
            // Set last error code in parent/main activity
            m_parentActivity->setLastErrorCode((_retcodes)retcode);
        }
        m_inBackground= true;
    }

    m_lock2.unlock();
}

/**
 * Resume 'RtmpClientCtrl' class
 */
void RtmpClientCtrl::resume()
{
    m_lock2.lock();

    /* Restore rtmp-client from background mode */
    if(m_inBackground)
    {
        int retcode= stopBackground();
        if(retcode<= 0)
        {
            // Set last error code in parent/main activity
            m_parentActivity->setLastErrorCode((_retcodes)retcode);
        }
        m_inBackground= false;
        
        // If running, we need to restart encoders
        if(m_vrecorder->isRecording())
        {
            //this->stop();
            //this->start();
        }
    }
#if 0 //FIXME!!
    /* Resuming from scanner activity */
    if(m_onScannerActivityResult)
    {
        Thread thread= new Thread()
        {
            @Override
            public void run()
            {
                /* Hack (have to code this better):
                 * Make sure surface is created before starting managing camera
                 * */
                try
                {
                    int count= 0, max_count= 10;
                    while(m_vrecorder!= NULL && !m_vrecorder.isSurfaceCreated()
                          && count< max_count)
                    {
                        LOGV("testing sleep interrumption'...\n");
                        Thread.sleep(500); // sleep 500 milliseconds ...
                        count++;
                    }
                    if(count>= max_count)
                    {
                        LOGE("Could not create video recorder surface view\n");
                        return; // exit run()
                    }
                }
                catch(InterruptedException e)
                {
                    LOGE("Could not start video recorder\n");
                    return; // exit run()
                }
                
                /* Start, on resume, rtmp-client */
                m_onScannerActivityResult= false;
                m_this.start();
            } // run()
        };
        thread.start();
    }
#endif
    m_lock2.unlock();
}

/**
 * Start 'RtmpClientCtrl' class.
 * A 'Json' formated String is passed to native RTMP-Client initialization
 * method specifying initialization parameters for A/V encoding and muxing.
 * A/V capturing threads are started.
 */
void RtmpClientCtrl::start()
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&m_rtmpclient_thread, &attr, start_thr, this);

    m_parentActivity->onConnectStatusChanged(true);
}

void* RtmpClientCtrl::start_thr(void *t) // auxiliary method (do not use locks)
{
    RtmpClientCtrl *client= (RtmpClientCtrl*)t;

    client->m_lock.lock();

    /* Initialize native RTMP client before start recording A/V */
    string cmd=
    "["
    "{\"tag\":\"streaming_url\",\"type\":\"String\",\"val\":\""+client->m_streaming_url+"\"}"
    "{\"tag\":\"v_bit_rate\",\"type\":\"int\",\"val\":\""+to_string(client->m_v_bit_rate)+"\"}"
    "{\"tag\":\"v_rc\",\"type\":\"int\",\"val\":\""+to_string(client->m_v_rc)+"\"}"
    "{\"tag\":\"v_frame_rate\",\"type\":\"int\",\"val\":\""+to_string(client->m_v_frame_rate)+"\"}"
    "{\"tag\":\"v_width\",\"type\":\"int\",\"val\":\""+to_string(client->m_v_width)+"\"}"
    "{\"tag\":\"v_height\",\"type\":\"int\",\"val\":\""+to_string(client->m_v_height)+"\"}"
    "{\"tag\":\"v_gop_size\",\"type\":\"int\",\"val\":\""+to_string(client->m_v_gop_size)+"\"}"
    "{\"tag\":\"v_codec_id\",\"type\":\"String\",\"val\":\""+client->m_v_codec_id+"\"}"
    "{\"tag\":\"a_bit_rate\",\"type\":\"int\",\"val\":\""+to_string(client->m_a_bit_rate)+"\"}"
    "{\"tag\":\"a_sample_rate\",\"type\":\"int\",\"val\":\""+to_string(client->m_a_sample_rate)+"\"}"
    "{\"tag\":\"a_channel_num\",\"type\":\"int\",\"val\":\""+to_string(client->m_a_channels_num)+"\"}"
    "{\"tag\":\"a_frame_size\",\"type\":\"int\",\"val\":\""+to_string(client->m_a_frame_size)+"\"}"
    "{\"tag\":\"a_codec_id\",\"type\":\"String\",\"val\":\""+client->m_a_codec_id+"\"}"
    "{\"tag\":\"av_quality\",\"type\":\"String\",\"val\":\""+client->m_av_quality+"\"}"
    "]";
    int retcode= rtmpclientInit(cmd.c_str(), 1/*client->m_vrecorder.isAvailable()*/, 1);
    if(retcode<= 0)
    {
        client->m_parentActivity->setLastErrorCode((_retcodes)retcode);
    }
    if(client->m_parentActivity->getLatestErrorCode()!= SUCCESS)
    {
        LOGE("'rtmpclientInit()' failed: \n");
        client->m_lock.unlock();
        return (void*)ERROR;
    }
    
    /* Set background still-image */
    // (will be shown when Main-Activity goes background)
    client->loadInBackgroundImage();
    if(client->m_parentActivity->getLatestErrorCode()!= SUCCESS)
    {
        LOGE("'loadInBackgroundImage()' failed\n");
        client->m_lock.unlock();
        return (void*)ERROR;
    }
    
    /* Start A/V recorders (capturing from camera/microphone) */
    if(client->m_vrecorder!= NULL)
        client->m_vrecorder->start(client->m_cameraId, client->m_v_width, client->m_v_height, client->m_v_frame_rate,
                                   client->m_a_mute_on);
    client->m_isConnected= true;
    client->m_lock.unlock();
    return (void*)SUCCESS;
}

/**
 * Stop 'RtmpClientCtrl' class.
 * A/V capturing threads are stopped/destroyed.
 * JNI RTMP-Client is released.
 */
void RtmpClientCtrl::stop()
{
    m_lock.lock();
    
    LOGV("Finishing recording, calling stop and releaseing\n");

    /* Stop A/V recorders */
    if(m_vrecorder!= NULL) m_vrecorder->stop();
    
    /* Release rtmp-client dynamically allocated resources */
    rtmpclientRelease();
    
    if(m_isConnected)
    {
        m_isConnected= false;
        m_parentActivity->onConnectStatusChanged(false);
    }

    LOGV("'stopRecording()' successfuly executed\n");
    
    m_lock.unlock();
}

/**
 * JNI to Java bridge on-error callback
 * @param errorCode Integer indicating error code
 */
void RtmpClientCtrl::onError(int errorCode)
{
    LOGV("'onError()'...\n");

    /* We *MUST* execute in the main (UI) thread */
    performOnErrorOnMainThread(m_context->getMainUIView(), errorCode);
}

//{ //Bogdan
bool RtmpClientCtrl::isRecording()
{
    return (m_vrecorder != NULL &&
            m_vrecorder->isRecording());
}
//} //Bogdan

/****************************************************************
 ***************** API IMPLEMENTATION ***************************
 ****************************************************************/

/*==== MUX SETTINGS ====*/

/**
 * Connect to RTMP server.
 * Note that for disconnection, stop() method applies.
 * @param url
 */
void RtmpClientCtrl::connect(string url)
{
    LOGW("Conecting to URL: '%s'\n", url.c_str() );
    
    /* Check if it is already connected and running */
    if(m_vrecorder->isRecording())
    {
        LOGW("... already connected to URL: %s\n", m_streaming_url.c_str());
        return;
    }
    
    /* Check if the demanded camera is available on this device */
    if(!m_vrecorder->isCameraAvailable(m_cameraId) )
    {
        LOGE("Error: Demanded camera not available\n");
        m_parentActivity->setLastErrorCode(OPEN_CAMERA_ERROR);
        return;
    }
    
    /* Check and set given URL */
    /*if(url== NULL) //FIXME!!
    {
        LOGE("Error: connection URL should not be NULL\n");
        m_parentActivity->setLastErrorCode(OPEN_CONNECTION_OR_OFILE_ERRROR);
        m_onScannerActivityResult= false; // reset bool
        return;
    }
    else*/ if(url.size()> 6) //e.g. "xxx://..."
    {
        m_streaming_url= url;
    }
    else if(m_parentActivity->m_debugMethods)
    {//RAL: debugging purposes (define 'm_debugMethods= false' for release version)
        m_streaming_url= "rtmp://10.0.0.103:1935/myapp/mystream";
    }
    else if(m_streaming_url.size()<= 6)
    {
        /* NOTE: When scanner app. is launched, this app. goes background.
         * For that reason, method 'start()' will be applied in 'resume()'
         * instead, and we must take special care for 'VideRecorder'
         * surface to be previously created. Refer to 'resume()' method
         * for the rest of the code.
         */
        m_parentActivity->setLastErrorCode(OPEN_CONNECTION_OR_OFILE_ERRROR);
        m_onScannerActivityResult= false; // reset bool
        //    		m_onScannerActivityResult= true;
        //    		barcodeScan();
    }
    
    // Set some user configuration preferences on start
    /*if(m_parentActivity->startModeGet()== MainActivity.LAUNCHER_MODE)
    {
        SharedPreferences preferences= m_context.getSharedPreferences("nowuseeme", Context.MODE_PRIVATE);
        SharedPreferences.Editor edit= preferences.edit();
        edit.putString("defaultServerURL", m_streaming_url);
        edit.putInt("defaultCameraID", m_cameraId);
        edit.commit();
    }*/ //FIXME!!
    
    if(m_onScannerActivityResult== false)
    {
        this->start();
    }
}

void RtmpClientCtrl::autoReconnectSet(int timeout)
{
    m_autoReconnectTimeOut= timeout;
    rtmpClientConnectionTimeOutSet(abs(timeout));
}

int RtmpClientCtrl::autoReconnectGet()
{
    return m_autoReconnectTimeOut;
}

string RtmpClientCtrl::videoServerURLGet()
{
    return m_streaming_url;
}

/*==== VIDEO SETTINGS ====*/

// get current camera width
int RtmpClientCtrl::videoResolutionWidthGet()
{
    return this->m_v_width;
}

//get current camera height
int RtmpClientCtrl::videoResolutionHeightGet()
{
    return this->m_v_height;
}

//set current resolution
bool RtmpClientCtrl::videoResolutionSet(int width, int height)
{
    LOGD("'videoResolutionSet()'; width: %d height: %d\n", width, height);
    
    // change resolution
    m_v_width= width;
    m_v_height= height;

    this->selectValidResolution();

    // If running, we need to restart encoder to change resolution
    if(m_vrecorder->isRecording())
    {
        this->stop();
        this->start();
    }

    // Check resolution changed
    if(videoResolutionWidthGet()== width &&
       videoResolutionHeightGet()== height)
    {
        return true; // set resolution OK
    }
    return false; // set resolution KO
}

// test capture from camera on this resolution and return closest resolution available
string RtmpClientCtrl::videoResolutionTest(int *&width, int *&height, int keepRatio)
{
    string resolution;
    int area= *width**height;
    float ratio= (float)*width/(float)*height;
    int idx_closest= 0; // index of 'closest' element in List
    int area_closest= 0;

    vector<pair<int, int>> *camera_sizes= m_vrecorder->getSupportedVideoSizes(m_cameraId);
    if(camera_sizes== NULL)
    {
        LOGW("Non resolution found for specified criterion\n");
        resolution= "{}";
        goto end_videoResolutionTest;
    }
    
    // Iterate vector to find the closest resolution available;
    // we use area as verisimilitude criterion
    for(int i= 0; i< camera_sizes->size(); i++)
    {
        int w= ((*camera_sizes)[i]).first, h= ((*camera_sizes)[i]).second;
        LOGD("Video resolution list [%d, %d]: \n", w, h);
        
        // If resolution matches, is done!
        if((w== *width) && (h==*height))
        {
            resolution= "{"+ to_string(w)+ "x"+ to_string(h)+ "}";
            *width= w; *height= h; // set values to return
            goto end_videoResolutionTest;
        }

        // If *MUST* keep ratio, we skip non-matching ratios
        float ratio_curr= (float)w/(float)h;
        if((keepRatio!= 0)&&(ratio_curr!=ratio))
        {
            LOGD("Current resolution does not respect ratio: %f\n", ratio_curr);
            continue;
        }
        
        // Measure verisimilitude for current resolution
        int area_curr= w*h;
        if(
           (i== 0) ||
           (abs(area_curr-area)<= abs(area_closest-area))
           )
        {
            area_closest= area_curr;
            idx_closest= i;
        }
    }

    // Closest resolution found
    if(area_closest!= 0)
    {
        int w= ((*camera_sizes)[idx_closest]).first;
        int h= ((*camera_sizes)[idx_closest]).second;
        resolution= "{"+ to_string(w)+ "x"+ to_string(h)+ "}";
        *width= w; *height= h; // set values to return
        goto end_videoResolutionTest;
    }

    LOGW("Non resolution found for specified criterion\n");
    resolution= "{}";
    
end_videoResolutionTest:

    if(camera_sizes!= NULL)
    {
        camera_sizes->clear();
        delete camera_sizes;
        camera_sizes= NULL;
        
    }
    return resolution;
}

// get list of supported resolutions
string RtmpClientCtrl::videoResolutionsGet()
{
    string s;
    vector<pair<int, int>> *camera_sizes= m_vrecorder->getSupportedVideoSizes(m_cameraId);
    if(camera_sizes== NULL)
    {
        s= "{}";
        goto end_videoResolutionsGet;
    }
    s= "[{";
    for(int i= 0; i< camera_sizes->size(); i++)
    {
        int w= ((*camera_sizes)[i]).first, h= ((*camera_sizes)[i]).second;
        if(i!= 0) s+= "},{";
        s+= "\"w\": \""+ to_string(w)+ "\",\"h\": \""+ to_string(h)+ "\"";
    }
    s+= "}]";
    LOGD("'videoResolutionsGet()': %s\n", s.c_str());

end_videoResolutionsGet:
    
    if(camera_sizes!= NULL)
    {
        camera_sizes->clear();
        delete camera_sizes;
        camera_sizes= NULL;
        
    }
    return s;
}

// get list of supported fps ranges
string RtmpClientCtrl::videoFPSRangesGet()
{
    string s;
    vector<pair<int, int>> *range_list= m_vrecorder->getSupportedPreviewFpsRange(m_cameraId);
    if(range_list== NULL)
    {
        s= "[]";
        goto end_videoFPSRangesGet;
    }
    s= "[{";
    for(int i= 0; i< range_list->size(); i++)
    {
        if(i!= 0) s+= "},{";
        int fps_max= ((*range_list)[i]).first,
        fps_min= ((*range_list)[i]).second;
        s+= "\"min\": \""+ to_string(fps_min)+ "\",\"max\": \""+ to_string(fps_max)+ "\"";
    }
    s+= "}]";
    LOGD("'videoFPSRangesGet()': %s\n", s.c_str());

end_videoFPSRangesGet:

    if(range_list!= NULL)
    {
        range_list->clear();
        delete range_list;
        range_list= NULL;
        
    }
    return s;
}

// get FPS
int RtmpClientCtrl::videoFPSGet()
{
    return this->m_v_frame_rate;
}

// set flash.media.Camera.setMode.fps
bool RtmpClientCtrl::videoFPSSet(int fps)
{
    m_v_frame_rate= fps;
    
    // set the nearest working fps
    this->selectValidFPS();
    
    // If running, we need to restart encoder to change resolution
    if(m_vrecorder->isRecording())
    {
        this->stop();
        this->start();
    }
    
    // Check resolution changed
    if(videoFPSGet()== fps)
    {
        return true; // set fps OK
    }
    return false; // set fps KO
}

// set flash.media.Camera.setKeyFrameInterval
bool RtmpClientCtrl::videoKeyFrameIntervalSet(int keyFrameInterval)
{
    m_v_gop_size= keyFrameInterval;
    
    // If running, we need to restart encoder to change resolution
    if(m_vrecorder->isRecording())
    {
        this->stop();
        this->start();
    }
    return true;
}

// get flash.media.Camera.setKeyFrameInterval
int RtmpClientCtrl::videoKeyFrameIntervalGet()
{
    return m_v_gop_size;
}

//get codec name
string RtmpClientCtrl::videoCodecNameGet()
{
    return this->m_v_codec_id;
}

// get list of compatible codecs
string RtmpClientCtrl::videoCodecsGet()
{
    string s= "[\"";
    for(int i= 0; i< m_v_codecs_list.size(); i++)
    {
        if(i!= 0) s+= "\",\"";
        s+= m_v_codecs_list[i];
    }
    s+= "\"]";
    LOGD("'videoCodecsGet()': %s\n", s.c_str() );
    return s;
}

// set codec
bool RtmpClientCtrl::videoCodecSet(string codec)
{
    // Check if given codec is supported
    for(int i= 0; i< m_v_codecs_list.size(); i++)
    {
        if(m_v_codecs_list[i].compare(codec)== 0)
        {
            this->m_v_codec_id= m_v_codecs_list[i];
            
            // If running, we need to restart encoder to change resolution
            if(m_vrecorder->isRecording())
            {
                this->stop();
                this->start();
            }
            return true;
        }
    }
    return false;
}

// get/set flash.media.Camera.bandwidth
bool RtmpClientCtrl::videoBandwidthSet(int bw)
{
    m_v_bit_rate= bw- m_a_bit_rate; // m_a_bit_rate is not configurable
    
    // If running, we need to restart encoder to change BW
    if(m_vrecorder->isRecording())
    {
        this->stop();
        this->start();
    }
    
    // Check BW changed
    if(videoBandwidthGet()== bw)
    {
        return true; // OK
    }
    return false; // KO
}

// get/set flash.media.Camera.bandwidth
int RtmpClientCtrl::videoBandwidthGet()
{
    return (this->m_v_bit_rate+ this->m_a_bit_rate);
}

// get/set flash.media.Camera.quality
bool RtmpClientCtrl::videoQualitySet(string quality)
{
    bool ret= false; // K.O. by default
    
    for(int i= 0; i< m_av_supported_q.size(); i++)
    {
        if(m_av_supported_q[i].compare(quality)== 0)
        {
            // quality specified is supported
            m_av_quality= quality;
            
            // If running, we need to restart encoder to change quality
            if(m_vrecorder->isRecording())
            {
                this->stop();
                this->start();
            }
            
            // Check quality changed
            if(videoQualityGet()== quality)
            {
                ret= true;; // OK
            }
            break; // break 'for' loop
        }
    }
    return ret;
}

// get/set flash.media.Camera.quality
string RtmpClientCtrl::videoQualityGet()
{
    return this->m_av_quality;
}

// get/set rate control tolerance (in % units; -1 o 0 means no control)
bool RtmpClientCtrl::rateCtrlToleranceSet(int rc)
{
    m_v_rc= rc;
    
    // If running, we need to restart encoder to change RC
    if(m_vrecorder->isRecording())
    {
        this->stop();
        this->start();
    }
    
    // Check RC changed
    if(rateCtrlToleranceGet()== rc)
    {
        return true; // OK
    }
    return false; // KO
}

int RtmpClientCtrl::rateCtrlToleranceGet()
{
    return this->m_v_rc;
}

// is it capturing from the camera?
bool RtmpClientCtrl::isVideoMuted()
{
    return this->m_vrecorder->isRecording();
}

/*==== AUDIO SETTINGS ====*/

//get codec name
string RtmpClientCtrl::audioCodecNameGet()
{
    return this->m_a_codec_id;
}

// get list of compatible codecs
string RtmpClientCtrl::audioCodecsGet()
{
    string s= "[\"";
    for(int i= 0; i< m_a_codecs_list.size(); i++)
    {
        if(i!= 0) s+= "\",\"";
        s+= m_a_codecs_list[i];
    }
    s+= "\"]";
    LOGD("'audioCodecsGet()': %s\n", s.c_str() );
    return s;
}

// set codec
bool RtmpClientCtrl::audioCodecSet(string codec)
{
    // Check if given codec is supported
    for(int i= 0; i< m_a_codecs_list.size(); i++)
    {
        if(m_a_codecs_list[i].compare(codec)== 0)
        {
            this->m_a_codec_id= m_a_codecs_list[i];
            
            // If running, we need to restart encoder to change resolution
            if(m_vrecorder->isRecording())
            {
                this->stop();
                this->start();
            }
            return true;
        }
    }
    return false;
}

// set/get mute on sender
bool RtmpClientCtrl::setMuteSender(bool on)
{
    this->m_a_mute_on= on;
    m_vrecorder->setMute(on);
    if(m_vrecorder->getMute()== on)
    {
        return true; // OK
    }
    return false; // KO
}

bool RtmpClientCtrl::getMuteSender()
{
    return this->m_a_mute_on;
}

/*==== H/W SETTINGS ====*/

// get list of connected cameras
string RtmpClientCtrl::videoCamerasGet()
{
    char** info_array= m_vrecorder->videoCamerasGet();
    if(info_array== NULL)
    {
        return "[]";
    }

    // Iterate over available cameras info and get further details
    string s= "[";
    for(int i= 0; info_array[i]!= NULL; i++)
    {
        if(i!= 0) s+= ",";
        s+= info_array[i];
    }
    s+= "]";
    LOGD("'videoCameraDetailsGet()': %s\n", s.c_str());

    // Release array
    for(int i= 0; info_array[i]!= NULL; i++)
    {
        free(info_array[i]);
    }
    free(info_array);
    return s;
}

// get info regarding camera
string RtmpClientCtrl::videoCameraDetailsGet(string cameraId)
{
    return m_vrecorder->videoCameraGet(cameraId);
}

// get current camera ID
string RtmpClientCtrl::videoCameraGet()
{
    return this->m_cameraId;
}

// set current camera
bool RtmpClientCtrl::videoCameraSet(string cameraId)
{
    // Check camera Id existence
    if(m_vrecorder->videoCameraGet(cameraId)== "{}")
    {
        m_parentActivity->setLastErrorCode(OPEN_CAMERA_ERROR);
        return false;
    }
    if(m_cameraId== cameraId)
    {
        return true; // set fps OK
    }
    
    m_cameraId= cameraId;
    
    // Set an initial working resolution ('m_vrecorder' must be already initialized)
    this->selectValidResolution();
    
    // set an initial working fps
    this->selectValidFPS();
    
    // If running, we need to restart encoder to change camera
    if(m_vrecorder->isRecording())
    {
        this->stop();
        this->start();
    }
    
    // Check camera changed
    if(videoCameraGet()== cameraId)
    {
        return true; // set camera OK
    }
    m_parentActivity->setLastErrorCode(OPEN_CAMERA_ERROR);
    return false; // set camera KO
}

/*==== Scan Bar ====*/
bool RtmpClientCtrl::barcodeScan()
{
    LOGD("Executing 'barcodeScan()'...\n");
    /*
    IntentIntegrator integrator= new IntentIntegrator(m_parentActivity);
    integrator.initiateScan();
    integrator= NULL;*/ //FIXME!!
    LOGD("'barcodeScan()' successfuly executed.\n");
    return true;
}

/*==== Preview window ====*/
void RtmpClientCtrl::localPreviewDisplay(bool display, int width, int height, int x, int y)
{
    m_vrecorder->localPreviewDisplay(display, width, height, x, y);
}

/*
 * PRIVATE METHODS
 */

/**
 * Set background image to be used in background thread.
 * Image provided should be in planar YUV 4:2:0 format.
 */
void RtmpClientCtrl::loadInBackgroundImage()
{
    int size_read= 0;
    FILE *yuv_file= NULL;
    char *buffer_background= NULL;
    int retcode= ERROR;

    /* Open YUV source file */
    const char *file_path= getBackgroundImagePath();
    LOGV("Opening file: '%s'\n", file_path);
    yuv_file= fopen(file_path, "r");
    if(yuv_file== NULL)
    {
        LOGE("Error occurred while opening 'background image' file\n");
        goto end_loadInBackgroundImage;
    }
    LOGV("O.K.\n");
    
    /* Allocate memory for background still image */
    buffer_background= (char*)malloc(m_backgroundImageSize);
    if(buffer_background== NULL)
    {
        LOGI("Could not allocate memory buffer for background still image\n");
        goto end_loadInBackgroundImage;
    }
    
    /* Read file into memory buffer; close file */
    size_read= fread(buffer_background, 1, m_backgroundImageSize, yuv_file);
    if(size_read!= m_backgroundImageSize)
    {
        LOGI("Error: error while reading YUV file "
             "(YUV size= %d; read bytes= %d; errno= %d %s)",
             m_backgroundImageSize, size_read, errno, strerror(errno) );
        goto end_loadInBackgroundImage;
    }

    retcode= setBackgroundImage(buffer_background, m_backgroundImageSize);
    if(retcode<= 0)
    {
        // Set last error code in parent/main activity
        m_parentActivity->setLastErrorCode((_retcodes)retcode);
    }

end_loadInBackgroundImage:

    if(buffer_background!= NULL)
    {
        delete [] buffer_background;
        buffer_background= NULL;
    }
    if(yuv_file!= NULL)
    {
        fclose(yuv_file);
        yuv_file= NULL;
    }
    return;
}

void RtmpClientCtrl::selectValidResolution()
{
    LOGV("Execution 'selectValidResolution()... '\n");
    int width= m_v_width, heigth= m_v_height, *pwidth= &width, *pheigth= &heigth;

    m_defaultLocalSettings->resolutionSelectDefault(pwidth, pheigth);
    if(*pwidth== -1 || *pheigth== -1)
    {
        LOGE("Error: Could not get default camera; no cameras available\n");
        m_parentActivity->setLastErrorCode(OPEN_CAMERA_ERROR);
    }
    else
    {
        m_v_width= *pwidth;
        m_v_height= *pheigth;
    }
}

void RtmpClientCtrl::selectValidFPS()
{
    LOGV("Execution 'selectValidFPS()... '\n");
    int *pmaxFrameRate= &m_v_max_frame_rate, *pminFrameRate= &m_v_min_frame_rate;

    m_defaultLocalSettings->fpsSelectDefault(m_cameraId, m_v_frame_rate, pmaxFrameRate, pminFrameRate);

    // update global fps value
    if(!((m_v_frame_rate<= m_v_max_frame_rate) && (m_v_frame_rate>= m_v_min_frame_rate)))
    {
        m_v_frame_rate= m_v_max_frame_rate;
    }
}
