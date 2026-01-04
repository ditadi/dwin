#ifndef APP_DELEGATE_H
#define APP_DELEGATE_H

#import <Cocoa/Cocoa.h>

@class MacStatusBar;

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, strong) MacStatusBar *statusBar;

@end

#endif
