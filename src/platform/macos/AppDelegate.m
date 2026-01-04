#import "AppDelegate.h"
#import "mac_status_bar.h"

@implementation AppDelegate

// called by macos when app is ready
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  (void)notification;
  NSLog(@"[dwin] Starting...");

  if ([self isDuplicateInstance]) {
    NSLog(@"[dwin] Another instance is already running. Exiting...");
    [NSApp terminate:nil];
    return;
  }

  self.statusBar = [[MacStatusBar alloc] init];
  [self.statusBar setup];

  NSLog(@"[dwin] Ready.");
}

// check if another instance is already running
- (BOOL)isDuplicateInstance {
  NSString *bundleId =
      [[NSBundle mainBundle] bundleIdentifier]; // e.g com.spotify.client
  pid_t selfPID = [[NSProcessInfo processInfo] processIdentifier];

  NSArray<NSRunningApplication *> *runningApps =
      [[NSWorkspace sharedWorkspace] runningApplications];
  for (NSRunningApplication *app in runningApps) {
    if (app.processIdentifier != selfPID &&
        [app.bundleIdentifier isEqualToString:bundleId]) {
      return YES;
    }
  }
  return NO;
}

@end
