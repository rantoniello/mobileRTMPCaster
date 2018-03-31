//
//  VideoRecorder.cpp
//  nowuseeme
//
//  
//

#include "VideoRecorder.h"

#include <stdlib.h>
#include <climits>
#include "rtmpclient-jni.h"
extern "C" {
    #include "log.h"
    #include "AVViewDelegate_ciface.h"
}
#include "MainActivity.h"
#include "CameraThread.h"
#include "DisplayCtrl.h"

VideoRecorder::VideoRecorder(MainActivity *context):
    /* Private attributes */
    MAX_WAITTIME_MILLIS(10*1000),
    //SurfaceHolder m_holder= null,
    m_camera(NULL),
    //Context *m_context= null,
    m_parentActivity(context),
    m_cameraId("null"), // Default camera
    m_previewRunning(false),
    m_recording(false),
    m_isSurfaceCreated(false),
    m_width(352), m_height(288), m_frameRate(15),
    m_setMuteOn(false),
    m_callbackBuffer(NULL),
    m_CameraThread(NULL)
    //mutex m_lock
{
    LOGD("Creating 'VideoRecorder'...\n");
    LOGD("'VideoRecorder' successfuly created.\n");
}
    
void VideoRecorder::start(string cameraId, int width, int height, int rate,
                          bool setMuteOn)
{
    LOGD("Starting video recorder...\n");
    if(m_recording)
    {
        LOGW("video recorder was already running\n");
        return;
    }
    
    m_lock.lock();

    /* Reset preview parameters */
    m_cameraId= cameraId;
    m_width= width;
    m_height= height;
    m_frameRate= rate;

    m_setMuteOn= setMuteOn;

    m_recording= true; //NOTE: set this before calling 'resetSurfaceParameters()'!!
    resetSurfaceParameters();

    m_lock.unlock();
}

void VideoRecorder::stop()
{
    LOGD("Stopping video recorder...\n");
    m_recording= false; // do not lock
    m_previewRunning= false;  // do not lock
    
    m_lock.lock();

     if(m_CameraThread!= NULL)
     {
         m_CameraThread->close();
         m_CameraThread= NULL;
         m_camera= NULL;
     }

    m_lock.unlock();
}

void VideoRecorder::localPreviewDisplay(bool display, int width, int height, int x, int y)
{
    LOGD("Executing 'localPreviewDisplay(%s, %d, %d, %d, %d %s)\n",
         display? "true": "false", width, height, x, y, "preview");
    m_lock.lock();
    
    DisplayCtrl::changeDisplayParams(m_parentActivity, display, width, height, x, y, "preview");
    
    m_lock.unlock();
}

void VideoRecorder::takeCmaera(bool lock, string cameraId)
{
    //FIXME: TODO
}

bool VideoRecorder::isRecording()
{
    bool isRecording= (m_recording && m_previewRunning);
    //LOGD("isRecording() returns: %s\n", isRecording? "true": "false");
    return isRecording;
}

bool VideoRecorder::isSurfaceCreated()
{
    return m_isSurfaceCreated;
}

/*static*/ void VideoRecorder::surfaceCreated(void *videoRecorderObjInstance)
{
    LOGD("Surface Created\n");
    ((VideoRecorder*)videoRecorderObjInstance)->m_isSurfaceCreated= true;
}

///*static*/ void VideoRecorder::surfaceChanged(void *videoRecorderObjInstance, int format, int width, int height) {}

/*static*/ void VideoRecorder::surfaceDestroyed(void *videoRecorderObjInstance)
{
    LOGV("Surface destroyed\n");
    ((VideoRecorder*)videoRecorderObjInstance)->m_previewRunning= false;
    if(((VideoRecorder*)videoRecorderObjInstance)->m_CameraThread!= NULL)
    {
        ((VideoRecorder*)videoRecorderObjInstance)->m_CameraThread->close();
        ((VideoRecorder*)videoRecorderObjInstance)->m_CameraThread= NULL;
    }
    ((VideoRecorder*)videoRecorderObjInstance)->m_isSurfaceCreated= false;
}

