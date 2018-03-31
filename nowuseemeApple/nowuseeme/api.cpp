//
//  api.cpp
//  nowuseeme
//
//  
//

#include "api.h"

#include "MainActivity.h"
#include "RtmpClientCtrl.h"
#include "PlayerCtrl.h"
extern "C" {
    #include "MainActivity_ciface.h"
}

Api::Api(MainActivity *activity):
    m_parentActivity(activity),
    m_RtmpClientCtrl(NULL),
    m_PlayerCtrl(NULL)
{
    //m_parentActivity= activity;
    m_RtmpClientCtrl= m_parentActivity->getRtmpClientCtrl();
    m_PlayerCtrl= m_parentActivity->getPlayerCtrl();
}

Api::~Api()
{
}

/*=== ERROR HANDLING ===*/
int Api::getLatestErrorCode()
{
    return this->m_parentActivity->getLatestErrorCode();
}

string Api::getLatestErrorText()
{
    return this->m_parentActivity->getLatestErrorText();
}

/*==== MUX SETTINGS ====*/

// connect to rtmp server
void Api::connect(string url)
{
    m_RtmpClientCtrl->connect(url);
}

void Api::disconnect()
{
    m_RtmpClientCtrl->stop();
}

void Api::autoReconnectRTMPCtrlSet(int autoReconnectTimeOut)
{
    m_RtmpClientCtrl->autoReconnectSet(autoReconnectTimeOut);
}

int Api::autoReconnectRTMPCtrlGet()
{
    return m_RtmpClientCtrl->autoReconnectGet();
}

string Api::videoServerURLGet()
{
    return m_RtmpClientCtrl->videoServerURLGet();
}

/*==== VIDEO SETTINGS ====*/

// get current camera width
int Api::videoResolutionWidthGet()
{
    return m_RtmpClientCtrl->videoResolutionWidthGet();
}
//get current camera height
int Api::videoResolutionHeightGet()
{
    return m_RtmpClientCtrl->videoResolutionHeightGet();
}
//set current resolution
bool Api::videoResolutionSet(int width, int height)
{
    return m_RtmpClientCtrl->videoResolutionSet(width, height);
}
// test capture from camera on this resolution and return closest resolution available
string Api::videoResolutionTest(int width, int height, int keepRatio)
{
    int _width= width, _height= height;
    int *pwidth= &_width, *pheight= &_height;
    return m_RtmpClientCtrl->videoResolutionTest(pwidth, pheight, keepRatio);
}
// get list of supported resolutions
string Api::videoResolutionsGet()
{
    return m_RtmpClientCtrl->videoResolutionsGet();
}

//get list of supported FPS ranges
string Api::videoFPSRangesGet()
{
    return m_RtmpClientCtrl->videoFPSRangesGet();
}

// get FPS
int Api::videoFPSGet()
{
    return m_RtmpClientCtrl->videoFPSGet();
}
// set flash.media.Camera.setMode.fps
bool Api::videoFPSSet(int fps)
{
    return m_RtmpClientCtrl->videoFPSSet(fps);
}
// set flash.media.Camera.setKeyFrameInterval
bool Api::videoKeyFrameIntervalSet(int keyFrameInterval)
{
    return m_RtmpClientCtrl->videoKeyFrameIntervalSet(keyFrameInterval);
}
// get flash.media.Camera.setKeyFrameInterval
int Api::videoKeyFrameIntervalGet()
{
    return m_RtmpClientCtrl->videoKeyFrameIntervalGet();
}
// get codec name
string Api::videoCodecNameGet()
{
    return m_RtmpClientCtrl->videoCodecNameGet();
}
// get list of compatible codecs
string Api::videoCodecsGet()
{
    return m_RtmpClientCtrl->videoCodecsGet();
}
// set codec
bool Api::videoCodecSet(string codec)
{
    return m_RtmpClientCtrl->videoCodecSet(codec);
}
// get/set flash.media.Camera.bandwidth
bool Api::videoBandwidthSet(int bw)
{
    return m_RtmpClientCtrl->videoBandwidthSet(bw);
}
// get/set flash.media.Camera.bandwidth
int Api::videoBandwidthGet()
{
    return m_RtmpClientCtrl->videoBandwidthGet();
}
// get/set flash.media.Camera.quality
bool Api::videoQualitySet(string quality)
{
    return m_RtmpClientCtrl->videoQualitySet(quality);
}
// get/set flash.media.Camera.quality
string Api::videoQualityGet()
{
    return m_RtmpClientCtrl->videoQualityGet();
}
// get/set rate control tolerance (in % units; -1 o 0 means no control)
bool Api::rateCtrlToleranceSet(int rc)
{
    return m_RtmpClientCtrl->rateCtrlToleranceSet(rc);
}

