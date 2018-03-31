//
//  api.h
//  nowuseeme
//
//  
//

#ifndef __nowuseeme__api__
#define __nowuseeme__api__

#include <stdio.h>
#include <string>

using namespace std;

class MainActivity;
class RtmpClientCtrl;
class PlayerCtrl;

class Api
{
private:
    MainActivity *m_parentActivity;
    RtmpClientCtrl *m_RtmpClientCtrl;
    PlayerCtrl *m_PlayerCtrl;
    
public:
    Api(MainActivity *activity);
    ~Api();

    /*=== ERROR HANDLING ===*/
    int getLatestErrorCode();
    string getLatestErrorText();

    /*==== MUX SETTINGS ====*/
    
    // connect to rtmp server
    void connect(string url);
    void disconnect();
    void autoReconnectRTMPCtrlSet(int autoReconnectTimeOut);
    int autoReconnectRTMPCtrlGet();
    string videoServerURLGet();
    
    /*==== VIDEO SETTINGS ====*/
    
    // get current camera width
    int videoResolutionWidthGet();
    //get current camera height
    int videoResolutionHeightGet();
    //set current resolution
    bool videoResolutionSet(int width, int height);
    // test capture from camera on this resolution and return closest resolution available
    string videoResolutionTest(int width, int height, int keepRatio);
    // get list of supported resolutions
    string videoResolutionsGet();

    //get list of supported FPS ranges
    string videoFPSRangesGet();
    // get FPS
    int videoFPSGet();
    // set flash.media.Camera.setMode.fps
    bool videoFPSSet(int fps);
    // set flash.media.Camera.setKeyFrameInterval
    bool videoKeyFrameIntervalSet(int keyFrameInterval);
    // get flash.media.Camera.setKeyFrameInterval
    int videoKeyFrameIntervalGet();
    // get codec name
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

    /*==== A/V Player ====*/

    // set video thumbnail (for 2 way video) to hide, and the position
    void viewVideoDisplay(bool display, int width, int height, int x, int y);
    string viewVideoGet();
    // get/set publish status
    void videoPublishSet(string url, bool start);
    bool videoPublishGet();
    // set/get mute on player
    bool setMutePlayer(bool on);
    bool getMutePlayer();
    void autoReconnectPlayerSet(int autoReconnectTimeOut);
    int autoReconnectPlayerGet();

    /*==== Preview window ====*/
    void localPreviewDisplay(bool display, int width, int height, int x, int y);

    /*==== Scan Bar ====*/
//    bool barcodeScan();
    
    //{ // Bogdan
    /*========= HTML init =========*/
//    void javascriptInit(bool available, string jsPrefix, bool useQueue);
    
    /*======= Other functionality ======*/
    // app control helper functions
//    void exit();
    
    // system bridge helper functions
    void toggleFullscreen(bool value);
    void toggleActionBar(bool value);
    string displayInfoGet();
//    void showToast(string value); // Not used?
//    void notificationAdd(string title, string text, string ticker, int intType); // Not used?
//    string jsQueueGet();
//    void browserReload();  // Not used?
//    void browserBackgroundColorSet(string color); // Not used?
    void onError(string val);  // Not used?
//    void sendEmail(string emailAddress, string subject, string body); // Not used?
//    void openWebBrowser(string sURL);

    //} //Bogdan
};

#endif /* defined(__nowuseeme__api__) */
