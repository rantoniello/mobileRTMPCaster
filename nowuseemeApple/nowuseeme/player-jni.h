//
//  player-jni.h
//  nowuseeme
//
//  
//

#ifndef nowuseeme_player_jni_h
#define nowuseeme_player_jni_h

#include <stdint.h>

#ifndef _ANDROID_
void setPlayerCtrlObj(void *t);
void releasePlayerCtrlObj();

int playerStart(const char *url, int width, int height, int isMuteOn, void *surface);
void playerStop();
void playerMute(int on);
void resetSurface(int width, int height, void *surface);
void playerConnectionTimeOutSet(int tout);

void playerCallback(void* demuxer_ctx, void *rendering_buf, int expected_size);
#endif

#endif // nowuseeme_player_jni_h