int Api::rateCtrlToleranceGet()
{
    return m_RtmpClientCtrl->rateCtrlToleranceGet();
}
// is it capturing from the camera?
bool Api::isVideoMuted()
{
    return m_RtmpClientCtrl->isVideoMuted();
}

/*==== AUDIO SETTINGS ====*/

//get codec name
string Api::audioCodecNameGet()
{
    return m_RtmpClientCtrl->audioCodecNameGet();
}
// get list of compatible codecs
string Api::audioCodecsGet()
{
    return m_RtmpClientCtrl->audioCodecsGet();
}
// set codec
bool Api::audioCodecSet(string codec)
{
    return m_RtmpClientCtrl->audioCodecSet(codec);
}
// set/get mute on sender
bool Api::setMuteSender(bool on)
{
    return m_RtmpClientCtrl->setMuteSender(on);
}
bool Api::getMuteSender()
{
    return m_RtmpClientCtrl->getMuteSender();
}

/*==== H/W SETTINGS ====*/

// get list of connected cameras
string Api::videoCamerasGet()
{
    return m_RtmpClientCtrl->videoCamerasGet();
}
// get info regarding camera
string Api::videoCameraDetailsGet(string cameraId)
{
    return m_RtmpClientCtrl->videoCameraDetailsGet(cameraId);
}
// get current camera ID
string Api::videoCameraGet()
{
    return m_RtmpClientCtrl->videoCameraGet();
}
// set current camera
bool Api::videoCameraSet(string cameraID)
{
    return m_RtmpClientCtrl->videoCameraSet(cameraID);
}

/*==== A/V Player ====*/

// set video thumbnail (for 2 way video) to hide, and the position
void Api::viewVideoDisplay(bool display, int width, int height,
                             int x, int y)
{
    m_PlayerCtrl->viewVideoDisplay(display, width, height, x, y);
}

string Api::viewVideoGet()
{
    return m_PlayerCtrl->viewVideoGet();
}
// get/set publish status
void Api::videoPublishSet(string url, bool start)
{
    m_PlayerCtrl->videoPublishSet(url, start);
}
bool Api::videoPublishGet()
{
    return m_PlayerCtrl->videoPublishGet();
}
// set/get mute on player
bool Api::setMutePlayer(bool on)
{
    return m_PlayerCtrl->setMutePlayer(on);
}

bool Api::getMutePlayer()
{
    return m_PlayerCtrl->getMutePlayer();
}

void Api::autoReconnectPlayerSet(int autoReconnectTimeOut)
{
    m_PlayerCtrl->autoReconnectSet(autoReconnectTimeOut);
}

int Api::autoReconnectPlayerGet()
{
    return m_PlayerCtrl->autoReconnectGet();
}

/*==== Preview window ====*/
void Api::localPreviewDisplay(bool display,
                                int width, int height, int x, int y)
{
    m_RtmpClientCtrl->localPreviewDisplay(display, width, height, x, y);
}

/*==== Scan Bar ====*/
//bool barcodeScan()
//{
//    return m_RtmpClientCtrl->barcodeScan();
//}

//{ // Bogdan
/*========= HTML init =========*/
//void javascriptInit(bool available, string jsPrefix, bool useQueue)
//{
//    m_parentActivity->javascriptInit(available, jsPrefix, useQueue);
//}

/*======= Other functionality ======*/
// app control helper functions
//void exit(){
//    m_parentActivity->finish();
//}

// system bridge helper functions
void Api::toggleFullscreen(bool value) {
    m_parentActivity->toggleFullscreen(value);
}

void Api::toggleActionBar(bool value) {
    m_parentActivity->toggleActionBar(value);
}

string Api::displayInfoGet() {
    return (string)displayInfoGet_ciface();
}

//void showToast(string value) { // Not used?
//    m_parentActivity->showToast(value);
//}

//void notificationAdd(string title, string text, string ticker, int intType) { // Not used?
//    m_parentActivity->notificationAdd(title, text, ticker, intType);
//}

//string jsQueueGet() {
//    return m_parentActivity->jsQueueGet();
//}

//void browserReload() {
//    m_parentActivity->browserReload();
//}

//void browserBackgroundColorSet(string color) {
//    m_parentActivity->browserBackgroundColorSet(color);
//}

void Api::onError(string val) {
    return;
}

//void sendEmail(string emailAddress, string subject, string body) {
//    m_parentActivity->sendEmail(emailAddress, subject, body);
//}

//void openWebBrowser(string sURL) {
//    m_parentActivity->openWebBrowser(sURL);
//}

//} //Bogdan
