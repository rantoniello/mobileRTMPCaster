//
//  main.m
//  nowuseeme
//
//  
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include <sys/signal.h>

#import "AppDelegate.h"

int main(int argc, char * argv[])
{
    @autoreleasepool {
        signal(SIGPIPE, SIG_IGN);
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
