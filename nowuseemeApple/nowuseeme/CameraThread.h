//
//  CameraThread.h
//  nowuseeme
//
//  
//

#ifndef __nowuseeme__CameraThread__
#define __nowuseeme__CameraThread__

#include <stdio.h>
#include <string>
#include <mutex>
extern "C" {
#include "AVViewDelegate_ciface.h"
}

using namespace std;
typedef void Camera;

class MainActivity;

class CameraThread
{
private:
    Camera *m_camera; // Objective-C Object managing camera session
    MainActivity *m_parentActivity;
    mutex m_lock; // mutual-exclusion lock for public interface
    onFrameData_t m_onVideoFrameDataCallback;
    onFrameData_t m_onAudioFrameDataCallback;
    onCameraStarted_t m_onCameraStartedCallback;
    void* m_videoRecorderObjInstance;

public:
    CameraThread(MainActivity *context,
                 onFrameData_t onVideoFrameData,
                 onFrameData_t onAudioFrameData,
                 onCameraStarted_t onCameraStarted, void *videoRecorderObjInstance);

    Camera* open(string cameraId, int width, int height, int rate);
    void close();
};

#endif /* defined(__nowuseeme__CameraThread__) */
