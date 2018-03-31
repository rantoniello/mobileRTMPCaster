//
//  AVViewDelegate_ciface.h
//  nowuseeme
//
//  
//

#ifndef __nowuseeme__AVViewDelegate_ciface__
#define __nowuseeme__AVViewDelegate_ciface__

typedef void(*onFrameData_t)(void* t, void* videoRecorderObjInstance);
typedef void(*onCameraStarted_t)(void* t);

void* openCamera(onFrameData_t onVideoFrameDataCallback,
                 onFrameData_t onAudioFrameDataCallback, 
                 onCameraStarted_t onCameraStartedCallback,
                 void* videoRecorderObjInstance,
                 const char *cameraId, int width, int height, int rate);
void closeCamera(void *myObjectInstance);

const void* getLastFrame(void *myObjectInstance, int *frameWidth, int *frameHeight);
int isCameraStarted(void *myObjectInstance);

int isCameraAvailable(const char* cameraId);
void* getSupportedVideoSizes(const char* cameraIdarg);
void* getSupportedPreviewFpsRange(const char* cameraIdarg);
char** videoCamerasGet();
const char* videoCameraGet(const char* cameraIdarg);
const char* videoCameraGetDefault();

void getSupportedAudioParameters(const char* micIdarg,
                                 double *sampleRate, unsigned long *formatId,
                                 unsigned long *bytesPerFrame, unsigned long *channelsNum,
                                 unsigned long *audioFrameSize, unsigned long *audioBitsPerChannel);
const void* getLastAudioFrame(void *myObjectInstance, int *frameSize);

void setupAVSublayerLayer(void* uiView, int display, int width, int height, int x, int y, const char *type);

void renderOnPlayerSublayer(void *dstYp, int width, int heigth,
                            void *dstCb, void *dstCr,
                            void *pthread_presentation_mutex,
                            void *surface_view);
void image_scale(void* src, int src_width, int src_height, int src_stride,
                 void** dst, int dst_width, int dst_height, int dst_stride);

int avviewdelegate_createAudioEngineAndPlayer(void *demuxer_ctx);
void avviewdelegate_audioEngineRelease();

#endif // AVViewDelegate_ciface
