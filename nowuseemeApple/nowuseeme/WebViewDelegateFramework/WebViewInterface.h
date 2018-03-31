//
//  WebViewInterface.h
//
//  
//

#import <Foundation/Foundation.h>

@protocol WebViewInterface <NSObject>
- (id) processFunctionFromJS:(NSString *) name withArgs:(NSArray*) args error:(NSError **) error;
@end
