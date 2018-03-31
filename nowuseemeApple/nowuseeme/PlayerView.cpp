//
//  PlayerView.cpp
//  nowuseeme
//
//  
//

#include "PlayerView.h"

#include "CameraThread.h"

extern "C" {
#include "log.h"
}
#include "MainActivity.h"
#include "ErrorCodes.h"
#include "DisplayCtrl.h"
#include "player-jni.h"

PlayerView::PlayerView(MainActivity *context):
    m_parentActivity((MainActivity*)context),
    m_playing(false),
    //m_isPlayerSurfaceCreated(false), // not used
    m_url("null"),
    m_urlDefault("rtmp://10.0.0.103:1935/myapp/mystream"), // default value;
    m_top(0), m_left(0), // defaults
    m_width(352), m_height(288), //defaults
    //m_lock // mutual-exclusion lock for public interface
    m_a_mute_on(false)
{
}

PlayerView::~PlayerView()
{
}

void PlayerView::setLayoutParams(bool display, int width, int height, int x, int y)
{
    m_lock.lock();
    
    LOGD("Executing 'setLayoutParams()'... \n");
    
    /* Set position and dimension in pixels */
    m_top= y; m_left= x;
    m_width= width; m_height= height;
    DisplayCtrl::changeDisplayParams(m_parentActivity, display, width, height, x, y, "player");
    LOGD("Exiting 'setLayoutParams()'... \n");
    m_lock.unlock();
}

void PlayerView::start(string url)
{
    m_lock.lock();
    start2(url);
    m_lock.unlock();
}
void PlayerView::start2(string url)
{
    LOGD("Starting video player...\n");
    
    if(m_playing)
    {
        LOGW("Could not start player thread, already running."
             "Please stop player before starting it\n");
        return;
    }
    LOGV("Player URL: '%s' (url size: %d)\n", url.c_str(), (int)url.size());
    if(url.compare("null")== 0 || url.size()< 6) // e.g. url== 'rtmp://...'
    {
        LOGE("Could not start player thread, non valid URL\n");
        return;
    }
    m_url= url;
    
    /* Start running thread */
    m_playing= true;
    
    /* Start native demuxer thread */
    playerStart(m_url.c_str(), m_width, m_height, m_a_mute_on? 1: 0, (void*)m_parentActivity->getMainUIView());
    
    LOGV("Video player started O.K.\n");
}

/**
 * Stop already running video player instance: waits player thread
 * to end (blocking) and release it.
 */
void PlayerView::stop()
{
    m_lock.lock();
    stop2();
    m_lock.unlock();
}
void PlayerView::stop2()
{
    LOGD("Stopping video player...\n");
    
    /* Release native player */
    playerStop();
    m_playing= false;
    
    LOGD("Video player stopped O.K.\n");
}

bool PlayerView::isPlaying()
{
    LOGD("isPlaying() returns: %d\n", m_playing? 1: 0);
    return m_playing;
}

//bool PlayerView::isSurfaceCreated() // Not used
//{
//    return m_isPlayerSurfaceCreated;
//}

void PlayerView::setMute(bool on)
{
    m_lock.lock();
    m_a_mute_on= on;
    playerMute(on? 1: 0);
    m_lock.unlock();
}

string PlayerView::viewVideoGet()
{
    string s= "{\"url\":\""+ m_url+ "\","+
    "\"top\":\""+ to_string(m_top)+ "\","+
    "\"left\":\""+ to_string(m_left)+ "\","+
    "\"width\":\""+ to_string(m_width)+ "\","+
    "\"height\":\""+ to_string(m_height)+ "\"}";
    return s;
}
string PlayerView::getDefaultPublishURL()
{
    return m_urlDefault;
}
