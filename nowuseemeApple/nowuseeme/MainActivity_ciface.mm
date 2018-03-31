//
//  MainActivity_ciface.mm
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
    #import "MainActivity_ciface.h" // C public inteface for this class
}

const char* displayInfoGet_ciface()
{
    //retrieve display information
    float scaleFactor= [[UIScreen mainScreen] scale];
    CGRect screen= [[UIScreen mainScreen] bounds];
    CGFloat widthInPixel= screen.size.width; // * scaleFactor;
    CGFloat heightInPixel= screen.size.height; // * scaleFactor;
    std::string s= "{\"width\" : "+ std::to_string(widthInPixel)+
    ", \"height\" : " + std::to_string(heightInPixel)+
    ", \"density\" : " + std::to_string(scaleFactor)+ "}";
    
    return s.c_str();
}
