//
//  AppDelegate.m
//
//  
//

#import "AppDelegate.h"
#import "MainViewController.h"
#include "MainActivity.h"

@implementation AppDelegate

MainViewController* mainController;
BOOL m_isInBackground;

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    m_isInBackground= false;
    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application
{
	// Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
	// Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
	// Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later. 
	// If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
    m_isInBackground= true;
    UIApplication * app = [UIApplication sharedApplication];
    if([[UIDevice currentDevice] respondsToSelector:@selector(isMultitaskingSupported)])
    {
        NSLog(@"Multitasking Supported");
        
        __block UIBackgroundTaskIdentifier background_task;
        background_task = [app beginBackgroundTaskWithExpirationHandler:^ {
            
            //Clean up code. Tell the system that we are done.
            //[app endBackgroundTask: background_task];
            //background_task = UIBackgroundTaskInvalid;
        }];
        
        //To make the code block asynchronous
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            
            // background task starts
            NSLog(@"Running in the background\n");
            ((MainActivity*)((MainViewController*)self.window.rootViewController).getMainActivity)->onPause();
            while(m_isInBackground== true)
            {
                NSLog(@"Background time Remaining: %f",[[UIApplication sharedApplication] backgroundTimeRemaining]);
                [NSThread sleepForTimeInterval:4]; //wait for 1 sec
            }
            // background task ends
            
            //Clean up code. Tell the system that we are done.
            [app endBackgroundTask: background_task];
            background_task = UIBackgroundTaskInvalid;
        });
    }
    else
    {
        NSLog(@"Multitasking Not Supported");
    }
    ((MainActivity*)((MainViewController*)self.window.rootViewController).getMainActivity)->onBackgroundStateChange(true);

}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
	// Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
    m_isInBackground= false;
    ((MainActivity*)((MainViewController*)self.window.rootViewController).getMainActivity)->onBackgroundStateChange(false);
    ((MainActivity*)((MainViewController*)self.window.rootViewController).getMainActivity)->onResume();
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
	// Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
}

- (void)applicationWillTerminate:(UIApplication *)application
{
	// Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
}

@end
