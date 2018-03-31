//
//  PlayerView.h
//  nowuseeme
//
//  
//

#ifndef __nowuseeme__PlayerView__
#define __nowuseeme__PlayerView__

#include <stdio.h>
#include <string>
#include <mutex>

using namespace std;
class MainActivity;

class PlayerView
{
private:
    MainActivity *m_parentActivity;
    volatile bool m_playing;
    //volatile bool m_isPlayerSurfaceCreated; // Not used
    string m_url, m_urlDefault; // default value;
    int m_top, m_left; // defaults
    int m_width, m_height; //defaults
    mutex m_lock; // mutual-exclusion lock for public interface
    bool m_a_mute_on;

public:
    PlayerView(MainActivity *context);
    ~PlayerView();

    //void surfaceChanged(SurfaceHolder holder, int format, int w, int h)
    //void surfaceCreated(SurfaceHolder holder)
    //void surfaceDestroyed(SurfaceHolder holder)
    
    /**
     * Set layout characteristics
     */
    void setLayoutParams(bool display, int width, int height, int x, int y);

    /**
     * Create new running thread to start playing A/V samples.
     */
    void start(string url);

    /**
     * Stop already running video player instance: waits player thread
     * to end (blocking) and release it.
     */
    void stop();

    /**
     * Check if PlayerView is playing A/V.
     * @return bool indicating if PlayerView is currently playing.
     */
    bool isPlaying();

    /**
     * Check if PlayerView has its surface created
     * @return bool indicating if surface is already created
     */
    //bool isSurfaceCreated(); // Not used

    /**
     * Mute player audio
     * @param on bool to mute/un-mute
     */
    void setMute(bool on);

    /*
     * API IMPLEMENTATION SUPPORT
     */
    string viewVideoGet();
    string getDefaultPublishURL();

private:
    /* Auxiliary members */
    void start2(string url);
    void stop2();
};

#endif /* defined(__nowuseeme__PlayerView__) */
