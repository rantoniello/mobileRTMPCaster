//
//  RtmpClientCtrl.h
//  nowuseeme
//
//  
//

#ifndef __nowuseeme__RtmpClientCtrl__
#define __nowuseeme__RtmpClientCtrl__

#include <stdio.h>
#include <string>
#include <vector>
#include <utility>
#include <mutex>

using namespace std;

class list;

class MainActivity;
class VideoRecorder;
class DefaultLocalSettings;

/**
 * class RtmpClientCtrl.
 */
class RtmpClientCtrl
{
private:
    /* General private attributes */
    /*Context*/MainActivity *m_context;
    MainActivity *m_parentActivity;
    RtmpClientCtrl *m_this;
    mutex m_lock;
    mutex m_lock2;
    bool m_inBackground;
    DefaultLocalSettings *m_defaultLocalSettings;
    pthread_t m_rtmpclient_thread;
    
    /* Streaming multiplex related attributes */
    bool m_isConnected;
    
    /* Video coding related attributes */
    VideoRecorder *m_vrecorder;
    string m_cameraId; // Default value
    int m_v_width;
    int m_v_height;
    int m_v_frame_rate;
    int m_v_max_frame_rate;
    int m_v_min_frame_rate;
    int m_v_gop_size;
    int m_v_bit_rate;
    int m_v_rc; // "rate-control"; -1 or 0 means "no control"
    string m_v_codec_id;
    vector<string> m_v_codecs_list;

    /* Audio coding related attributes */
    int m_a_bit_rate;
    int m_a_channels_num;
    int m_a_frame_size;
    int m_a_sample_rate; //[Hz]
    string m_a_codec_id;
    vector<string> m_a_codecs_list;
    bool m_a_mute_on;
    
    /* Streaming multiplex related */
    string m_streaming_url;
    int m_autoReconnectTimeOut; // [seconds]
    string m_av_quality; // possible values: q1-q2-q3
    // Following object (supported range of qualities) can be extended/modified
    vector<string> m_av_supported_q;

    // Following parameter is intended to be used for "background still-image"
    // initialization. This feature can be made "dynamic" in the future:
    // e.g. with a user wizard for choosing a personalized image. (//TODO)
    int m_backgroundImageSize;

public:

    /* Scanner related */
    bool m_onScannerActivityResult= false;
    
    /**
     * 'RtmpClientCtrl' class constructor
     */
    RtmpClientCtrl(MainActivity *context);
    
    /**
     * Open 'RtmpClientCtrl' class.
     */
    void open();
    
    /**
     * Close 'RtmpClientCtrl' class.
     */
    void close();
    
    /**
     * Pause 'RtmpClientCtrl' class
     */
    void pause();
    
    /**
     * Resume 'RtmpClientCtrl' class
     */
    void resume();
    
    /**
     * Start 'RtmpClientCtrl' class.
     * A 'Json' formated String is passed to native RTMP-Client initialization
     * method specifying initialization parameters for A/V encoding and muxing.
     * A/V capturing threads are started.
     */
    void start();
    
    /**
     * Stop 'RtmpClientCtrl' class.
     * A/V capturing threads are stopped/destroyed.
     * JNI RTMP-Client is released.
     */
    void stop();

    //{ //Bogdan
    bool isRecording();
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
    void connect(string url);

    void autoReconnectSet(int timeout);
    int autoReconnectGet();
    string videoServerURLGet();
    
    /*==== VIDEO SETTINGS ====*/
    
    // get current camera width
    int videoResolutionWidthGet(); 
    
    //get current camera height
    int videoResolutionHeightGet();
    
    //set current resolution
    bool videoResolutionSet(int width, int height);
    
    // test capture from camera on this resolution and return closest resolution available
    string videoResolutionTest(int *&width, int *&height, int keepRatio);

    // get list of supported resolutions
    string videoResolutionsGet();
    
    // get list of supported fps ranges
    string videoFPSRangesGet();
    
    // get FPS
    int videoFPSGet();
    
    // set flash.media.Camera.setMode.fps
    bool videoFPSSet(int fps);
    
    // set flash.media.Camera.setKeyFrameInterval
    bool videoKeyFrameIntervalSet(int keyFrameInterval);
    
    // get flash.media.Camera.setKeyFrameInterval
    int videoKeyFrameIntervalGet();
    
    //get codec name
    string videoCodecNameGet();
    
    // get list of compatible codecs
    string videoCodecsGet();
    
    // set codec
    bool videoCodecSet(string codec);
    
    // get/set flash.media.Camera.bandwidth
    bool videoBandwidthSet(int bw);
    
    // get/set flash.media.Camera.bandwidth
    int videoBandwidthGet();
    
    // get/set flash.media.Camera.quality
    bool videoQualitySet(string quality);
    
    // get/set flash.media.Camera.quality
    string videoQualityGet();
    
    // get/set rate control tolerance (in % units; -1 o 0 means no control)
    bool rateCtrlToleranceSet(int rc);
    
    int rateCtrlToleranceGet();
    
    // is it capturing from the camera?
    bool isVideoMuted();
    
    /*==== AUDIO SETTINGS ====*/
    
    //get codec name
    string audioCodecNameGet();
    
    // get list of compatible codecs
    string audioCodecsGet();
    
    // set codec
    bool audioCodecSet(string codec);
    
    // set/get mute on sender
    bool setMuteSender(bool on);
    
    bool getMuteSender();
    
    /*==== H/W SETTINGS ====*/
    
    // get list of connected cameras
    string videoCamerasGet();
    
    // get info regarding camera
    string videoCameraDetailsGet(string cameraId);
    
    // get current camera ID
    string videoCameraGet();
    
    // set current camera
    bool videoCameraSet(string cameraID);
    
    /*==== Scan Bar ====*/
    bool barcodeScan();
    
    /*==== Preview window ====*/
    void localPreviewDisplay(bool display, int width, int height, int x, int y);

    /*==== Other ====*/
    /**
     * JNI to Java bridge on-error callback
     * @param errorCode Integer indicating error code
     */
    void onError(int errorCode);

private:
    /*
     * PRIVATE METHODS
     */

    // auxiliary methods (do not use locks)
    static void* start_thr(void *t);

    /**
     * Set background image to be used in background thread.
     * Image provided should be in planar YUV 4:2:0 format.
     */
    void loadInBackgroundImage();
    void selectValidResolution();
    void selectValidFPS();
};

#endif /* defined(__nowuseeme__RtmpClientCtrl__) */
