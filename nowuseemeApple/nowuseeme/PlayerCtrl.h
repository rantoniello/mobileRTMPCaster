//
//  PlayerCtrl.h
//  nowuseeme
//
//  
//

#ifndef __nowuseeme__PlayerCtrl__
#define __nowuseeme__PlayerCtrl__

#include <stdio.h>
#include <string>
#include <mutex>

using namespace std;
class MainActivity;
class PlayerView;

class PlayerCtrl
{
private:
    MainActivity *m_parentActivity;
    mutex m_lock;
    PlayerView *m_player;
    bool m_isPlayingOnPause;
    string m_url;
    bool m_a_mute_on;
    int m_autoReconnectTimeOut= 5; // [seconds]

public:
    PlayerCtrl(MainActivity *context);
    ~PlayerCtrl();

    /**
     * Open 'PlayerCtrl' class.
     */
    void open();

    /**
     * Close 'PlayerCtrl' class.
     */
    void close();
    
    /**
     * Pause 'PlayerCtrl' class
     */
    void pause();

    /**
     * Resume 'PlayerCtrl' class
     */
    void resume();

    /**
     * Start 'PlayerCtrl' class
     */
    void start();

    /**
     * Stop 'PlayerCtrl' class
     */
    void stop();

    /****************************************************************
     ***************** API IMPLEMENTATION ***************************
     ****************************************************************/

    /*==== A/V Player ====*/

    // set video thumbnail URL (for 2 way video) or null to hide, and the position
    void viewVideoDisplay(bool display, int width, int height, int x, int y);

    string viewVideoGet();

    // get/set publish status
    void videoPublishSet(string url, bool start);

    bool videoPublishGet();

    // set/get mute on player
    bool setMutePlayer(bool on);

    bool getMutePlayer();

    void autoReconnectSet(int timeout);

    int autoReconnectGet();

    /**
     * JNI to C++ bridge on-error callback
     * @param errorCode Integer indicating error code
     */
    void onError(int errorCode);

    void onInputResolutionChanged(int width, int heigth);
};

#endif /* defined(__nowuseeme__PlayerCtrl__) */
