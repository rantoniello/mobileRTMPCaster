//
//  DefaultLocalSettings.cpp
//  nowuseeme
//
//  
//

#include "DefaultLocalSettings.h"

#include "MainActivity.h"
#include "VideoRecorder.h"
#include "RtmpClientCtrl.h"
extern "C" {
    #include "log.h"
    #include "AVViewDelegate_ciface.h"
}

DefaultLocalSettings::DefaultLocalSettings(MainActivity *context, VideoRecorder *vrecorder):
    m_parentActivity(context),
    m_vrecorder(vrecorder)
{
}

string DefaultLocalSettings::cameraSelectDefault(string savedCameraId)
{
    string ret= "null"; // not available (default)

    LOGV("Selecting default/user-preferred camera\n");
    
    /* Sanity check */
    if(checkInitialization()< 0)
    {
        return ret;
    }
    
    /* Return user preferred camera if exist */
    // Check camera Id existence
    LOGV("Checking if user preferred camera is available...");
    if(m_vrecorder->videoCameraGet(savedCameraId)!= "{}")
    {
        LOG_("O.K\n");
        return savedCameraId;
    }
    LOG_("not availale.\n");

    /* Return "CAMERA_FACING_FRONT" if exist */
    LOGV("Getting a default camera -front camera is the priority-...");
    if((ret= m_vrecorder->videoCameraGetDefault())!= "null")
    {
        LOG_("O.K (camera Id: '%s')\n", ret.c_str());
        return ret;
    }
    LOG_("No camera availale!\n");
    return ret;
}

void DefaultLocalSettings::resolutionSelectDefault(int *& width, int *& height)
{
    /* Sanity check */
    if(checkInitialization()< 0)
    {
        return;
    }
    
    /*string*/m_parentActivity->getRtmpClientCtrl()->videoResolutionTest(width, height, 0);

    if(*width> 0 && *height> 0)
    {
        LOGD("Selected resolution: %dx%d\n", *width, *height);
    }
    else
    {
        LOGD("Bad camera-id passed to method 'resolutionSelectDefault()'");
    }
}

void DefaultLocalSettings::fpsSelectDefault(string cameraId, int objective_fps, int *&maxFrameRate, int *&minFrameRate)
{
    *maxFrameRate= 30; // set some defaults
    *minFrameRate= 10;
    
    /* Sanity check */
    if(checkInitialization()< 0)
    {
        return;
    }
    m_vrecorder->getNearestSupportedFps(cameraId, objective_fps, maxFrameRate, minFrameRate);
}

void DefaultLocalSettings::audioSelectDefaults(const char *micIdarg,
                               double *& sampleRate, unsigned long *& formatId,
                               unsigned long *& bytesPerFrame, unsigned long *& channelsNum,
                               unsigned long *& audioFrameSize, unsigned long *& audioBitsPerChannel)
{
    LOGV("Executing 'audioSelectDefaults()'... \n");
    ::getSupportedAudioParameters(micIdarg, sampleRate, formatId, bytesPerFrame, channelsNum, audioFrameSize, audioBitsPerChannel);
}

int DefaultLocalSettings::checkInitialization()
{
    if(m_parentActivity== NULL || m_vrecorder== NULL)
    {
        LOGE("'checkInitialization()' failed");
        return -1;
    }
    return 0;
}
