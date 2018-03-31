//
//  MainActivity.cpp
//  nowuseeme
//
//  
//

#include "MainActivity.h"

#include <stdlib.h>
extern "C" {
    #include "log.h"
    #include "WebViewDelegate_ciface.h"
    #include "MainViewController_ciface.h"
}
#include "api.h"
#include "Preferences.h"
#include "ErrorCodes.h"
#include "RtmpClientCtrl.h"
#include "PlayerCtrl.h"
#include "main-jni.h"

//"onCreate"
MainActivity::MainActivity(void *uiview, void* webview_bridge):
    /* General private members */
    /* App. control related */
    m_uiview(uiview),
    m_webview_bridge(webview_bridge),
    m_api(NULL),
    m_preferences(new Preferences(this)),
    m_lastErrorCode(SUCCESS),
    m_RtmpClientCtrl(new RtmpClientCtrl(this)),
    m_PlayerCtrl(new PlayerCtrl(this)),
    m_intKey((352*288)+((352*288)/2)),
    rtmpURL("null"),
    isBackground(false),
    m_debugMethods(true), //true; // debugging purposes: put to "false" in RELEASE version
    m_disableCache(true),
    lastRequestedUrl("null")
{
    /* Initialize preferences */
    m_preferences->open();

    /* GLobally initialize FFmpeg libraries */
    initFFmpeg();

    /* Initialize rtmp-client */
    m_RtmpClientCtrl->open();
    
    /* Initialize player */
    m_PlayerCtrl->open();

    /* Instantiate API at the end (last class to instantiate because of dependencies) */
    m_api= new Api(this);
}

MainActivity::~MainActivity()
{
    /* Un-initialize rtmp-client */
    m_RtmpClientCtrl->close();
    
    /* Un-initialize player */
    m_PlayerCtrl->close();

    /* App. control related */
    delete m_api;
    m_api= NULL;

    delete m_RtmpClientCtrl;
    m_RtmpClientCtrl= NULL;

    delete m_PlayerCtrl;
    m_PlayerCtrl= NULL;
}

void MainActivity::onPause()
{
    LOGV("Activity 'onPause()'...");
    
    /* Pause rtmp-client */
    m_RtmpClientCtrl->pause();
    
    /* Pause player */
    m_PlayerCtrl->pause();
    
    /* Launch "on-background" event (=true) )to Javascript interface */
    //onBackgroundStateChange(true); // Performed outside this method
}

void MainActivity::onResume()
{
    LOGV("Activity 'onResume()'...");
    
    /* Resume rtmp-client */
    m_RtmpClientCtrl->resume();
    
    /* Resume player */
    m_PlayerCtrl->resume();
    
    /* Launch "on-background" event (=false) )to Javascript interface */
    //onBackgroundStateChange(false); // Performed outside this method
}

/*=== ERROR HANDLING ===*/

void MainActivity::setLastErrorCode(_retcodes errkey)
{
    this->m_lastErrorCode= errkey;
}

int MainActivity::getLatestErrorCode()
{
    return this->m_lastErrorCode;
}

string MainActivity::getLatestErrorText()
{
    return ErrorCodes::m_map.at(this->m_lastErrorCode);
}

/*==== EVENTS ====*/

void MainActivity::onAMFMessageSetString(string msg)
{
    m_onAMFMessageString= msg;
}

string MainActivity::onAMFMessageGetString()
{
    return m_onAMFMessageString;
}

void MainActivity::onAMFMessage(string msg)
{
    LOGD("'onAMFMessage()' recieved: %s\n", msg.c_str());
}

// application sent to background/foreground
void MainActivity::onBackgroundStateChange(bool _isBackground)
{
    if (isBackground != _isBackground) {
        isBackground = _isBackground;
        LOGD("'onBackgroundStateChange()' recieved: %s\n", isBackground? "true": "false");
        // Note that arg array is locally allcated and *MUST* be copied inside called method
        const char *arg0= isBackground? "true": "false";
        const char *args[]= {arg0, "isBackground", NULL};
        callJSFunction(m_webview_bridge, "onBackgroundStateChange", args);
    }
}

//client is connected/disconnected
void MainActivity::onConnectStatusChanged(bool connected)
{
    LOGD("'onConnectStatusChanged()' recieved: %s\n", connected? "true": "false");
    const char *arg0= connected? "true": "false";
    const char *args[]= {arg0, "connected", NULL};
    callJSFunction(m_webview_bridge, "onConnectStatusChanged", args);
}