void VideoRecorder::onPreviewFrame(void *t, void* videoRecorderObjInstance)
{
    int frameWidth= -1, frameHeight= -1;
    char* data= (char*)getLastFrame(t, &frameWidth, &frameHeight);

    //{ //deleteme: debugging purposes
#if 1
    static int cnt= 0;
    if(cnt++>= 60)
    {
        LOGD("[%p:%d:%d]\n", data, frameWidth, frameHeight);
        cnt= 0;
    }
#endif
    //}

    if(data== NULL)
        return;

    if(((VideoRecorder*)videoRecorderObjInstance)->isRecording())
    {
        /* Pass camera frame to JNI */
        /*int retcode=*/ encodeVFrame(data);
        //if(retcode<= 0) // comment-me
        //{
        //	// Error occurred; set last error code in parent/main activity
        //	m_parentActivity.setLastErrorCode(
        //			ErrorCodes.getByCode(retcode)
        //	);
        //}
    }
}

void VideoRecorder::onAudioFrame(void *t, void* videoRecorderObjInstance)
{
    int audioFrameSize= -1;
    char* data= (char*)getLastAudioFrame(t, &audioFrameSize);
    
    //{ //deleteme: debugging purposes
#if 1
    static int cnt= 0;
    if(cnt++>= 60)
    {
        LOGD("[%p:%d]\n", data, audioFrameSize);
        cnt= 0;
    }
#endif
    //}

    if(data== NULL)
        return;

    /* Put audio frame into encoder-multiplexer */
    if(((VideoRecorder*)videoRecorderObjInstance)->isRecording() && audioFrameSize> 0)
    {

        /* Set mute if apply */
        if(((VideoRecorder*)videoRecorderObjInstance)->m_setMuteOn)
        {
            if(data!= NULL) memset(data, 0, audioFrameSize);
        }

        /*int retcode=*/ encodeAFrame((int16_t*)data, audioFrameSize/2); // pass size in short units
        //if(retcode<= 0) //comment-me
        //{
        //	// Error occurred; set last error code in parent/main activity
        //	((MainActivity)m_context).setLastErrorCode(
        //			ErrorCodes.getByCode(retcode)
        //	);
        //}
    }

    /*if(data!= NULL)
    {
        free(data);
        data= NULL;
    }*/
}

bool VideoRecorder::isCameraAvailable(string cameraId)
{
    bool isAvailable= false;

    m_lock.lock();

    if(::isCameraAvailable(cameraId.c_str()))
        isAvailable= true;
    
    m_lock.unlock();

    LOGD("isCameraAvailable() returns: %s\n", isAvailable? "true": "false");
    return isAvailable;
}

vector<pair<int, int>>* VideoRecorder::getSupportedVideoSizes(string cameraId)
{
    vector<pair<int, int>>* supportedVideoSizes= NULL;
    m_lock.lock();
    supportedVideoSizes= (std::vector<std::pair<int, int>>*)
    ::getSupportedVideoSizes(cameraId.c_str());
    m_lock.unlock();
    return supportedVideoSizes; // will return null if cameraId is not availale
}

vector<pair<int, int>>* VideoRecorder::getSupportedPreviewFpsRange(string cameraId)
{
    vector<pair<int, int>> *range_list;
    LOGV("Executing 'getSupportedPreviewFpsRange()... '\n");
    m_lock.lock();
    range_list= (vector<pair<int, int>>*)::getSupportedPreviewFpsRange(cameraId.c_str());
    m_lock.unlock();
    return range_list;
}

