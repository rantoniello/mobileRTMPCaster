//
//  MainViewController.h
//
//  
//

#import <UIKit/UIKit.h>

#import <GLKit/GLKit.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <QuartzCore/QuartzCore.h>
#import <CoreImage/CoreImage.h>
#import <ImageIO/ImageIO.h>

#import "WebViewInterface.h"

extern "C" {
#import "MainViewController_ciface.h" // C public inteface for this class
}

@interface MainViewController : UIViewController <WebViewInterface>
    - (void*) getMainActivity;
    @property (weak, nonatomic) IBOutlet UIWebView *webView;
@end