//player input resolution changed
void MainActivity::onPlayerInputResolutionChanged(int width, int heigth)
{
    LOGD("onPlayerInputResolutionChanged(%d,%d)\n", width, heigth);
    const char *arg0= to_string(width).c_str();
    const char *arg2= to_string(heigth).c_str();
    const char *args[]= {arg0, "width", arg2, "heigth", NULL};

    /* We *MUST* execute in the main (UI) thread */
    performOnPlayerInputResolutionChangedOnMainThread(getMainUIView(), args);
}

void MainActivity::toggleFullscreen(bool hideBar)
{
    ::toggleFullscreen(getMainUIView(), hideBar? 1: 0);
}

void MainActivity::toggleActionBar(bool hideBar)
{
    ::toggleActionBar(getMainUIView(), hideBar? 1: 0);
}

/*=== Api bridge ===*/

string MainActivity::callApi(const char* name, char** args, int nargs)
{
    LOGV("function name: '%s'\n", name);
    LOGV("function arguments: ");
    for(int i= 0; i< nargs; i++) LOG_("%s, ", args[i]);
    LOG_("NULL \n");

    /*=== ERROR HANDLING ===*/
    if(strcmp(name, "getLatestErrorCode")== 0)
    {
        return to_string(m_api->getLatestErrorCode());
    }
    else if(strcmp(name, "getLatestErrorText")== 0)
    {
        return m_api->getLatestErrorText();
    }
    /*==== MUX SETTINGS ====*/
    else if(strcmp(name, "connect")== 0)
    {
        m_api->connect(args[0]);
    }
    else if(strcmp(name, "disconnect")== 0)
    {
        m_api->disconnect();
    }
    else if(strcmp(name, "autoReconnectRTMPCtrlSet")== 0)
    {
        m_api->autoReconnectRTMPCtrlSet(atoi(args[0])); //int autoReconnectTimeOut
    }
    else if(strcmp(name, "autoReconnectRTMPCtrlGet")== 0)
    {
        return to_string(m_api->autoReconnectRTMPCtrlGet());
    }
    else if(strcmp(name, "videoServerURLGet")== 0)
    {
        return m_api->videoServerURLGet();
    }
    /*==== VIDEO SETTINGS ====*/
    else if(strcmp(name, "videoResolutionWidthGet")== 0)
    {
        return to_string(m_api->videoResolutionWidthGet());
    }
    else if(strcmp(name, "videoResolutionHeightGet")== 0)
    {
        return to_string(m_api->videoResolutionHeightGet());
    }
    else if(strcmp(name, "videoResolutionSet")== 0)
    {
        return m_api->videoResolutionSet(atoi(args[0]), atoi(args[1]))? "true": "false";
    }
    else if(strcmp(name, "videoResolutionTest")== 0)
    {
        return m_api->videoResolutionTest(atoi(args[0]), atoi(args[1]), atoi(args[2]));
    }
    else if(strcmp(name, "videoResolutionsGet")== 0)
    {
        return m_api->videoResolutionsGet();
    }
    else if(strcmp(name, "videoFPSRangesGet")== 0)
    {
        return m_api->videoFPSRangesGet();
    }
    else if(strcmp(name, "videoFPSGet")== 0)
    {
        return to_string(m_api->videoFPSGet());
    }
    else if(strcmp(name, "videoFPSSet")== 0)
    {
        return m_api->videoFPSSet(atoi(args[0]))? "true": "false";
    }
    else if(strcmp(name, "videoKeyFrameIntervalSet")== 0)
    {
        return m_api->videoKeyFrameIntervalSet(atoi(args[0]))? "true": "false";
    }
    else if(strcmp(name, "videoKeyFrameIntervalGet")== 0)
    {
        return to_string(m_api->videoKeyFrameIntervalGet());
    }
    else if(strcmp(name, "videoCodecNameGet")== 0)
    {
        return m_api->videoCodecNameGet();
    }
    else if(strcmp(name, "videoCodecsGet")== 0)
    {
        return m_api->videoCodecsGet();
    }
    else if(strcmp(name, "videoCodecSet")== 0)
    {
        return m_api->videoCodecSet(args[0])? "true": "false";
    }
    else if(strcmp(name, "videoBandwidthSet")== 0)
    {
        return m_api->videoBandwidthSet(atoi(args[0]))? "true": "false";
    }
    else if(strcmp(name, "videoBandwidthGet")== 0)
    {
        return to_string(m_api->videoBandwidthGet());
    }
    else if(strcmp(name, "videoQualitySet")== 0)
    {
        return m_api->videoQualitySet(args[0])? "true": "false";
    }
    else if(strcmp(name, "videoQualityGet")== 0)
    {
        return m_api->videoQualityGet();
    }
    else if(strcmp(name, "rateCtrlToleranceSet")== 0)
    {
        return m_api->rateCtrlToleranceSet(atoi(args[0]))? "true": "false";
    }
    else if(strcmp(name, "rateCtrlToleranceGet")== 0)
    {
        return to_string(m_api->rateCtrlToleranceGet());
    }
    else if(strcmp(name, "isVideoMuted")== 0)
    {
        return m_api->isVideoMuted()? "true": "false";
    }
    /*==== AUDIO SETTINGS ====*/
    else if(strcmp(name, "audioCodecNameGet")== 0)
    {
        return m_api->audioCodecNameGet();
    }
    else if(strcmp(name, "audioCodecsGet")== 0)
    {
        return m_api->audioCodecsGet();
    }
    else if(strcmp(name, "audioCodecSet")== 0)
    {
        return m_api->audioCodecSet(args[0])? "true": "false";
    }
    else if(strcmp(name, "setMuteSender")== 0)
    {
        bool on= (strncmp(args[0], "true", strlen("true"))== 0)? true: false;
        return m_api->setMuteSender(on)? "true": "false";
    }
    else if(strcmp(name, "getMuteSender")== 0)
    {
        return m_api->getMuteSender()? "true": "false";
    }
    /*==== H/W SETTINGS ====*/
    else if(strcmp(name, "videoCamerasGet")== 0)
    {
        return m_api->videoCamerasGet();
    }
    else if(strcmp(name, "videoCameraDetailsGet")== 0)
    {
        return m_api->videoCameraDetailsGet(args[0]);
    }
    else if(strcmp(name, "videoCameraGet")== 0)
    {
        return m_api->videoCameraGet();
    }
    else if(strcmp(name, "videoCameraSet")== 0)
    {
        return m_api->videoCameraSet(args[0])? "true": "false";
    }
    /*==== A/V Player ====*/
    else if(strcmp(name, "viewVideoDisplay")== 0)
    {
        bool display= (strncmp(args[0], "true", strlen("true"))== 0)? true: false;
        m_api->viewVideoDisplay(display, atoi(args[1]), atoi(args[2]), atoi(args[3]), atoi(args[4]));
    }
    else if(strcmp(name, "viewVideoGet")== 0)
    {
        return m_api->viewVideoGet();
    }
    else if(strcmp(name, "videoPublishSet")== 0)
    {
        bool start= (strncmp(args[1], "true", strlen("true"))== 0)? true: false;
        m_api->videoPublishSet(args[0], start);
    }
    else if(strcmp(name, "videoPublishGet")== 0)
    {
        return m_api->videoPublishGet()? "true": "false";
    }
    else if(strcmp(name, "setMutePlayer")== 0)
    {
        bool on= (strncmp(args[0], "true", strlen("true"))== 0)? true: false;
        return m_api->setMutePlayer(on)? "true": "false";
    }
    else if(strcmp(name, "getMutePlayer")== 0)
    {
        return m_api->getMutePlayer()? "true": "false";
    }
    else if(strcmp(name, "autoReconnectPlayerSet")== 0)
    {
        m_PlayerCtrl->autoReconnectSet(atoi(args[0]));
    }
    else if(strcmp(name, "autoReconnectPlayerGet")== 0)
    {
        return to_string(m_api->autoReconnectPlayerGet());
    }

    /*==== Preview window ====*/
    else if(strcmp(name, "localPreviewDisplay")== 0)
    {
        bool display= (strncmp(args[0], "true", strlen("true"))== 0)? true: false;
        m_api->localPreviewDisplay(display, atoi(args[1]), atoi(args[2]), atoi(args[3]), atoi(args[4]));
    }
    /*=== Other functionality ===*/
    else if(strcmp(name, "toggleFullscreen")== 0)
    {
        bool hideBar= (strncmp(args[0], "true", strlen("true"))== 0)? true: false;
        m_api->toggleFullscreen(hideBar);
    }
    else if(strcmp(name, "toggleActionBar")== 0)
    {
        bool hideBar= (strncmp(args[0], "true", strlen("true"))== 0)? true: false;
        m_api->toggleActionBar(hideBar);
    }
    else if(strcmp(name, "displayInfoGet")== 0)
    {
        return m_api->displayInfoGet();
    }
    else if(strcmp(name, "onError")== 0)
    {
        m_api->onError((string)args[0]);
    }
    return "null";
}
