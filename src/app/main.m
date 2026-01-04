#import "../platform/macos/AppDelegate.h"
#import <Cocoa/Cocoa.h>

// entrypoint fo macos app
int main(int argc, const char *argv[]) {
  (void)argc;
  (void)argv;

  @autoreleasepool {
    // create application instance
    NSApplication *application = [NSApplication sharedApplication];

    // accessory app (only shows in menu bar)
    [application setActivationPolicy:NSApplicationActivationPolicyAccessory];

    // delegate to handle lifecycle events
    AppDelegate *delegate = [[AppDelegate alloc] init];
    application.delegate = delegate;

    [application run];
  }
  return 0;
}
