#import "AppDelegate.h"
#import "mac_event_tap.h"
#import "mac_status_bar.h"
#import "wm_actions.h"
#include <AppKit/AppKit.h>

@implementation AppDelegate

static WMConfig g_config;

// handle actions from the event tap
static void handle_action(int action_type, int argument,
                          const char *bundle_identifier) {
  (void)bundle_identifier;
  NSLog(@"[dwin] Action received: type=%d, arg=%d", action_type, argument);

  switch (action_type) {
  case WM_ACTION_SWITCH_BUFFER:
    NSLog(@"[dwin] Switching buffer to %d", argument);
    break;
  case WM_ACTION_MOVE_BUFFER:
    NSLog(@"[dwin] Moving buffer to %d", argument);
    break;
  default:
    NSLog(@"[dwin] Unknown action type: %d", action_type);
    break;
  }
}

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

  // initialize config
  wm_config_init(&g_config);

  // start event tap for global hotkeys
  if (!mac_event_tap_start(&g_config, handle_action)) {
    NSLog(@"[dwin] Failed to start event tap. Exiting...");
    [self showAccessibilityAlert];
    return;
  }

  NSLog(@"[dwin] Ready.");
}

// show accessibility permission alert
- (void)showAccessibilityAlert {
  NSAlert *alert = [[NSAlert alloc] init];
  alert.messageText = @"Accessibility Permission Required";
  alert.informativeText = @"dwin needs Accessibility access to capture global "
                          @"hotkeys. \n\nGo to System Settings -> Privacy & "
                          @"Security -> Accessibility and enable dwin. ";
  alert.alertStyle = NSAlertStyleWarning;
  [alert addButtonWithTitle:@"Open System Settings"];
  [alert addButtonWithTitle:@"Quit"];

  NSModalResponse response = [alert runModal];
  if (response == NSAlertFirstButtonReturn) {
    // open system settings
    NSURL *url =
        [NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference."
                             @"security?Privacy_Accessibility"];
    [[NSWorkspace sharedWorkspace] openURL:url];
  }

  [NSApp terminate:nil];
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
