//
//  MainViewController.m
//
//  
//

#import "AppDelegate.h"
#import "MainViewController.h"
#import "WebViewDelegate.h"
#import "AVViewDelegate.h"
extern "C" {
    #include "log.h"
}
#include "MainActivity.h"
#include "RtmpClientCtrl.h"

@interface MainViewController ()
{
@private
    MainActivity* m_MainAcivity;
    CALayer *m_customPreviewLayer;
    BOOL hideBar;
}
	@property WebViewDelegate *webViewDelegate;
@end

@implementation MainViewController

void performOnErrorOnMainThread(void *self, int errorCode)
{
    [(__bridge id)(self) performSelectorOnMainThread:@selector(performOnErrorOnMainThread:) withObject:[NSNumber numberWithInt:errorCode] waitUntilDone:YES];
}

- (void) performOnErrorOnMainThread: (NSNumber*)errorCode
{
    LOGV("Executng 'performOnErrorOnMainThread(%d)'... \n", [errorCode intValue]);

    /* Set error code */
    m_MainAcivity->setLastErrorCode((_retcodes)[errorCode intValue]);
    
    /* Stop/disconnect RTMP server */
    m_MainAcivity->getRtmpClientCtrl()->stop();

    /* Auto-reconnect if apply */
    if(m_MainAcivity->getRtmpClientCtrl()->autoReconnectGet()>= 0)
    {
        m_MainAcivity->getRtmpClientCtrl()->start();
    }
}

void performOnPlayerInputResolutionChangedOnMainThread(void *self, const char *args[])
{
    NSMutableDictionary *ns_args= [[NSMutableDictionary alloc] init];
    for(int i= 0; args[i]!= NULL && args[i+ 1]!= NULL; i+= 2)
    {
        [ns_args setObject:[NSString stringWithCString:args[i] encoding:[NSString defaultCStringEncoding]]
                    forKey:[NSString stringWithCString:args[i+ 1] encoding:[NSString defaultCStringEncoding]]];
    }

    [(__bridge id)(self) performSelectorOnMainThread:@selector(performOnPlayerInputResolutionChangedOnMainThread:) withObject:(NSMutableDictionary*)ns_args waitUntilDone:YES];
}

- (void) performOnPlayerInputResolutionChangedOnMainThread: (NSMutableDictionary*)args
{
    NSString *ns_name= [NSString stringWithCString:"onPlayerInputResolutionChanged" encoding:[NSString defaultCStringEncoding]];
    [self.webViewDelegate callJSFunction: ns_name withArgs:args];
}

const char* getBackgroundImagePath()
{
    NSString* filePath = [[NSBundle mainBundle] pathForResource:@"NoVideoAvailable" ofType:@"yuv" inDirectory:@"www"];
    return [filePath UTF8String];
}

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        // Custom initialization
    }
    return self;
}

- (void*) getMainActivity
{
    return (void*)m_MainAcivity;
}

- (void*) getCustomPreviewLayer
{
    return (__bridge void*)m_customPreviewLayer;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    // Set network progress activity indicator ON
    [UIApplication sharedApplication].networkActivityIndicatorVisible= YES;

    // Web View relaated
	self.webViewDelegate = [[WebViewDelegate alloc] initWithWebView:self.webView withWebViewInterface:self];
	self.webView.scrollView.scrollEnabled = true;
    //self.webView.scalesPageToFit= true;
	[self.webViewDelegate loadPage:@"index_debug.html" fromFolder:@"www"];

    /* Init App. */
    m_MainAcivity= new MainActivity((__bridge void*)(self), (__bridge void*)(self.webViewDelegate));
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
}

- (id) processFunctionFromJS:(NSString *) name withArgs:(NSArray*) args error:(NSError **) error
{
    //if ([name compare:@"connect" options:NSCaseInsensitiveSearch] == NSOrderedSame)
    //{
    //    return result;
    //}

    unsigned nargs = [args count];
    char **array = (char **)malloc((nargs + 1) * sizeof(char*));
    for (unsigned i = 0; i < nargs; i++)
    {
        NSString *obji = [NSString stringWithFormat:@"%@", [args objectAtIndex:i]]; //description
        array[i] = strdup([obji UTF8String]);
    }
    array[nargs] = NULL;

    string answer= m_MainAcivity->callApi([name UTF8String], array, nargs);
    if (array != NULL)
    {
        for (unsigned index = 0; array[index] != NULL; index++)
        {
            free(array[index]);
        }
        free(array);
    }

    if(answer!= "null") {
        LOGV("Answer: '%s'\n", answer.c_str());
        NSArray *listElements = @[[NSString stringWithCString:answer.c_str() encoding:[NSString defaultCStringEncoding]]];
        NSData *jsonData = [NSJSONSerialization dataWithJSONObject:listElements options:0 error:nil];
        NSString *result = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
        return result;
    }
    else
    {
        LOGV("Answer: NULL -void/no answer-\n");
        return nil;
    }
}

void toggleFullscreen(void *self, int hideBar)
{
    [(__bridge id)(self) toggleFullscreen: hideBar? YES: NO];
}

- (void) toggleFullscreen: (BOOL)hideBar_arg
{
    LOGV("Executng 'toggleFullscreen(%d)'... \n", hideBar_arg? 1 : 0);

    /* Force 'prefersStatusBarHidden()' execution */
    self->hideBar= hideBar_arg;
    [self setNeedsStatusBarAppearanceUpdate];
}

- (BOOL)prefersStatusBarHidden {
    static bool first_enter= true;

    if(first_enter) {
        first_enter= false;
        return (self->hideBar= YES);
    }
    return self->hideBar;
}

void toggleActionBar(void *self, int hideBar)
{
    [(__bridge id)(self) toggleActionBar: hideBar? YES: NO];
}

- (void) toggleActionBar: (BOOL)hideBar_arg
{
    LOGV("Executng 'toggleActionBar(%d)'... \n", hideBar_arg? 1 : 0);
    [[self navigationController] setNavigationBarHidden:hideBar_arg animated:YES];
}

@end
