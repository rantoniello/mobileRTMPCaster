//
//  WebViewDelegate.h
//
//  
//

#import <Foundation/Foundation.h>
#import "WebViewInterface.h"

extern "C" {
    #import "WebViewDelegate_ciface.h" // C public inteface for this class
}

@interface WebViewDelegate : NSObject <UIWebViewDelegate>
	@property UIWebView* webView;

	- (id)initWithWebView:(UIWebView*) webView withWebViewInterface:(id<WebViewInterface>) webViewInterface;
	- (void) loadPage:(NSString*) pageName fromFolder:(NSString*) folderName;
	-(void) createError:(NSError**) error withCode:(int) code withMessage:(NSString*) msg;
	-(void) callJSFunction:(NSString *) name withArgs:(NSDictionary *) args;

@end

