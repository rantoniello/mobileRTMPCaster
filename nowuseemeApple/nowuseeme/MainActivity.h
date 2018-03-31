//
//  MainActivity.h
//  nowuseeme
//
//  
//

#ifndef __nowuseeme__MainActivity__
#define __nowuseeme__MainActivity__

#include <stdio.h>
#include <string>

#include "ErrorCodes.h"

class Api;
class Preferences;
class RtmpClientCtrl;
class PlayerCtrl;

using namespace std;

class MainActivity
{
private:
    /* General private members */

    /* App. control related */
    void *m_uiview;
    void *m_webview_bridge;
    Api *m_api;
    Preferences *m_preferences;
    _retcodes m_lastErrorCode;
    RtmpClientCtrl *m_RtmpClientCtrl;
    PlayerCtrl *m_PlayerCtrl;
    int m_intKey;
    string rtmpURL;
    bool isBackground;
    bool m_disableCache;
    string lastRequestedUrl;
    string m_onAMFMessageString;

public:
    bool m_debugMethods; // debugging purposes: put to "false" in RELEASE version

public:
    MainActivity(void *uiview, void* webview_bridge);
    ~MainActivity();

    void onPause();
    void onResume();

    string callApi(const char* name, char** args, int nargs);

    Api* getApi() { return m_api; }
    RtmpClientCtrl* getRtmpClientCtrl() { return m_RtmpClientCtrl; }
    PlayerCtrl* getPlayerCtrl() { return m_PlayerCtrl; }
    void* getMainUIView() { return m_uiview; }

    /*=== ERROR HANDLING ===*/
    
    void setLastErrorCode(_retcodes errkey);
    int getLatestErrorCode();
    string getLatestErrorText();

    /*==== EVENTS ====*/

    void onAMFMessageSetString(string msg);
    string onAMFMessageGetString();
    void onAMFMessage(string msg);
    
    // application sent to background/foreground
    void onBackgroundStateChange(bool _isBackground);
    
    //client is connected/disconnected
    void onConnectStatusChanged(bool connected);
    
    //player input resolution changed
    void onPlayerInputResolutionChanged(int width, int heigth);

    /*======= Other functionality ======*/

    void toggleFullscreen(bool hideBar);
    void toggleActionBar(bool hideBar);
};

#endif /* defined(__nowuseeme__MainActivity__) */
