//
//  CameraThread.cpp
//  nowuseeme
//
//  
//

#include "CameraThread.h"

extern "C" {
    #include "log.h"
}
#include "MainActivity.h"
#include "ErrorCodes.h"

CameraThread::CameraThread(MainActivity *context,
                           onFrameData_t onVideoFrameData,
                           onFrameData_t onAudioFrameData,
                           onCameraStarted_t onCameraStarted, void* videoRecorderObjInstance):
    m_camera(NULL),
    m_parentActivity((MainActivity*)context),
    //m_lock
    m_onVideoFrameDataCallback(onVideoFrameData),
    m_onAudioFrameDataCallback(onAudioFrameData),
    m_onCameraStartedCallback(onCameraStarted),
    m_videoRecorderObjInstance(videoRecorderObjInstance)

{
}

Camera* CameraThread::open(string cameraId, int width, int height, int rate)
{
    LOGD("Opening camera\n");
    m_lock.lock();

    if(m_camera!= NULL)
    {
        LOGW("Camera already instantiated\n");
        goto end_open;
    }
    m_camera= openCamera(m_onVideoFrameDataCallback,
                         m_onAudioFrameDataCallback,
                         m_onCameraStartedCallback,
                         m_videoRecorderObjInstance,
                         cameraId.c_str(), width, height, rate);
    LOGD("%s\n", (m_camera!= NULL)? "Camera successfuly opened": "Camera failed to open");

end_open:

    m_lock.unlock();
    return m_camera;
}

void CameraThread::close()
{
    LOGD("Closing camera\n");
    if(m_camera== NULL)
    {
        LOGW("Camera is already closed\n");
        return;
    }
    m_lock.lock();
    closeCamera(m_camera);
    m_camera= NULL;
    m_lock.unlock();
    LOGD("Camera closed\n");
}
