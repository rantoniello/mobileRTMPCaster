//
//  PlayerView.mm
//  nowuseeme
//
//  
//

#import "AVViewDelegate.h"
#import "MainViewController.h"
#import "CAStreamBasicDescription.h"
#import <AudioToolbox/AudioToolbox.h>
#include <vector>
#include <utility>
#import "Endian.h"
extern "C" {
    #include "log.h"
    #include "error_codes.h"
}
#include "rtmpclient-jni.h"
#include "player-jni.h"

#define DEFAULT_RESOLUTION_PRESET AVCaptureSessionPreset352x288

#ifndef max
#define max( a, b ) ( ((a) > (b)) ? (a) : (b) )
#endif

#ifndef min
#define min( a, b ) ( ((a) < (b)) ? (a) : (b) )
#endif

#define AVVIEWDELEGATE_USE_AUDIO_UNIT_FRAMEWORK
#define kOutputBus 0
#define kInputBus 1

typedef struct res_preset
{
    int width, heigth;
    NSString* preset;
} res_preset_t;

static res_preset_t available_res_preset[]=
{
//    {320,   240,    AVCaptureSessionPreset320x240},
    {352,   288,    AVCaptureSessionPreset352x288},
    {640,   480,    AVCaptureSessionPreset640x480},
//    {320,   240,    AVCaptureSessionPreset960x540},
    {1280,  720,    AVCaptureSessionPreset1280x720},
    {1920,  1080,   AVCaptureSessionPreset1920x1080},
    {NULL,  NULL,   NULL}
};

static CALayer *static_customPlayerLayer= NULL; // TODO (use GLKView)
static EAGLContext *static_customPreviewLayer= NULL;
static CIContext *static_customCoreImageContextPreviewLayer= NULL;
static GLKView *static_customGLKViewPreview= NULL;

AudioStreamBasicDescription m_audioFormat;
unsigned long m_audioFrameSize= 0;

AudioStreamBasicDescription m_static_oAudioFormat;
AudioComponentInstance m_static_playbackAudioUnit;
void *m_static_demuxer_ctx= NULL;

@interface AVViewDelegate () <AVCaptureVideoDataOutputSampleBufferDelegate, AVCaptureAudioDataOutputSampleBufferDelegate>
{
@private
    AVCaptureSession *m_captureSession; // To set up and control the camera
    AVCaptureDevice *m_camera; // A pointer to the front or to the back camera
    AVCaptureDeviceInput *m_cameraInput; // This is the data input for the camera that allows us to capture frames
    AVCaptureVideoDataOutput *m_videoOutput; // For the video frame data from the camera

    // Conversions and rendering related
    uint8_t m_permuteMap[4];
    vImage_YpCbCrPixelRange m_pixelRange;
    vImage_ARGBToYpCbCr m_infoARGBtoYpCbCr;
    vImage_Buffer m_destYp, m_destCb, m_destCr;
    size_t m_set_width, m_set_height;
    vImage_CGImageFormat m_fmt;
    iOS_data_buffer m_rtmpclient_data;

    AVCaptureDevice *m_micro; // Reference to microphone
    /* Option -1-: use capture device API for audio */
    AVCaptureDeviceInput *m_microInput; // This is the data input for the microphone
    AVCaptureAudioDataOutput *m_audioOutput; // For the audio frame data
    /* Option -2-: use AVFoundation "audio units" */
    AudioComponentInstance m_captureAudioUnit;

    NSData *m_cameraWriteBuffer;   // Pixels, captured from the camera, are copied here
    NSData *m_middleManBuffer;     // Pixels will be read from here
    NSData *m_consumerReadBuffer;  // Pixels are copied from this buffer into the App. buffer
    int m_frameWidth;
    int m_frameHeight;

    BOOL m_avCaptureSessionStarted;
    onCameraStarted_t m_onCameraStartedCallback;
    void* m_videoRecorderObjInstance;

    onFrameData_t m_onVideoFrameDataCallback;

    NSString* m_cameraId;
    int m_width;
    int m_heigth;
    int m_videoFrameRate;
    NSString* m_cameraResolutionPreset;

    NSMutableData *m_micWriteBuffer;  // audio samples, captured from the microphone, are written here
    NSData *m_micMiddleManBuffer;     // audio samples will be read from here
    NSData *m_micConsumerReadBuffer;  // audio samples are copied from this buffer into the App. buffer
    NSCondition *m_audioFrameCaptureCondition;
    BOOL m_audioFrameCaptureConditionSignaled;
    onFrameData_t m_onAudioFrameDataCallback;
}
@end

@implementation AVViewDelegate

/* ==== Public inteface methods ==== */

void* openCamera(onFrameData_t onVideoFrameDataCallback,
                 onFrameData_t onAudioFrameDataCallback,
                 onCameraStarted_t onCameraStartedCallback,
                 void* argvideoRecorderObjInstance,
                 const char *cameraId, int width, int height, int videoFrameRate)
{
    LOGD("Executing 'openCamera()'...");
    NSString *cameraIdStr= cameraId?
    [NSString stringWithCString:cameraId encoding:[NSString defaultCStringEncoding]]:
    @"null";

    AVViewDelegate *avviewdelegate= [[AVViewDelegate alloc] init: onVideoFrameDataCallback
                                        onAudioFrameDataCallback: onAudioFrameDataCallback
                                                didCameraStarted: onCameraStartedCallback
                                         videoRecorderObjInstance: argvideoRecorderObjInstance
                                                        cameraId:cameraIdStr width:width heigth:height videoFrameRate:videoFrameRate];
    if([avviewdelegate startCaptureSession: true captureAudio:true captureAudioBasedOnAUnit:false]!= true)
    {
        LOGE("Could not start camera\n");
    }
    LOGD("O.K.\n");
    return (void*)CFBridgingRetain(avviewdelegate);
}

void closeCamera(void *self)
{
    LOGV("Executing 'closeCamera()'... \n");

    [(__bridge id)(self) stopCaptureSession];
    //[avviewdelegate dealloc]; //remove this line as using ARC
}

