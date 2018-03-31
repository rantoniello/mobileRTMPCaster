//
//  DefaultLocalSettings.h
//  nowuseeme
//
//  
//

#ifndef __nowuseeme__DefaultLocalSettings__
#define __nowuseeme__DefaultLocalSettings__

#include <stdio.h>
#include <string>

using namespace std;

class MainActivity;
class VideoRecorder;

/**
 * Class defining default supported parameters for initializing application.
 */
class DefaultLocalSettings
{
private:
    MainActivity *m_parentActivity;
    VideoRecorder *m_vrecorder;

public:
    DefaultLocalSettings(MainActivity *context, VideoRecorder *vrecorder);

    /**
     * Select default hardware camera, if available.
     * @param savedCameraId Suggested camera considering user preferences.
     * @return Suggested or default camera id, string "null" if no hardware camera
     * is available. If user suggested camera is not available, facing-front
     * camera is the priority.
     */
    string cameraSelectDefault(string savedCameraId);

    /**
     * Select default recording resolution for camera, if camera is available.
     * @param width pointer reference to default width value (set to -1 if camera given
     * is not available).
     * @param height pointer reference to default height value (set to -1 if camera given
     * is not available).
     * Note that for iOS only "preset" related resolution values are enabled.
     */
    void resolutionSelectDefault(int *& width, int *& height);

    /**
     * Return the nearest supported range of frames-per-second (fps) given a
     * reference value, for a specified camera device. If the reference 'fps'
     * value is not within any supported range, the nearest range is returned.
     * @param cameraId The hardware camera to access.
     * @param objective_fps Reference FPS value
     * @param maxFrameRate returned max frame rate integer pointer reference
     * @param minFrameRate returned min frame rate integer pointer reference
     */
    void fpsSelectDefault(string cameraId, int objective_fps, int *&maxFrameRate, int *&minFrameRate);

    /**
     * Test possible audio-format combinations to be able to support
     * opening "audio-recorder" on a wide set of devices/emulators.
     * @return valid audio sample rate on success, -1 on fail.
     */
    void audioSelectDefaults(const char *micIdarg,
                             double *& sampleRate, unsigned long *& formatId,
                             unsigned long *& bytesPerFrame, unsigned long *& channelsNum,
                             unsigned long *& audioFrameSize, unsigned long *& audioBitsPerChannel);

private:

    /**
     * Check if mandatory attributes are initialized
     * @return Error code 0 if succeed, -1 if fails.
     */
    int checkInitialization();
};

#endif /* defined(__nowuseeme__DefaultLocalSettings__) */
