//
//  MainViewController_ciface.h
//
//  
//

#ifndef __nowuseeme__MainViewController_ciface__
#define __nowuseeme__MainViewController_ciface__

void performOnErrorOnMainThread(void *uiviewObject, int errorCode);
void performOnPlayerInputResolutionChangedOnMainThread(void *uiviewObject, const char *args[]);
const char* getBackgroundImagePath();
void toggleFullscreen(void *self, int hideBar);
void toggleActionBar(void *self, int hideBar);

#endif // __nowuseeme__MainViewController_ciface__
