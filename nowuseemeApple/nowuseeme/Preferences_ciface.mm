//
//  Preferences_ciface.mm
//  nowuseeme
//
//  
//

#import <Foundation/Foundation.h>
#import <GLKit/GLKit.h>
#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>
#import <CoreMedia/CoreMedia.h>
#import <Accelerate/Accelerate.h>

#include <string>

extern "C" {
    #import "Preferences_ciface.h" // C public inteface for this class
}

int Preferences_open_ciface()
{
    NSUserDefaults *preferences = [NSUserDefaults standardUserDefaults];
    NSString *isFirstRunKey= @"isFirstRun";
    BOOL isFirstRun= FALSE;

    if([preferences objectForKey:isFirstRunKey]== nil)
    {
        BOOL isFirstRun= TRUE;
        [preferences setBool:isFirstRun forKey:isFirstRunKey];
    }
    else
    {
        // Get current preferences values
        isFirstRun= [preferences boolForKey:isFirstRunKey];
    }
    return isFirstRun? 1: 0;
}
