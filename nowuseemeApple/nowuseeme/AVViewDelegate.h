//
//  AVViewDelegate.h
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

extern "C" {
    #import "AVViewDelegate_ciface.h" // C public inteface for this class
}

@interface AVViewDelegate : NSObject
@end
