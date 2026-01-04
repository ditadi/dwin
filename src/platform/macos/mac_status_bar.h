#ifndef MAC_STATUS_BAR_H
#define MAC_STATUS_BAR_H

#import <Cocoa/Cocoa.h>

// Status bar UI component (platform layer)
@interface MacStatusBar : NSObject

@property(nonatomic, strong) NSStatusItem *statusItem;
@property(nonatomic, strong) NSMenu *menu;

- (void)setup;
- (void)refresh;

@end

#endif
