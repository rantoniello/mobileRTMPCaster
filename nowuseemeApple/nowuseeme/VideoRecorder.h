//
//  VideoRecorder.h
//  nowuseeme
//
//  
//

#ifndef __nowuseeme__VideoRecorder__
#define __nowuseeme__VideoRecorder__

#include <stdio.h>
#include <string>
#include <mutex>
#include <vector>
#include <utility>

using namespace std;
typedef void Camera;

class MainActivity;
class CameraThread;

/**
 * class VideoRecorder:
 * This class is in charge of capturing video frames from available cameras
 * and pass them to JNI RTMP encoding/multiplexing application.
 */
class VideoRecorder
{
private:
    /* Private attributes */
    int MAX_WAITTIME_MILLIS;
    //SurfaceHolder m_holder;
    Camera *m_camera;
    MainActivity/*Context*/ *m_context;
    MainActivity *m_parentActivity;
    string m_cameraId; // Default camera
    volatile bool m_previewRunning;
    volatile bool m_recording;
    volatile bool m_isSurfaceCreated;
    int m_width, m_height, m_frameRate;
    bool m_setMuteOn;
    char *m_callbackBuffer;
    CameraThread *m_CameraThread;
    mutex m_lock;

public:
    VideoRecorder(MainActivity *context);

    /**
     * Reset SurfaceView parameters and start recording.
     * @param cameraId Camera id.
     * @param width Image width.
     * @param height Image height.
     * @param rate Camera video frame-rate.
     */
    void start(string cameraId, int width, int height, int rate,
               bool setMuteOn);

    /**
     * Stop VideoRecording: stop capturing, stop preview and release camera
     * resource.
     */
    void stop();

    /**
     * Set VideoRecording preview layout characteristics
     */
    void localPreviewDisplay(bool display, int width, int height, int x, int y);

    /**
     * Lock/release camera. This method is intended to lock camera to get/set
     * default supported setting.
     * @param lock boolean indicating lock/release camera.
     * @param The hardware camera to access
     */
    void takeCmaera(bool take, string cameraId);

    /**
     * Check if VideoRecorder is recording.
     * @return bool indicating if VideoRecorder is currently recording.
     */
    bool isRecording();

    /**
     * Check if VideoRecorder has its surface created
     * @return Boolean indicating if surface is already created
     */
    bool isSurfaceCreated();

    /**
     * Surface creation callback
     */
    static void surfaceCreated(void *videoRecorderObjInstance);

    /**
     * Surface creation callback
     */
    //static void surfaceChanged(void *videoRecorderObjInstance, int format, int width, int height); // not used

    /**
     * Surface destructor callback
     */
    static void surfaceDestroyed(void *videoRecorderObjInstance);

    /**
     * Video frame capturing callback
     */
    static void onPreviewFrame(void *t, void* videoRecorderObjInstance);

    /**
     * Audio frame capturing callback
     */
    static void onAudioFrame(void *t, void* videoRecorderObjInstance);

    /*
     * API IMPLEMENTATION SUPPORT
     */
    
    /**
     * Check if available camera exist in device.
     */
    bool isCameraAvailable(string cameraId);

    /**
     * Return list of capturing available resolutions.
     * @param cameraId The hardware camera to access.
     * @return vector of available resolutions, null if cameraId is not available
     * IMPORTANT: vector should be released on calling method.
     * Note that for an available cameraID, this method will always return
     * a vector with at least one pair element.
     */
    vector<pair<int, int>>* getSupportedVideoSizes(string cameraId);

    /**
     * Get preferred or recommended preview size (width and height) in pixels
     * for video recording if possible. Else, it returns at least a supported
     * resolution.
     * @param cameraId The hardware camera to access
     * @return recommended preview size, null if cameraId is not available.
     * Note that for an available cameraID, this method will always return
     * a recommended preview size.
     */
    //void getPreferredPreviewSizeForVideo(string cameraId, int *w, int *h) // Does not apply in iOS (not supported anyway)

    /**
     * Gets the supported preview fps (frame-per-second) ranges.
     * @param cameraId The hardware camera to access.
     * @return vector of available ranges, null if cameraId is not available
     * IMPORTANT: vector should be released on calling method.
     * Note that for an available cameraID, this method returns a list with
     * at least one element.
     */
    vector<pair<int, int>>* getSupportedPreviewFpsRange(string cameraId);

    /**
     * Return the nearest supported range of frames-per-second (fps) given a 
     * reference value, for a specified camera device. If the reference 'fps' 
     * value is not within any supported range, the nearest range is returned.
     * @param cameraId The hardware camera to access.
     * @param objective_fps Reference FPS value
     * @param maxFrameRate returned max frame rate integer pointer reference
     * @param minFrameRate returned min frame rate integer pointer reference
     */
    void getNearestSupportedFps(string cameraId, int objective_fps, int *&maxFrameRate, int *&minFrameRate);

    /**
     * Get list of connected cameras with textual description
     */
    char** videoCamerasGet();
    string videoCameraGet(string cameraId);
    string videoCameraGetDefault();

    /**
     * Mute audio recorder
     * @param on boolean to mute/un-mute
     */
    void setMute(bool on);
    bool getMute();

private:
    /**
     * Access a particular hardware camera and get its parameters.
     * @param cameraId (camera identifier)
     * @return camera parameters on success, null on error.
     */
    //private Camera.Parameters getCameraParameters(string cameraId); // Does not apply in iOS

    void resetSurfaceParameters();
};

#endif /* defined(__nowuseeme__VideoRecorder__) */