void setupAVSublayerLayer(void* uiView, int display, int width, int height, int x, int y, const char *type)
{
    LOGV("Executing 'setupCameraSession()...'\n");

    if(((std::string)type).compare("preview")== 0)
    {
        EAGLContext * __strong *customAVLayer= &static_customPreviewLayer;
        CIContext * __strong *customCoreImageContext= &static_customCoreImageContextPreviewLayer;
        GLKView * __strong *customGKView= &static_customGLKViewPreview;

        if(display)
        {
            if((*customAVLayer)!= NULL)
            {
                LOGV("AV layer already being displayed\n");
                return;
            }
            *customAVLayer= [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
            if(!(*customAVLayer))
            {
                LOGE("Failed to create ES context\n");
                return;
            }
            (*customGKView)= [[GLKView alloc] initWithFrame: CGRectMake(x, y, width, height)];
            (*customGKView).context = *customAVLayer;
            
            [((UIViewController*)(__bridge id)(uiView)).view addSubview:(*customGKView)];
            
            GLuint _renderBuffer;
            glGenRenderbuffers(1, &_renderBuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, _renderBuffer);
            
            (*customCoreImageContext)= [CIContext contextWithEAGLContext:(*customAVLayer)];
        }
        else
        {
            if((*customAVLayer)== NULL)
            {
                LOGV("AV layer already hidden\n");
                return;
            }
            [(*customGKView) removeFromSuperview];
            [(*customGKView) deleteDrawable];
            if ([EAGLContext currentContext]== (*customAVLayer))
            {
                [EAGLContext setCurrentContext:nil];
            }
            (*customAVLayer)= NULL;
            (*customCoreImageContext)= NULL;
        } // preview type
    }
    else if(((std::string)type).compare("player")== 0)
    {
        CALayer * __strong *customAVLayer= &static_customPlayerLayer;

        if(display)
        {
            if((*customAVLayer)!= NULL)
            {
                LOGV("AV layer already being displayed\n");
                return;
            }
            (*customAVLayer)= [CALayer layer];
            
            /* Set position and resolution rectangle (x, y; width, height) */
            (*customAVLayer).frame= CGRectMake(x, y, width, height);
            
            /* Rotate accordingly */
            //(*customAVLayer).affineTransform = CGAffineTransformMakeRotation(M_PI);
            
            /* Other settings */
            (*customAVLayer).backgroundColor= [UIColor blackColor].CGColor;
            
            /* Finally, attach to main view */
            [((UIViewController*)(__bridge id)(uiView)).view.layer addSublayer:(*customAVLayer)];
        }
        else
        {
            if((*customAVLayer)== NULL)
            {
                LOGV("AV layer already hidden\n");
                return;
            }
            [(*customAVLayer) removeFromSuperlayer];
            (*customAVLayer)= NULL;
        } // player type
    }
    else
    {
        LOGV("Unknown requested AV layer\n");
        return;
    }
    LOGV("O.K.\n");
}

void image_scale(void* src, int src_width, int src_height, int src_stride,
                 void** dst, int dst_width, int dst_height, int dst_stride)
{
    vImage_Buffer input_vimage, output_scaled_vimage;
    vImage_Error vret_code= kCVReturnSuccess;
    void *to= NULL;
    int end_code = ERROR;
    
    if(src== NULL || dst== NULL)
    {
        LOGE("Bad arguments\n");
        return;
    }

    to= malloc(dst_stride* dst_height);
    if(to== NULL)
    {
        LOGE("Could not allocate temp. output buffer\n");
        goto end_image_scale;
    }

    input_vimage= {src,
        static_cast<vImagePixelCount>(src_height),
        static_cast<vImagePixelCount>(src_width),
        static_cast<size_t>(src_stride)};

    output_scaled_vimage= {to,
        static_cast<vImagePixelCount>(dst_height),
        static_cast<vImagePixelCount>(dst_width),
        static_cast<size_t>(dst_stride)};

    vImageScale_Planar8(&input_vimage, &output_scaled_vimage, NULL, kvImageNoFlags);
    if(kCVReturnSuccess!= vret_code)
    {
        LOGE("Could not perfrom 'vImageScale' (code: %d)\n", (int)vret_code);
        goto end_image_scale;
    }

    *dst= output_scaled_vimage.data;

    end_code= SUCCESS;

end_image_scale:
    
    if(end_code!= SUCCESS)
    {
        if(to!= NULL)
        {
            free(to);
            to= NULL;
        }
    }
}

#define NUM_POSTPROC 1
void renderOnPlayerSublayer(void *dstYp, int width, int height,
                            void *dstCb, void *dstCr,
                            void *pthread_presentation_mutex,
                            void *surface_view)
{
    void *to[NUM_POSTPROC]= {0};
    CGImageRef dstImage= NULL;
    vImagePixelCount w= width, h= height;
    size_t stride= width;
    vImage_Error vret_code;
    vImage_YpCbCrToARGB info;
    vImage_Buffer output_brga_vimage;
    int end_code= ERROR;

    //LOGV("Rendering decoded image; res. %dx%d\n", width, height); //Comment-me

    if(dstYp== NULL || dstCb== NULL || dstCr== NULL || pthread_presentation_mutex== NULL)
    {
        LOGE("Bad arguments\n");
        return;
    }

    // Initialize vImage related resources to perform color conversion
    vImage_Buffer srcYp= {dstYp, h, w, stride};
    vImage_Buffer srcCb= {dstCb, h/2, w/2, stride/2};
    vImage_Buffer srcCr= {dstCr, h/2, w/2, stride/2};
    uint8_t permuteMap[4]= {3, 2, 1, 0};
    vImage_YpCbCrPixelRange pixelRange= (vImage_YpCbCrPixelRange){0, 128, 255, 255, 255, 1, 255, 0};
    vImage_CGImageFormat fmt= {
        .bitsPerComponent= 8,
        .bitsPerPixel= 32,
        .colorSpace= NULL,
        .bitmapInfo= kCGBitmapByteOrder32Little| kCGImageAlphaPremultipliedFirst
    };

    // Use acceleration to convert YCbCr420P -> ARGB-32
    to[0]= malloc(width*4* height);
    output_brga_vimage= {to[0], h, w, stride*4};
    if(to[0]== NULL)
    {
        LOGE("Could not allocate temp. output buffer\n");
        goto end_renderOnPlayerSublayer;
    }

    vret_code= vImageConvert_YpCbCrToARGB_GenerateConversion(kvImage_YpCbCrToARGBMatrix_ITU_R_601_4,
                                                         &pixelRange,
                                                         &info,
                                                         kvImage420Yp8_Cb8_Cr8,
                                                         kvImageARGB8888,
                                                         kvImagePrintDiagnosticsToConsole);
    if(kCVReturnSuccess!= vret_code)
    {
        LOGE("Could not perform 'GenerateConversion' (code: %d)\n", (int)vret_code);
        goto end_renderOnPlayerSublayer;
    }
    vret_code= vImageConvert_420Yp8_Cb8_Cr8ToARGB8888(&srcYp, &srcCb, &srcCr, &output_brga_vimage,
                                                      &info, permuteMap, 1, kvImageNoFlags);
    if(kCVReturnSuccess!= vret_code)
    {
        LOGE("Could not convert '420Yp8_Cb8_Cr8->ARGB8888' (code: %d)\n", (int)vret_code);
        goto end_renderOnPlayerSublayer;
    }

    // Get destination image
    dstImage= vImageCreateCGImageFromBuffer(&output_brga_vimage, &fmt, NULL, NULL,
                                                        kvImageNoFlags, &vret_code);
    if(kCVReturnSuccess!= vret_code)
    {
        LOGE("Could not create 'CGImageRef' frame (code: %d)\n", (int)vret_code);
        usleep(1); //schedule
        return;
    }

    // Dispach video frame to player layer
    if(static_customPlayerLayer!= NULL)
    {
        dispatch_sync(dispatch_get_main_queue(), ^{
            static_customPlayerLayer.contents = (__bridge id)dstImage;
        });
    }

    end_code= SUCCESS;

end_renderOnPlayerSublayer:

    if(end_code!= SUCCESS)
    {
        usleep(1); //schedule
    }

    // Release resorces
    if(dstImage!= NULL)
    {
        CGImageRelease(dstImage);
        dstImage= NULL;
    }
    for(int i= 0; i< NUM_POSTPROC; i++)
    {
        if(to[i]!= NULL)
        {
            free(to[i]);
            to[i]= NULL;
        }
    }
}
#undef NUM_POSTPROC

const void *getLastFrame(void *self, int *frameWidth, int *frameHeight)
{
    return ([(__bridge id)(self) getLastFrame: frameWidth height: frameHeight]).bytes;
}

int isCameraStarted(void *self)
{
    return [(__bridge id)(self) isCameraStarted]? 1: 0;
}

int isCameraAvailable(const char* cameraIdarg)
{
    NSString *cameraId = [NSString stringWithCString:cameraIdarg encoding:[NSString defaultCStringEncoding]];
    AVCaptureDevice *camera= getCameraDeviceById(cameraId);

    // Set if available
    if(camera!= NULL)
    {
        return [camera isConnected]? 1: 0;
    }
    return 0; // false
}

void* getSupportedVideoSizes(const char* cameraIdarg)
{
    std::vector<std::pair<int, int>>* supportedPresets= new std::vector<std::pair<int, int>>;
    NSString *cameraId = [NSString stringWithCString:cameraIdarg encoding:[NSString defaultCStringEncoding]];
    
    // Get specified camera
    AVCaptureDevice *camera= getCameraDeviceById(cameraId);
    if(camera== NULL)
    {
        LOGE("Could not get specified camera\n");
        return NULL;
    }
    
    // We iterate over available presets... iOS API give this in a hard way...
    for(int i= 0; available_res_preset[i].preset!= NULL; i++)
    {
        // Get resolution preset structure
        res_preset_t res_preset= available_res_preset[i];
        
        // Get a dummy capture session
        AVCaptureSession* captureSession= [ [ AVCaptureSession alloc ] init ];
        
        // Check if preset is supported on the device by asking the capture session
        if(![captureSession canSetSessionPreset: res_preset.preset])
        {
            LOGE("Video capture resolution preset not supported by device\n");
            continue; // test the following preset in the available list
        }
        
        // The preset is OK; set up the capture session to use it
        [captureSession setSessionPreset: res_preset.preset];
        
        // Try to plug camera and capture sesiossion together (bt not afectively attach)
        AVCaptureDeviceInput* captureDeviceInput= NULL;
        if(!attachCameraToCaptureSession(captureSession, camera, captureDeviceInput, false))
        {
            LOGE("Video capture resolution preset not supported by specified camera\n");
            continue;
        }
        
        // Got it! Add supported preset to vector
        supportedPresets->push_back(std::make_pair(res_preset.width, res_preset.heigth));
    } // for
    
    return supportedPresets;
}

void* getSupportedPreviewFpsRange(const char* cameraIdarg)
{
    LOGV("Execution 'getSupportedPreviewFpsRange(camera Id: '%s')... '\n", cameraIdarg);
    std::vector<std::pair<int, int>>* supportedRanges= new std::vector<std::pair<int, int>>;
    NSString *cameraId = [NSString stringWithCString:cameraIdarg encoding:[NSString defaultCStringEncoding]];
    
    // Get camera device
    AVCaptureDevice *camera= getCameraDeviceById(cameraId);
    if(camera== NULL)
    {
        LOGE("Could not get specified camera device\n");
        return NULL;
    }
    
    // Itarate formats supported and get ranges
    for ( AVCaptureDeviceFormat *format in [camera formats] ) {
        for ( AVFrameRateRange *range in format.videoSupportedFrameRateRanges ) {

            // Check if range is already set
            BOOL rangeIsAlreadyRegistered= false;
            for(int i= 0; i< supportedRanges->size(); i++)
            {
                int fps_max= ((*supportedRanges)[i]).first,
                fps_min= ((*supportedRanges)[i]).second;

                LOGV("'Check if range '{%d, %d}' is already registered in vector\n", fps_max, fps_min);
                if((range.maxFrameRate== fps_max) && (range.minFrameRate== fps_min))
                {
                    LOGD("This range is already registered; continue with the following... \n");
                    rangeIsAlreadyRegistered= true;
                }
            } // for

            // Register new range
            if(!rangeIsAlreadyRegistered)
            {
                supportedRanges->push_back(std::make_pair(range.maxFrameRate, range.minFrameRate));
            }
        } // for
    }
    return supportedRanges;
}

char** videoCamerasGet()
{
    LOGV("Executing 'videoCamerasGet()'... \n");

    // Define vector to store textual description of all camera devices
    NSMutableArray* details= [[NSMutableArray alloc] init];
    
    // Get a list of available devices:
    // specifying AVMediaTypeVideo will ensure we only get a list of cameras, no microphones
    NSArray *devices= [AVCaptureDevice devicesWithMediaType: AVMediaTypeVideo];
    
    // Iterate through the device array and store textual information in vector
    for(AVCaptureDevice *device in devices)
    {
        NSString* cameraPosition= [device position]== AVCaptureDevicePositionFront? @"front":
        [device position]== AVCaptureDevicePositionBack? @"back": @"unspecified";
        NSString* cameraDetails= [NSString stringWithFormat:
                                  @"{\"id\":\"%@\",\"availability\":\"%@\",\"orientation\":\"%@\"}",
                                  [device uniqueID],
                                  [device isConnected]? @"true": @"false",
                                  cameraPosition];
        LOGD("'videoCameraDetailsGet()': %s\n", [cameraDetails UTF8String]);
        [details addObject:cameraDetails];
    }

    int details_size= [details count];
    LOGV("Number of camera detected: %d\n", details_size);

    // Dynamically allocate an array to return vector information
    char **array = (char **)malloc((details_size + 1) * sizeof(char*));
    if(array ==NULL)
    {
        LOGE("Could not allocate aray\n");
        return NULL;
    }
    for (unsigned i = 0; i < details_size; i++)
    {
        NSString *obji = [NSString stringWithFormat:@"%@", [details objectAtIndex:i]]; //description
        LOGV("Device detected: %s\n", [obji UTF8String]);
        array[i] = strdup([obji UTF8String]);
    }
    array[details_size] = NULL;
    
    //[details release]; //We're using ARC

    return array;
}

const char* videoCameraGet(const char* cameraIdarg)
{
    NSString *cameraId = cameraIdarg? [NSString stringWithCString:cameraIdarg encoding:[NSString defaultCStringEncoding]]: @"null";

    LOGV("Get camera device using given Id... ");
    AVCaptureDevice *camera= getCameraDeviceById(cameraId);
    if(camera== NULL)
    {
        LOG_("No camera found with given Id.\n");
        return [@"{}" UTF8String];
    }
    LOG_("O.K.\n");
    
    NSString* cameraPosition= [camera position]== AVCaptureDevicePositionFront? @"front":
    [camera position]== AVCaptureDevicePositionBack? @"back": @"unspecified";
    NSString* cameraDetails= [NSString stringWithFormat:
                              @"{\"id\":\"%@\",\"availability\":\"%@\",\"orientation\":\"%@\"}",
                              [camera uniqueID],
                              [camera isConnected]? @"true": @"false",
                              cameraPosition];
    LOGD("'videoCameraGet(%s)': %s\n", [cameraId UTF8String], [cameraDetails UTF8String]);
    return [cameraDetails UTF8String];
}

const char* videoCameraGetDefault()
{
    // Make sure we initialize our camera pointer:
    AVCaptureDevice* camera = NULL;
    
    // Get a list of available devices:
    // specifying AVMediaTypeVideo will ensure we only get a list of cameras, no microphones
    NSArray * devices = [ AVCaptureDevice devicesWithMediaType: AVMediaTypeVideo ];
    
    // Iterate through the device array and if a device is a camera, check if it's the one we want:
    for ( AVCaptureDevice * device in devices )
    {
        camera = device;
        
        if (AVCaptureDevicePositionFront == [ device position ] )
        {
            // Front camera is the priority
            break;
        }
    }
    
    if(camera== NULL)
    {
        return [@"null" UTF8String];
    }
    return [[camera uniqueID] UTF8String];
}

void getSupportedAudioParameters(const char* micIdarg,
                                 double *sampleRate, unsigned long *formatId,
                                 unsigned long *bytesPerFrame, unsigned long *channelsNum,
                                 unsigned long *audioFrameSize, unsigned long *audioBitsPerChannel)
{
    //NSString *micId = [NSString stringWithCString:micIdarg encoding:[NSString defaultCStringEncoding]]; // Future use

    // We instantiate and run an object of 'AVVideoDelegate' class to get same audio
    // samples and be able to deduce supported microphone parameters
    AVViewDelegate *avviewdelegate= [[AVViewDelegate alloc] init: NULL
                                        onAudioFrameDataCallback: NULL
                                                didCameraStarted: NULL
                                        videoRecorderObjInstance: NULL
                                                        cameraId:@"null" width:0 heigth:0 videoFrameRate:0];
    
    // We *ONLY* capture audio in this "testing" session
    if([avviewdelegate startCaptureSession: false captureAudio:true captureAudioBasedOnAUnit:false]!= true)
    {
        LOGE("Could not start session\n");
    }

    // Get supported parameters
    [avviewdelegate getAudioSessionSupportedParameters: sampleRate
                                              formatId: (AudioFormatID*)formatId
                                         bytesPerFrame: (UInt32*)bytesPerFrame
                                           channelsNum: (UInt32*)channelsNum
                                        audioFrameSize: (UInt32*)audioFrameSize
                                   audioBitsPerChannel: (UInt32*)audioBitsPerChannel];

    LOGD("sample rate: %f formatId: %u bytesPerFrame: %u channelsNum: %u audioFrameSize: %u audioBitsPerChannel: %u\n", *sampleRate,
         (unsigned int)*formatId, (unsigned int)*bytesPerFrame, (unsigned int)*channelsNum,
         (unsigned int)*audioFrameSize, (unsigned int)*audioBitsPerChannel);

    // Got parameters! Stop session then... and we're done.
    [avviewdelegate stopCaptureSession];
}

const void *getLastAudioFrame(void *self, int *frameSize)
{
    return ([(__bridge id)(self) getLastAudioFrame: frameSize]).bytes;
}

/* ==== Private inteface methods ==== */

/* ==== Audio processing based on "Audio Unit" ==== */

void au_checkStatus(int status, int line){
    if (status) {
        LOGE("Status not 0! %d (line: %d)\n", status, line);
        //		exit(1);
    }
}

- (AudioComponentInstance)getCaptureAudioUnit
{
    return m_captureAudioUnit;
}

/**
 This callback is called when new audio data from the microphone is
 available.
 */
static OSStatus au_recordingCallback(void *inRefCon,
                                  AudioUnitRenderActionFlags *ioActionFlags,
                                  const AudioTimeStamp *inTimeStamp,
                                  UInt32 inBusNumber,
                                  UInt32 inNumberFrames,
                                  AudioBufferList *ioData) {
    
    AVViewDelegate *avviewdelegate= (__bridge AVViewDelegate*)inRefCon;

    //LOGV("%s\n", __func__); // Comment-me

    // Because of the way our audio format (setup below) is chosen:
    // we only need 1 buffer, since it is mono
    // Samples are 16 bits = 2 bytes.
    // 1 frame includes only 1 sample
    
    AudioBuffer buffer;

    buffer.mNumberChannels = m_audioFormat.mChannelsPerFrame;
    buffer.mDataByteSize = inNumberFrames * 2;
    buffer.mData = malloc( inNumberFrames * 2 );
    
    // Put buffer in a AudioBufferList
    AudioBufferList bufferList;
    bufferList.mNumberBuffers = 1;
    bufferList.mBuffers[0] = buffer;
    
    // Then:
    // Obtain recorded samples
    
    OSStatus status;
    
    status = AudioUnitRender([avviewdelegate getCaptureAudioUnit],
                             ioActionFlags,
                             inTimeStamp,
                             inBusNumber,
                             inNumberFrames,
                             &bufferList);
    au_checkStatus(status, __LINE__);
    
    // Now, we have the samples we just read sitting in buffers in bufferList
    // Process the new data
    [avviewdelegate au_processAudio:&bufferList];
    
    // release the malloc'ed data in the buffer we created earlier
    //if(buffer.mData!= NULL)
    //{
        //free(buffer.mData);
        //free(bufferList.mBuffers[0].mData);
    //}
    for(int i= 0; i< bufferList.mNumberBuffers; i++)
    {
        free(bufferList.mBuffers[i].mData);
    }
    return noErr;
}

int avviewdelegate_createAudioEngineAndPlayer(void *demuxer_ctx)
{
    m_static_demuxer_ctx= demuxer_ctx;
    playback_au_init();
    au_start(m_static_playbackAudioUnit);
    return 0;
}

void avviewdelegate_audioEngineRelease()
{
    au_stop(m_static_playbackAudioUnit);
}

/**
 This callback is called when the audioUnit needs new data to play through the
 speakers. If you don't have any, just don't write anything in the buffers
 */
static OSStatus au_playbackCallback(void *inRefCon,
                                 AudioUnitRenderActionFlags *ioActionFlags,
                                 const AudioTimeStamp *inTimeStamp,
                                 UInt32 inBusNumber,
                                 UInt32 inNumberFrames,
                                 AudioBufferList *ioData)
{
    //LOGV("%s\n", __func__); // Comment-me
    for (int i= 0; i< ioData->mNumberBuffers; i++)
    {
        // in practice we will only ever have 1 buffer, since audio format is mono
        AudioBuffer buffer= ioData->mBuffers[i];

        /* Execute rendering callback */
        playerCallback(m_static_demuxer_ctx, buffer.mData, buffer.mDataByteSize);
    }
    return noErr;
}

static int playback_au_init()
{
    OSStatus status;
    
    LOGV("%s\n", __func__);
    
    // Describe audio component
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    
    // Get component
    AudioComponent inputComponent = AudioComponentFindNext(NULL, &desc);
    
    // Get audio units
    status = AudioComponentInstanceNew(inputComponent, &m_static_playbackAudioUnit);
    au_checkStatus(status, __LINE__);
    
    // Enable IO for playback
    UInt32 flag = 1;
    status = AudioUnitSetProperty(m_static_playbackAudioUnit,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Output,
                                  kOutputBus,
                                  &flag,
                                  sizeof(flag));
    au_checkStatus(status, __LINE__);
    
    // Apply format
    m_static_oAudioFormat.mSampleRate			= 44100;
    m_static_oAudioFormat.mFormatID			= kAudioFormatLinearPCM;
    m_static_oAudioFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    m_static_oAudioFormat.mFramesPerPacket	= 1;
    m_static_oAudioFormat.mChannelsPerFrame	= 1;
    m_static_oAudioFormat.mBitsPerChannel		= 16;
    m_static_oAudioFormat.mBytesPerPacket		= 2;
    m_static_oAudioFormat.mBytesPerFrame		= 2;

    status = AudioUnitSetProperty(m_static_playbackAudioUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  kOutputBus,
                                  &m_static_oAudioFormat,
                                  sizeof(m_audioFormat));
    au_checkStatus(status, __LINE__);

    // Describe format
    LOGV("Audio format: \n"
         "- Sample rate: %f\n"
         "- Number of channels: %d\n"
         "- Format Id.: %u\n"
         "- Frames per Packet: %u\n"
         "- Bits per Channel: %u\n"
         "- Bytes per Packet: %u\n"
         "- Bytes per Frame: %u\n"
         "- Format Flags: %u\n"
         "\n",
         m_static_oAudioFormat.mSampleRate,
         (unsigned int)m_static_oAudioFormat.mChannelsPerFrame,
         (unsigned int)m_static_oAudioFormat.mFormatID,
         (unsigned int)m_static_oAudioFormat.mFramesPerPacket,
         (unsigned int)m_static_oAudioFormat.mBitsPerChannel,
         (unsigned int)m_static_oAudioFormat.mBytesPerPacket,
         (unsigned int)m_static_oAudioFormat.mBytesPerFrame,
         (unsigned int)m_static_oAudioFormat.mFormatFlags
         );
    
    // Set output callback
    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = au_playbackCallback;
    callbackStruct.inputProcRefCon = NULL;
    status = AudioUnitSetProperty(m_static_playbackAudioUnit,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Global,
                                  kOutputBus,
                                  &callbackStruct,
                                  sizeof(callbackStruct));
    au_checkStatus(status, __LINE__);

    
    // Initialise
    status = AudioUnitInitialize(m_static_playbackAudioUnit);
    au_checkStatus(status, __LINE__);

    return (int)status;
}

/**
 Initialize the audioUnit and allocate our own temporary buffer.
 The temporary buffer will hold the latest data coming in from the microphone,
 and will be copied to the output when this is requested.
 */
- (id) capture_au_init
{
    OSStatus status;

    LOGV("%s\n", __func__);
    
    // Describe audio component
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    
    // Get component
    AudioComponent inputComponent = AudioComponentFindNext(NULL, &desc);
    
    // Get audio units
    status = AudioComponentInstanceNew(inputComponent, &m_captureAudioUnit);
    au_checkStatus(status, __LINE__);
    
    // Enable IO for recording
    UInt32 flag = 1;
    status = AudioUnitSetProperty(m_captureAudioUnit,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Input,
                                  kInputBus,
                                  &flag,
                                  sizeof(flag));
    au_checkStatus(status, __LINE__);

    // Apply format
    status = AudioUnitSetProperty(m_captureAudioUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output,
                                  kInputBus,
                                  &m_audioFormat,
                                  sizeof(m_audioFormat));
    au_checkStatus(status, __LINE__);

    // Describe format
    LOGV("Audio format: \n"
         "- Sample rate: %f\n"
         "- Number of channels: %d\n"
         "- Format Id.: %u\n"
         "- Frames per Packet: %u\n"
         "- Bits per Channel: %u\n"
         "- Bytes per Packet: %u\n"
         "- Bytes per Frame: %u\n"
         "- Format Flags: %u\n"
         "\n",
         m_audioFormat.mSampleRate,
         (unsigned int)m_audioFormat.mChannelsPerFrame,
         (unsigned int)m_audioFormat.mFormatID,
         (unsigned int)m_audioFormat.mFramesPerPacket,
         (unsigned int)m_audioFormat.mBitsPerChannel,
         (unsigned int)m_audioFormat.mBytesPerPacket,
         (unsigned int)m_audioFormat.mBytesPerFrame,
         (unsigned int)m_audioFormat.mFormatFlags
         );

    // Set input callback
    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = au_recordingCallback;
    callbackStruct.inputProcRefCon = (__bridge void*)self;
    status = AudioUnitSetProperty(m_captureAudioUnit,
                                  kAudioOutputUnitProperty_SetInputCallback,
                                  kAudioUnitScope_Global,
                                  kInputBus,
                                  &callbackStruct,
                                  sizeof(callbackStruct));
    au_checkStatus(status, __LINE__);

    // Disable buffer allocation for the recorder (optional - do this if we want to pass in our own)
    flag = 0;
    status = AudioUnitSetProperty(m_captureAudioUnit,
                                  kAudioUnitProperty_ShouldAllocateBuffer,
                                  kAudioUnitScope_Output,
                                  kInputBus,
                                  &flag,
                                  sizeof(flag));

    // Initialise
    status = AudioUnitInitialize(m_captureAudioUnit);
    au_checkStatus(status, __LINE__);
    
    return self;
}

/**
 Start the audioUnit. This means data will be provided from
 the microphone, and requested for feeding to the speakers, by
 use of the provided callbacks.
 */
static void au_start(AudioComponentInstance audioUnit)
{
    LOGV("%s\n", __func__);
    OSStatus status = AudioOutputUnitStart(audioUnit);
    au_checkStatus(status, __LINE__);
}

/**
 Stop the audioUnit
 */
static void au_stop(AudioComponentInstance audioUnit)
{
    LOGV("%s\n", __func__);
    OSStatus status = AudioOutputUnitStop(audioUnit);
    au_checkStatus(status, __LINE__);

    // Release audio processing resources
    //[super	dealloc]; // Using ARC
    AudioUnitUninitialize(audioUnit);
    LOGV("%s\n", __func__);
}

/**
 Change this funtion to decide what is done with incoming
 audio data from the microphone.
 Right now we copy it to our own temporary buffer.
 */
- (void) au_processAudio: (AudioBufferList*) audioBufferList
{
    //LOGV("Processing incoming data from Audio Unit\n"); // Comment-me

    // Allocate memory for copying the audio samples
    m_micWriteBuffer= [NSMutableData data];

    // Copy audio frame samples
    UInt32 audioFrameSize= 0;
    for(int i= 0; i< audioBufferList->mNumberBuffers; i++)
    {
        AudioBuffer audioBuffer= audioBufferList->mBuffers[i];
        Float32 *aframe= (Float32*)audioBuffer.mData;
        //LOGV("aframe pointer: %p; aframe size: %u; Buffer list index.: %d\n",
        //     aframe, (unsigned int)audioBuffer.mDataByteSize, i); // Comment-me (debugging purposes)
        [m_micWriteBuffer appendBytes:aframe length:audioBuffer.mDataByteSize];
        audioFrameSize+= audioBuffer.mDataByteSize;
    }
    //LOGV("audioFrameSize= %d\n", (int)audioFrameSize);

    // Make the copied bytes available for 'consuming' by App.:
    // - Block access to the two buffer pointers:
    @synchronized( self )
    {
        // 'Swap' the buffer pointers:
        m_micMiddleManBuffer = m_micWriteBuffer;
        //[m_micWriteBuffer release]; // using ARC
        m_micWriteBuffer = NULL;
    }

    // Call video frame processing callback
    if(m_onAudioFrameDataCallback!= NULL)
        m_onAudioFrameDataCallback((__bridge void*)(self), m_videoRecorderObjInstance);
}

/* ==== Audio/Video processing based on "Capture Devices" ==== */

static AVCaptureDevice* getCameraDeviceById(NSString* cameraId)
{
    // Get a list of available devices:
    // specifying AVMediaTypeVideo will ensure we only get a list of cameras, no microphones
    NSArray *devices= [AVCaptureDevice devicesWithMediaType: AVMediaTypeVideo];
    
    // Iterate through the device array and if a device is a camera, check if it's the one we want:
    for(AVCaptureDevice *device in devices)
    {
        if([[device uniqueID] isEqualToString: cameraId])
        {
            LOGV("Camera found with specified Id: '%s'\n", [cameraId UTF8String]);
            return device;
        }
    }
    LOGE("No camera vailable with specified Id\n");
    return NULL;
}

static AVCaptureDevice* setCamera(NSString* cameraId)
{
    // Initialize camera pointer
    AVCaptureDevice* camera= NULL;
    
    // Get a list of available devices
    // specifying AVMediaTypeVideo will ensure we only get a list of cameras, no microphones
    NSArray* devices= [ AVCaptureDevice devicesWithMediaType: AVMediaTypeVideo ];
    
    // Iterate through the device array and get specific desired camera if possible
    for ( AVCaptureDevice * device in devices )
    {
        if([[device uniqueID] isEqualToString: cameraId])
        {
            camera= device; // camera found!
            return camera;
        }
    }
    
    // Finally, if we still don't have one, we'll try to get any available camera, priority will be front camera
    if(camera== NULL)
    {
        for ( AVCaptureDevice * device in devices )
        {
            camera= device;
            if([camera position]== AVCaptureDevicePositionFront)
            {
                return camera;
            }
        }
    }
    
    // Reached this point camera== NULL (no device found/available)
    return camera;
}

static NSString* setCameraResolutionPreset(int w, int h)
{
    // We iterate over available presets... iOS API give this in a hard way...
    for(int i= 0; available_res_preset[i].preset!= NULL; i++)
    {
        if(w== available_res_preset[i].width && h== available_res_preset[i].heigth)
        {
            // got it!
            return available_res_preset[i].preset;
        }
    }

    // Reached this point, cameraResolutionPreset returned is the default 'DEFAULT_RESOLUTION_PRESET'
    return DEFAULT_RESOLUTION_PRESET;
}

static BOOL setCameraVideoFrameRate(AVCaptureDevice *camera, int videoFrameRate)
{
    // Set a default frame rate for the camera
    if(camera!= NULL)
    {
        // We firt need to lock the camera, so noone else can mess with its configuration:
        if ( [ camera lockForConfiguration: NULL ] )
        {
            // Set a minimum frame rate of 10 frames per second
            [ camera setActiveVideoMinFrameDuration: CMTimeMake( 1, 10 ) ]; //FIXME!!
            
            // and a maximum of 30 frames per second
            [ camera setActiveVideoMaxFrameDuration: CMTimeMake( 1, videoFrameRate ) ];
            
            [ camera unlockForConfiguration ];
        }
    }
    
    // If we've found the camera we want, return true
    return (camera!= NULL);
}

static BOOL attachCameraToCaptureSession(AVCaptureSession* captureSession,
                                         AVCaptureDevice* cameraArg,
                                         AVCaptureDeviceInput* captureDeviceInputArg,
                                         BOOL doAttachCameraArg)
{
    // Check we've found the camera and set up the session first
    if(cameraArg== NULL)
    {
        LOGE("Camera should bet set\n");
        return false;
    }
    if(captureSession== NULL)
    {
        LOGE("Session not allocated, should be set\n");
        return false;
    }
    
    // Re-initialize the camera input if apply
    captureDeviceInputArg= NULL;
    
    // Request a camera input from the camera
    NSError* error= NULL;
    captureDeviceInputArg= [AVCaptureDeviceInput deviceInputWithDevice: cameraArg error: &error];
    
    // Check if we've got any errors
    if(error!= NULL)
    {
        LOGE("Could not get camera to input interface\n");
        return false;
    }
    
    // We've got the input from the camera, now attach it to the capture session if requested
    if([captureSession canAddInput: captureDeviceInputArg])
    {
        if(doAttachCameraArg)
            [captureSession addInput: captureDeviceInputArg];
    }
    else
    {
        LOGE("Could not attach camera to input interface\n");
        return false;
    }
    
    // Done (If the case, the attaching was successful)
    return true;
}

static BOOL attachMicroToCaptureSession(AVCaptureSession* captureSession,
                                         AVCaptureDevice* microArg,
                                         AVCaptureDeviceInput* captureDeviceInputArg,
                                         BOOL doAttachMicroArg)
{
    // Check we've found the microphone and set up the session first
    if(microArg== NULL)
    {
        LOGE("Microhpne should bet set\n");
        return false;
    }
    if(captureSession== NULL)
    {
        LOGE("Session not allocated, should be set\n");
        return false;
    }
    
    // Re-initialize the microphone input if apply
    captureDeviceInputArg= NULL;
    
    // Request an audio input from the microphone
    NSError* error= NULL;
    captureDeviceInputArg= [AVCaptureDeviceInput deviceInputWithDevice: microArg error: &error];

    // Check if we've got any errors
    if(error!= NULL)
    {
        LOGE("Could not get microphone to input interface\n");
        return false;
    }

    // We've got the input from the microphone, now attach it to the capture session if requested
    if([captureSession canAddInput: captureDeviceInputArg])
    {
        if(doAttachMicroArg)
            [captureSession addInput: captureDeviceInputArg];
    }
    else
    {
        LOGE("Could not attach microphone to input interface\n");
        return false;
    }
    
    // Done (If the case, the attaching was successful)
    return true;
}

- ( id ) init: (onFrameData_t) onVideoFrameDataCallback
onAudioFrameDataCallback: (onFrameData_t)onAudioFrameDataCallback
didCameraStarted: (onCameraStarted_t)onCameraStartedCallback
videoRecorderObjInstance: (void*)argvideoRecorderObjInstance
     cameraId: (NSString*)cameraIdArg
        width: (int)widthArg
       heigth: (int)heigthArg
videoFrameRate: (int)videoFrameRateArg
{
    vImagePixelCount width= widthArg, heigth= heigthArg;
    size_t rowBytes= widthArg;

    // Initialize the parent class(es) up the hierarchy and create self:
    self = [ super init ];
    
    // Initialize members:
    m_captureSession    = NULL;
    m_camera            = NULL;
    m_cameraInput       = NULL;
    m_videoOutput       = NULL;

    m_permuteMap[0]= 3;
    m_permuteMap[1]= 2;
    m_permuteMap[2]= 1;
    m_permuteMap[3]= 0;
    m_pixelRange= (vImage_YpCbCrPixelRange){0, 128, 255, 255, 255, 1, 255, 0};
    vImage_Error error;
    error= vImageConvert_ARGBToYpCbCr_GenerateConversion(kvImage_ARGBToYpCbCrMatrix_ITU_R_601_4,
                                                         &m_pixelRange,
                                                         &m_infoARGBtoYpCbCr,
                                                         kvImageARGB8888,
                                                         kvImage420Yp8_Cb8_Cr8,
                                                         kvImagePrintDiagnosticsToConsole);
    if(error!= kvImageNoError)
    {
        LOGE("Could not initialize 'ARGBToYpCbCr_GenerateConversion'\n");
    }

    void *toY= malloc(widthArg* heigthArg);
    void *toCb= malloc(widthArg* heigthArg/ 4);
    void *toCr= malloc(widthArg* heigthArg/ 4);
    if(toY== NULL || toCb== NULL || toCr== NULL)
    {
        LOGE("Could not allocate converiosn frame buffers\n");
    }
    m_destYp= {toY, heigth, width, rowBytes};
    m_destCb= {toCb, heigth/2, width/2, rowBytes/2};
    m_destCr= {toCr, heigth/2, width/2, rowBytes/2};
    m_set_width= widthArg;
    m_set_height= heigthArg;

    m_fmt= {.bitsPerComponent= 8,
        .bitsPerPixel= 32,
        .colorSpace= NULL,
        .bitmapInfo= kCGBitmapByteOrder32Little| kCGImageAlphaPremultipliedFirst
    };

    m_rtmpclient_data= {
        .ybuf= m_destYp.data,
        .ysize= static_cast<int>(m_destYp.rowBytes*m_destYp.height),
        .cbbuf= m_destCb.data,
        .cbsize= static_cast<int>(m_destCb.rowBytes*m_destCb.height),
        .crbuf= m_destCr.data,
        .crsize= static_cast<int>(m_destCb.rowBytes*m_destCb.height),
    };

    m_micro             = NULL;
    m_microInput        = NULL;
    m_audioOutput       = NULL;
    
    // Video frame buffering
    m_cameraWriteBuffer     = NULL;
    m_middleManBuffer       = NULL;
    m_consumerReadBuffer    = NULL;
    
    // Initialize the frame size:
    m_frameWidth = 0;
    m_frameHeight = 0;
    
    // State related
    m_avCaptureSessionStarted= FALSE;
    m_onCameraStartedCallback= onCameraStartedCallback;
    m_videoRecorderObjInstance= argvideoRecorderObjInstance;
    
    // Preview layer
    m_onVideoFrameDataCallback = onVideoFrameDataCallback;
    
    // Video capturing session parameters
    m_cameraId= cameraIdArg;
    m_width= widthArg;
    m_heigth= heigthArg;
    m_videoFrameRate= videoFrameRateArg;
    m_cameraResolutionPreset= DEFAULT_RESOLUTION_PRESET;

    // Audio capturing session parameters
    m_onAudioFrameDataCallback= onAudioFrameDataCallback;

    return self;
}

- ( void ) setupVideoOutput
{
    LOGD("Executing 'setupVideoOutput'\n");
    
    // Create the video data output
    m_videoOutput = [ [ AVCaptureVideoDataOutput alloc ] init ];
    
    // Create a queue for capturing video frames
    dispatch_queue_t captureQueue = dispatch_queue_create( "captureQueue", DISPATCH_QUEUE_SERIAL );
    
    // Use the AVCaptureVideoDataOutputSampleBufferDelegate capabilities of CameraDelegate:
    [ m_videoOutput setSampleBufferDelegate: self queue: captureQueue ];
    
    // Set up the video output
    // Do we care about missing frames?
    m_videoOutput.alwaysDiscardsLateVideoFrames = YES; //NO;
    
    // Preview only supports RGB format
    NSNumber * framePixelFormat = [ NSNumber numberWithInt: kCVPixelFormatType_32BGRA ];

    m_videoOutput.videoSettings = [ NSDictionary dictionaryWithObject: framePixelFormat
                                                               forKey: ( id ) kCVPixelBufferPixelFormatTypeKey ];

    //{ // Comment-me
#if 0
        //TODO: if H264 codec is supported...
        NSLog(@"availableVideoCodecTypes= %@", [m_videoOutput availableVideoCodecTypes]);
        NSDictionary *videoCompressionProps = [NSDictionary dictionaryWithObjectsAndKeys:
                                               [NSNumber numberWithDouble:128.0*1024.0], AVVideoAverageBitRateKey, nil ];
        m_videoOutput.videoSettings = [[NSDictionary dictionaryWithObjectsAndKeys:
                                        AVVideoCodecH264, AVVideoCodecKey,
                                        [NSNumber numberWithInt:352], AVVideoWidthKey,
                                        [NSNumber numberWithInt:288], AVVideoHeightKey,
                                        videoCompressionProps, AVVideoCompressionPropertiesKey, nil];
#endif
    //}

    // Add the video data output to the capture session
    [ m_captureSession addOutput: m_videoOutput ];
    
    LOGD("O.K.\n");
}

- ( void ) setupAudioOutput
{
    LOGD("Executing 'setupAudioOutput'\n");

    // Create the audio data output
    m_audioOutput= [[AVCaptureAudioDataOutput alloc] init];

    // Create a queue for capturing audio frames
    dispatch_queue_t audioQueue= dispatch_queue_create("audioQueue", DISPATCH_QUEUE_SERIAL);

    // Use the AVCaptureAudioDataOutputSampleBufferDelegate capabilities
    [m_audioOutput setSampleBufferDelegate: self queue: audioQueue];
    //dispatch_release(audioQueue);

    // Setup the Output Configurations
    /*NSDictionary *audioSettings = [NSDictionary dictionaryWithObjectsAndKeys:
                                    [NSNumber numberWithInt:kAudioFormatLinearPCM], AVFormatIDKey,
                                    [NSNumber numberWithInt:44100], AVSampleRateKey,
                                    [NSNumber numberWithInt:1], AVNumberOfChannelsKey,
                                    [NSNumber numberWithInt:16], AVLinearPCMBitDepthKey,
                                    [NSNumber numberWithBool:NO], AVLinearPCMIsFloatKey,
                                    [NSNumber numberWithBool:NO], AVLinearPCMIsBigEndianKey,
                                    nil];
    [m_audioOutput setAudioSettings:audioSettings];*/ // Apply Output Settings
    // :( not available for Ios, only for OSX; we must use "audio units" for conversion

    // Add the audio data output to the capture session
    if([m_captureSession canAddOutput:m_audioOutput])
    {
        [m_captureSession addOutput:m_audioOutput];
    }
    else
    {
        LOGE("Could not add audio data output to the capture session\n");
    }
    //[m_audioOutput release];

    LOGD("O.K.\n");
}

- (BOOL) startCaptureSession: (BOOL)captureVideo
captureAudio: (BOOL)captureAudio
captureAudioBasedOnAUnit: (BOOL)captureAudioBasedOnAUnit
{
    // Make sure we have a capture session
    if(m_captureSession== NULL)
    {
        m_captureSession= [ [ AVCaptureSession alloc ] init ];
    }

    if(captureVideo)
    {
        // Try to set camera for session
        if((m_camera= setCamera(m_cameraId))== NULL)
        {
            LOGE("Could not set camera\n");
            return false;
        }

        // Get video preset, and check if it is supported on the device by asking the capture session
        m_cameraResolutionPreset= setCameraResolutionPreset(m_width, m_heigth);
        if(![m_captureSession canSetSessionPreset: m_cameraResolutionPreset])
        {
            LOGE("Video capture resolution preset not supported\n");
            return false;
        }
        
        // The preset is OK, now set up the capture session to use it
        [ m_captureSession setSessionPreset: m_cameraResolutionPreset ];
        
        // Plug camera and capture sesiossion together
        if(!attachCameraToCaptureSession(m_captureSession, m_camera, m_cameraInput, true))
        {
            LOGE("Could not attach camera to capturre session\n");
            return false;
        }

        // Set capture video frame-rate
        if(!setCameraVideoFrameRate(m_camera, m_videoFrameRate))
        {
            LOGE("Could not set specified video capturing frame-rate\n");
            return false;
        }

        // Initialize the frame size:
        m_frameWidth = 0;
        m_frameHeight = 0;

        // Add the video output
        [self setupVideoOutput];
    }

    if(captureAudio)
    {
        if(!captureAudioBasedOnAUnit)
        {
            // Try to set microphone for session
            m_micro= [[AVCaptureDevice devicesWithMediaType:AVMediaTypeAudio] firstObject];
            if(m_micro== NULL)
            {
                LOGE("Could not set microphone");
                return false;
            }

            // Plug microphone to capture session
            if(!attachMicroToCaptureSession(m_captureSession, m_micro, m_microInput, true))
            {
                LOGE("Could not attach microphone to capturre session\n");
                return false;
            }

            // Add the audio output
            [self setupAudioOutput];
        }
        else
        {
            // Configure audio input/output (Audio Unit based capturing)
            [ self capture_au_init ];
        }
    }

    // Set up a callback, so we are notified when the session actually starts
    [ [ NSNotificationCenter defaultCenter ] addObserver: self
                                                selector: @selector( avCaptureSessionStarted: )
                                                    name: AVCaptureSessionDidStartRunningNotification
                                                  object: m_captureSession ];

    // Start capturing
    [ m_captureSession startRunning ];
    if(captureAudioBasedOnAUnit)
    {
        au_start(m_captureAudioUnit);
    }

    // Note: Returning true from this function only means that setting up went OK.
    // It doesn't mean that the camera has started yet.
    // We get notified about the camera having started in the avCaptureSessionStarted() callback.
    return true;
}

- (void) stopCaptureSession
{
    if(m_captureSession== NULL)
    {
        LOGW("Camera is alredy stoped\n");
        return;
    }

    // Mutex to stop and close the camera while capturing
    @synchronized(self)
    {
        if([m_captureSession isRunning])
        {
            [m_captureSession stopRunning];

            if(m_videoOutput!= NULL)
            {
                [m_captureSession removeOutput:m_videoOutput];
            }
            
            if(m_cameraInput!= NULL)
            {
                [m_captureSession removeInput:m_cameraInput];
            }

            if(m_audioOutput!= NULL)
            {
                [m_captureSession removeOutput:m_audioOutput];
            }
            
            if(m_microInput!= NULL)
            {
                [m_captureSession removeInput:m_microInput];
            }

            // The rest og the job is for garbage collector
            m_cameraWriteBuffer= NULL;
            m_middleManBuffer= NULL;
            m_consumerReadBuffer= NULL;
            
            m_captureSession= NULL;
            m_camera= NULL;
            m_cameraInput= NULL;
            m_videoOutput= NULL;
            
            m_cameraId= NULL;
            m_cameraResolutionPreset= NULL;

            if(m_destYp.data!= NULL)
            {
                free(m_destYp.data);
                m_destYp.data= NULL;
            }
            if(m_destCb.data!= NULL)
            {
                free(m_destCb.data);
                m_destCb.data= NULL;
            }
            if(m_destCr.data!= NULL)
            {
                free(m_destCr.data);
                m_destCr.data= NULL;
            }

            m_micro= NULL;
            m_microInput= NULL;
            m_audioOutput= NULL;

            m_audioFrameCaptureCondition= NULL;
        }

        au_stop(m_captureAudioUnit);

    } // @synchronized
}

- (void) copyVideoFrame: (CMSampleBufferRef) sampleBuffer
{
    vImage_Error error;
    void *from= NULL, *to2= NULL;
    //vImage_Buffer output_brga_vimage, output_brga_flipped_vimage; // FIXME!!

    // Get a pointer to the image and pixel buffer:
    CVImageBufferRef imageBuffer=  CMSampleBufferGetImageBuffer(sampleBuffer);
    CVPixelBufferRef pixelBuffer= ( CVPixelBufferRef )imageBuffer;
    
    // Obtain access to the pixel buffer by locking its base address
    CVOptionFlags lockFlags= 0;
    CVReturn status = CVPixelBufferLockBaseAddress(pixelBuffer, lockFlags);
    //assert(kCVReturnSuccess== status);
    if(kCVReturnSuccess!= status) {
        LOGE("Could not lock video frame buffer base\n");
        usleep(1); //schedule
        return;
    }

    // Copy bytes from the pixel buffer
    // - First, work out how many bytes we need to copy
    size_t bytesPerRow= CVPixelBufferGetBytesPerRow(pixelBuffer);
    size_t height= CVPixelBufferGetHeight(pixelBuffer);
    size_t width= CVPixelBufferGetWidth(pixelBuffer);
    NSUInteger numBytesToCopy= bytesPerRow* height;
    //LOGV("bytesPerRow= %d\n", (int)bytesPerRow); //Comment-me
    
    // - Then work out where in memory we'll need to start copying
    from= CVPixelBufferGetBaseAddress(pixelBuffer);


    // Check necessary class members initialization
    if(m_destYp.data== NULL || m_destCb.data== NULL || m_destCr.data== NULL)
    {
        LOGE("Converiosn frame buffers not alocated\n");
        usleep(1); //schedule
        return;
    }
    if(m_set_width!= width || m_set_height!= height)
    {
        LOGE("Frame dimensions not expected\n");
        usleep(1); //schedule
        return;
    }

    // Use acceleration to convert ARGB-32 -> YCbCr420P
    vImage_Buffer srcrgba888= {from, height, width, bytesPerRow};
    error= vImageConvert_ARGB8888To420Yp8_Cb8_Cr8(&srcrgba888, &m_destYp, &m_destCb, &m_destCr,
                                           &m_infoARGBtoYpCbCr, m_permuteMap, kvImageNoFlags);
    if(kCVReturnSuccess!= error)
    {
        LOGE("Could not create 'vImageConvert' (code: %d)\n", (int)error);
        usleep(1); //schedule
        return;
    }

#if 0
    // Rotate image accordingly
    if([m_camera position]== AVCaptureDevicePositionFront)
    {
        to2= malloc(width*4* height);
        if(to2== NULL)
        {
            LOGE("Could not allocate temp. output buffer\n");
            usleep(1); //schedule
            return;
        }
        output_brga_flipped_vimage= {to2, height, width, width*4};
        vImageVerticalReflect_ARGB8888(&output_brga_vimage,
                                       &output_brga_flipped_vimage,
                                       kvImageNoFlags);
    }
    else
    {
        output_brga_flipped_vimage= output_brga_vimage;
    }
#endif
    //output_brga_flipped_vimage= output_brga_vimage; // FIXME!!

    // Get destination image
    CIImage *image = [CIImage imageWithCVPixelBuffer:pixelBuffer];

    // Dispach video frame to preview layer
    if(static_customPreviewLayer!= NULL)
    {
        //LOGV("Dispaching video frame to preview layer...'\n");
        dispatch_sync(dispatch_get_main_queue(), ^{
            [static_customCoreImageContextPreviewLayer drawImage:image
                                                          inRect: CGRectMake(0, 0,
                                                                             static_customGLKViewPreview.drawableWidth,
                                                                             static_customGLKViewPreview.drawableHeight)
                                                        fromRect:[image extent] ];
            [static_customPreviewLayer presentRenderbuffer:GL_RENDERBUFFER];
        });
        //LOGV("O.K.\n");
    }

    // Allocate memory for copying the pixels and copy them
    m_cameraWriteBuffer= [NSData dataWithBytes: &m_rtmpclient_data length: sizeof(iOS_data_buffer)];

    // Make the copied bytes available for 'consuming' by App.
    // - Block access to the two buffer pointers:
    @synchronized( self )
    {
        // 'Swap' the buffer pointers:
        m_middleManBuffer= m_cameraWriteBuffer;
        m_cameraWriteBuffer= NULL;
        
        m_frameWidth= (int)bytesPerRow;
        m_frameHeight= (int)height;
    }

    // Let go of the access to the pixel buffer by unlocking the base address:
    CVOptionFlags unlockFlags= 0;
    CVPixelBufferUnlockBaseAddress(pixelBuffer, unlockFlags);
    
    // Call video frame processing callback
    if(m_onVideoFrameDataCallback!= NULL)
        m_onVideoFrameDataCallback((__bridge void*)(self), m_videoRecorderObjInstance);
}

- ( void ) copyAudioFrame: ( CMSampleBufferRef ) sampleBuffer
{
    //LOGV("ASAMPE|"); // Comment-me (debugging purposes)

    // Get the sample buffer's AudioStreamBasicDescription
    CMFormatDescriptionRef formatDescription= CMSampleBufferGetFormatDescription(sampleBuffer);
    CAStreamBasicDescription sampleBufferASBD(*CMAudioFormatDescriptionGetStreamBasicDescription(formatDescription));
    if(kAudioFormatLinearPCM!= sampleBufferASBD.mFormatID)
    {
        NSLog(@"Bad format or bogus ASBD!");
        NSLog(@"AVCaptureAudioDataOutput Audio Format:");
        sampleBufferASBD.Print();
        usleep(1); // schedule
        return;
    }

    // Get frame pointer and number of samples
    AudioBufferList audioBufferList;
    CMBlockBufferRef blockBuffer;
    CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(sampleBuffer, NULL, &audioBufferList, sizeof(audioBufferList), NULL, NULL, 0, &blockBuffer);

    // Allocate memory for copying the audio samples
    m_micWriteBuffer= [NSMutableData data];

    // Copy audio frame samples
    UInt32 audioFrameSize= 0;
    for(int i= 0; i< audioBufferList.mNumberBuffers; i++)
    {
        AudioBuffer audioBuffer= audioBufferList.mBuffers[i];
        Float32 *aframe= (Float32*)audioBuffer.mData;
        //LOGV("aframe pointer: %p; aframe size: %u; Buffer list index.: %d\n",
        //     aframe, (unsigned int)audioBuffer.mDataByteSize, i); // Comment-me (debugging purposes)
        [m_micWriteBuffer appendBytes:aframe length:audioBuffer.mDataByteSize];
        audioFrameSize+= audioBuffer.mDataByteSize;
    }

    // Make the copied bytes available for 'consuming' by App.:
    // - Block access to the two buffer pointers:
    @synchronized( self )
    {
        // 'Swap' the buffer pointers:
        m_micMiddleManBuffer = m_micWriteBuffer;
        m_micWriteBuffer = NULL;

        // Lock audio-capture condition
        [m_audioFrameCaptureCondition lock];

        m_audioFormat.mSampleRate= 44100.00; //sampleBufferASBD.mSampleRate; //FIXME!!
        m_audioFormat.mFormatID= sampleBufferASBD.mFormatID;
        m_audioFormat.mBytesPerPacket= sampleBufferASBD.mBytesPerPacket;
        m_audioFormat.mFramesPerPacket= sampleBufferASBD.mFramesPerPacket;
        m_audioFormat.mChannelsPerFrame= 1; //sampleBufferASBD.mChannelsPerFrame;
        m_audioFormat.mBytesPerFrame= sampleBufferASBD.mBytesPerFrame;
        m_audioFormat.mBitsPerChannel= sampleBufferASBD.mBitsPerChannel;
        m_audioFormat.mFormatFlags= sampleBufferASBD.mFormatFlags;
        m_audioFrameSize= audioFrameSize;

        //LOGV("Signaling AFRAME!\n"); //Comment-me
        m_audioFrameCaptureConditionSignaled= true;
        [m_audioFrameCaptureCondition broadcast];

        // Unlock
        [m_audioFrameCaptureCondition unlock];
    }

    // Release locked sample buffer
    CFRelease(blockBuffer);

    // Call video frame processing callback
    if(m_onAudioFrameDataCallback!= NULL)
        m_onAudioFrameDataCallback((__bridge void*)(self), m_videoRecorderObjInstance);
}

- (void) getAudioSessionSupportedParameters: (Float64*) sampleRate
                                       formatId: (AudioFormatID*) formatId
                                       bytesPerFrame: (UInt32*) bytesPerFrame
                                       channelsNum: (UInt32*) channelsNum
                                       audioFrameSize:(UInt32*) audioFrameSize
                                       audioBitsPerChannel: (UInt32*) audioBitsPerChannel
{
    // Lock audio-capture condition
    [m_audioFrameCaptureCondition lock];

    // Wait for session to start
    LOGD("Starting testing capture session to get audio supported parameters\n");
    m_audioFrameCaptureConditionSignaled= false;
    while(m_audioFrameCaptureConditionSignaled== false)
        [m_audioFrameCaptureCondition wait];

    LOGD("O.K.! Got parameters\n");
    *sampleRate= m_audioFormat.mSampleRate;
    *formatId= m_audioFormat.mFormatID; // (4 bytes UInt32 with value 'lpcm')
    *bytesPerFrame= m_audioFormat.mBytesPerFrame;
    *channelsNum= m_audioFormat.mChannelsPerFrame;
    *audioFrameSize= m_audioFrameSize;
    *audioBitsPerChannel= m_audioFormat.mBitsPerChannel;

    // Got parameters! Unlock then...
    [m_audioFrameCaptureCondition unlock];
}

- (const NSData * const) getLastFrame: (int *) frameWidth
                                 height: (int *) frameHeight
{
    @synchronized(self)
    {
        m_consumerReadBuffer= m_middleManBuffer;
        m_middleManBuffer= NULL;
        
        *frameWidth= m_frameWidth;
        *frameHeight= m_frameHeight;
    }
    return m_consumerReadBuffer;
}

- (const NSData * const) getLastAudioFrame: (int *) frameSize
{
    @synchronized(self)
    {
        m_micConsumerReadBuffer= m_micMiddleManBuffer;
        m_micMiddleManBuffer= NULL;
        
        *frameSize= m_audioFrameSize;
    }
    return m_micConsumerReadBuffer;
}

- ( void ) captureOutput: ( AVCaptureOutput * ) captureOutput
   didOutputSampleBuffer: ( CMSampleBufferRef ) sampleBuffer
          fromConnection: ( AVCaptureConnection * ) connection
{
    // Check if this is the output we are expecting (Note, here we can catch video, audio, ...)
    if(captureOutput== m_videoOutput)
    {
        static int cnt= 0; if(cnt++) {cnt= 0; return; } //HACK: reduce to half the FPs
        [self copyVideoFrame: sampleBuffer];
    }
    if(captureOutput== m_audioOutput)
    {
        [self copyAudioFrame: sampleBuffer];
    }
}

- ( void ) avCaptureSessionStarted: ( NSNotification * ) note
{
    // This callback has done its job, now disconnect it
    [ [ NSNotificationCenter defaultCenter ] removeObserver: self
                                                       name: AVCaptureSessionDidStartRunningNotification
                                                     object: m_captureSession ];
    
    // Change state
    m_avCaptureSessionStarted= TRUE;
    
    // Now send an event to App.
    if(m_onCameraStartedCallback!= NULL && m_videoRecorderObjInstance!= NULL)
        m_onCameraStartedCallback(m_videoRecorderObjInstance);
}

- ( BOOL ) isCameraStarted
{
    LOGD("%s\n", __func__);
    return m_avCaptureSessionStarted;
}

@end
