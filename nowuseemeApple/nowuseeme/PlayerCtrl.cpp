//
//  PlayerCtrl.cpp
//  nowuseeme
//
//  
//

#include "PlayerCtrl.h"

#include <stdlib.h>

extern "C" {
#include "log.h"
}
#include "MainActivity.h"
#include "ErrorCodes.h"
#include "DisplayCtrl.h"
#include "PlayerView.h"
#include "player-jni.h"

PlayerCtrl::PlayerCtrl(MainActivity *context):
    m_parentActivity((MainActivity*)context),
    //m_lock
    m_player(NULL),
    m_isPlayingOnPause(false),
    m_url("null"),
    m_a_mute_on(false),
    m_autoReconnectTimeOut(5) // [seconds]
{
}

PlayerCtrl::~PlayerCtrl()
{
}

void PlayerCtrl::open()
{
    m_lock.lock();
    
    /* Save Main-Activity environment and object handlers into globals
     * variables of JNI level
     */
    setPlayerCtrlObj(this);
    
    m_player= new PlayerView(m_parentActivity);
    
    m_lock.unlock();
}

void PlayerCtrl::close()
{
    m_lock.lock();
    
    m_player= NULL;
    
    /* Release global references at JNI level */
    releasePlayerCtrlObj();
    
    m_lock.unlock();
}

void PlayerCtrl::pause()
{
    m_lock.lock();
    
    if(m_player->isPlaying() )
    {
        m_isPlayingOnPause= true;
        m_player->stop();
    }
    
    m_lock.unlock();
}

/**
 * Resume 'PlayerCtrl' class
 */
void PlayerCtrl::resume()
{
    m_lock.lock();
    
    /* Resume player if it is the case */
    if(m_isPlayingOnPause== true)
    {
        // TODO: use thread here to call 'start()'?
        
        /* Start, on resume, the player */
        m_player->start(m_url);
        m_isPlayingOnPause= false;
        
    }
    m_lock.unlock();
}

/**
 * Start 'PlayerCtrl' class
 */
void PlayerCtrl::start()
{
    m_lock.lock();
    m_lock.unlock();
}

/**
 * Stop 'PlayerCtrl' class
 */
void PlayerCtrl::stop()
{
    m_lock.lock();
    m_player->stop();
    m_lock.unlock();
}

void PlayerCtrl::viewVideoDisplay(bool display, int width, int height, int x, int y)
{
    m_lock.lock();
    
    m_player->setLayoutParams(display, width, height, x, y);
    LOGV("'viewVideoDisplay(display: %d width: %d height: %d x: %d y: %d)'\n",
         display? 1: 0, width, height, x, y);
    
    m_lock.unlock();
}

string PlayerCtrl::viewVideoGet()
{
    string s;
    m_lock.lock();
    
    LOGD("'viewVideoGet()'\n");
    s= m_player->viewVideoGet();
    
    m_lock.unlock();
    return s;
}

// get/set publish status
void PlayerCtrl::videoPublishSet(string url, bool start)
{
    m_lock.lock();
    
    /* Use default if noURL/"null" is specified */
    LOGV("Player URL: '%s' (url size: %d)\n", url.c_str(), (int)url.size());
    if(m_url.compare("null")== 0 || url.size()< 6) // e.g. url== 'rtmp://...'
    {
        LOGV("Using default URL: '%s'\n", m_player->getDefaultPublishURL().c_str() );
        m_url= m_player->getDefaultPublishURL();
    }
    else
    {
        m_url= url;
    }
    
    if(start== true)
    {
        m_player->start(m_url);
    }
    else
    {
        m_player->stop();
    }
    
    m_lock.unlock();
}

bool PlayerCtrl::videoPublishGet()
{
    bool ret;
    m_lock.lock();
    
    ret= (m_player!= NULL && m_player->isPlaying() );
    LOGV("'videoPublishGet()' returns: %d\n", ret? 1: 0);
    
    m_lock.unlock();
    return ret;
}

// set/get mute on player
bool PlayerCtrl::setMutePlayer(bool on)
{
    LOGV("'setMutePlayer()' set to: %d\n", on? 1: 0);
    this->m_a_mute_on= on;
    m_player->setMute(on);
    if(this->getMutePlayer()== on)
    {
        return true; // OK
    }
    return false; // KO
}

bool PlayerCtrl::getMutePlayer()
{
    return this->m_a_mute_on;
}

void PlayerCtrl::autoReconnectSet(int timeout)
{
    m_autoReconnectTimeOut= timeout;
    playerConnectionTimeOutSet(abs(timeout));
}

int PlayerCtrl::autoReconnectGet()
{
    return m_autoReconnectTimeOut;
}

/**
 * JNI to C++ bridge on-error callback
 * @param errorCode Integer indicating error code
 */
void PlayerCtrl::onError(int errorCode)
{
    LOGV("'onError()'...\n");
    
    /* Set error code */
    m_parentActivity->setLastErrorCode((_retcodes)errorCode);
    
    if(errorCode!= SUCCESS)
    {
        /* Stop/disconnect RTMP server */
        this->stop();
        
        /* Auto-reconnect if apply */
        if(m_autoReconnectTimeOut>= 0)
        {
            m_player->start(m_url);
        }
    }
}

void PlayerCtrl::onInputResolutionChanged(int width, int heigth)
{
    LOGV("'onInputResolutionChanged(%d, %d)'...\n", width, heigth);
    m_parentActivity->onPlayerInputResolutionChanged(width, heigth);
}
