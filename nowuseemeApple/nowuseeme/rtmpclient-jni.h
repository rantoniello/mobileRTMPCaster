//
//  rtmpclient-jni.h
//  nowuseeme
//
//  
//

#ifndef nowuseeme_rtmpclient_jni_h
#define nowuseeme_rtmpclient_jni_h

#include <stdint.h>

#ifndef _ANDROID_
typedef struct iOS_data_buffer
{
    void* ybuf;
    int ysize;
    void* cbbuf;
    int cbsize;
    void *crbuf;
    int crsize;
} iOS_data_buffer;

void setRtmpClientCtrlObj(void *t);
void releaseRtmpClientCtrlObj();
void rtmpclientOpen();
void rtmpclientClose();
int rtmpclientInit(const char* array, int vavailable, int aavailable);
void rtmpclientRelease();
int rtmpclientGetABufSize();
int encodeVFrame(char* array);
int encodeAFrame(int16_t *array, int framesize);
int runBackground();
int setBackgroundImage(char *array, int length);
int stopBackground();
void rtmpClientConnectionTimeOutSet(int tout);
#endif

#endif // nowuseeme_rtmpclient_jni_h