void VideoRecorder::getNearestSupportedFps(string cameraId, int objective_fps,
                                            int *&maxFrameRate, int *&minFrameRate)
{
    LOGV("Executing 'getNearestSupportedFps()... '\n");
    int diff= INT32_MAX, ret_idx= 0;
    *maxFrameRate= 30; // set some defaults
    *minFrameRate= 10;

    m_lock.lock();

    vector<pair<int, int>> *range_list= (vector<pair<int, int>>*)
        ::getSupportedPreviewFpsRange(cameraId.c_str());
    if(range_list== NULL)
    {
        goto end_getNearestSupportedFps;
    }
    
    for(int i= 0; i< range_list->size(); i++)
    {
        int fps_max= ((*range_list)[i]).first,
        fps_min= ((*range_list)[i]).second;
        LOGV("'getNearestSupportedFps()' ret[%d]= '{%d, %d}'; fps-objective= %d\n",
             i, fps_max, fps_min, objective_fps);

        if((objective_fps<= fps_max) && (objective_fps>= fps_min))
        {
            LOGD("'getNearestSupportedFps()' returns '{%d, %d}'\n", fps_max, fps_min);
            *maxFrameRate= fps_max;
            *minFrameRate= fps_min;
            goto end_getNearestSupportedFps;
        }
        else
        {
            int curr_diff= std::min(abs(objective_fps- fps_max), abs(objective_fps- fps_min));
            if(curr_diff< diff)
            {
                diff= curr_diff;
                ret_idx= i;
            }
        }
    }
    *maxFrameRate= ((*range_list)[ret_idx]).first;
    *minFrameRate= ((*range_list)[ret_idx]).second;
    LOGD("'getNearestSupportedFps()' returns '{%d, %d}'\n",
         ((*range_list)[ret_idx]).first, ((*range_list)[ret_idx]).second);
    

end_getNearestSupportedFps:

    if(range_list!= NULL)
    {
        range_list->clear();
        delete range_list;
        range_list= NULL;
    }
    m_lock.unlock();
    return;
}

char** VideoRecorder::videoCamerasGet()
{
    return ::videoCamerasGet();
}

string VideoRecorder::videoCameraGet(string cameraId)
{
    LOGV("Executing 'videoCameraGet(id: '%s')'... \n", cameraId.c_str());
    return ::videoCameraGet(cameraId.c_str()); //FIXME!! delete ret?
}

string VideoRecorder::videoCameraGetDefault()
{
    return ::videoCameraGetDefault(); //FIXME!! delete ret?
}

void VideoRecorder::setMute(bool on)
{
    LOGD("Setting microphone Mute: %s\n", on? "true": "false");
    m_lock.lock();
    m_setMuteOn= on;
    m_lock.unlock();
}

bool VideoRecorder::getMute()
{
    bool on;
    LOGD("Getting microphone Mute... \n");
    m_lock.lock();
    on= m_setMuteOn;
    m_lock.unlock();
    LOGD("Microphone Mute set to: %s\n", on? "true": "false");
    return on;
}

/*
 * Private method: Stop and release camera if being used. Re-opens camera
 * (note that the camera released could be a different one to the new
 * camera opened; this is signaled by the 'camera id.' parameter),
 * set new parameters and finally restart preview.
 */
void VideoRecorder::resetSurfaceParameters()
{
    bool recordingStateBkp= m_recording;
    
    /* Stop preview and release camera */
    if(m_previewRunning)
    {
        m_previewRunning= false;
        if(m_CameraThread!= NULL)
        {
            m_CameraThread->close();
            m_CameraThread= NULL;
        }
    }
    
    /* Instantiate camera wrapper */
    if(m_CameraThread== NULL)
    {
        m_CameraThread= new CameraThread(m_context,
                                         VideoRecorder::onPreviewFrame,
                                         VideoRecorder::onAudioFrame,
                                         VideoRecorder::surfaceCreated,
                                         this);
    }

    /* Run camera thrad with specific preview properties/parameters */
    m_camera= m_CameraThread->open(m_cameraId, m_width, m_height, m_frameRate);
    if(m_camera== NULL)
    {
        this->stop();
        return;
    }

    m_previewRunning= true;
    m_recording= recordingStateBkp;
}
