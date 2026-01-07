#import "AppDelegate.h"
#import "mac_effects.h"
#import "mac_event_tap.h"
#import "mac_status_bar.h"
#import "wm_actions.h"
#import "wm_layout.h"
#include "wm_state.h"
#include <AppKit/AppKit.h>

@implementation AppDelegate

static WMConfig g_config;
static WMState g_state;

// blacklisted apps that we shouldn't manage
static const char *BLACKLIST[] = {
    "com.apple.finder",
    "com.apple.dock",
    "com.apple.Spotlight",
    "com.apple.loginwindow",
    "com.apple.SecurityAgent",
    "com.apple.systemuiserver",
    "com.apple.notificationcenterui",
    "com.apple.controlcenter",
    "com.apple.WindowManager",
    "com.dwin.app", // ourselves
};

static bool is_blacklisted(const char *bundle_id) {
  if (bundle_id == NULL || strlen(bundle_id) == 0)
    return true;
  for (size_t i = 0; i < sizeof(BLACKLIST) / sizeof(BLACKLIST[0]); i++) {
    if (strcasecmp(bundle_id, BLACKLIST[i]) == 0)
      return true;
  }
  return false;
}

static bool is_app_manageable(NSRunningApplication *app) {
  if (!app)
    return false;

  // check activation policy
  if (app.activationPolicy != NSApplicationActivationPolicyRegular)
    return false;

  // check bundle identifier
  if (!app.bundleIdentifier)
    return false;

  // check blacklist
  if (is_blacklisted([app.bundleIdentifier UTF8String]))
    return false;

  return true;
}

static void apply_layout_to_active_buffer(void) {
  WMRect screen = mac_effects_get_visible_screen_rect();
  WMFrameChange frame_changes[WM_MAX_APPS];
  int count =
      wm_layout_compute_dwindle(&g_state, g_state.active_buffer, &g_config,
                                screen, frame_changes, WM_MAX_APPS);

  for (int i = 0; i < count; i++) {
    mac_effects_apply_frame(frame_changes[i].pid, frame_changes[i].frame);
  }
}

// handle actions from the event tap
static void handle_action(int action_type, int argument,
                          const char *bundle_identifier) {
  (void)bundle_identifier; // app launch not implemented yet

  WMActionType type = (WMActionType)action_type;

  switch (type) {
  case WM_ACTION_SWITCH_BUFFER: {
    mac_switch_buffer(&g_state, argument);
    apply_layout_to_active_buffer();
    break;
  };
  case WM_ACTION_MOVE_BUFFER: {
    // don't allow moving to current buffer
    if (argument == g_state.active_buffer)
      return;

    pid_t pid = mac_effects_get_focused_pid();
    if (pid <= 0)
      return;

    // move app to target buffer
    wm_state_assign_to_buffer(&g_state, pid, argument);

    // set the moved app as last_focused so it stays on top when we switch
    wm_state_set_focused(&g_state, pid);

    mac_switch_buffer(&g_state, argument);
    apply_layout_to_active_buffer();
    break;
  }
  case WM_ACTION_SNAP_LEFT:
  case WM_ACTION_SNAP_RIGHT:
  case WM_ACTION_SNAP_TOP:
  case WM_ACTION_SNAP_BOTTOM:
  case WM_ACTION_SNAP_MAXIMIZE:
  case WM_ACTION_SNAP_CENTER:
  case WM_ACTION_SNAP_TOP_LEFT:
  case WM_ACTION_SNAP_TOP_RIGHT:
  case WM_ACTION_SNAP_BOTTOM_LEFT:
  case WM_ACTION_SNAP_BOTTOM_RIGHT: {
    pid_t pid = mac_effects_get_focused_pid();
    if (pid <= 0)
      return;

    // mark as floating so dwindle ignores it
    wm_state_set_floating(&g_state, pid, true);

    // apply snap loading
    WMRect screen = mac_effects_get_visible_screen_rect();
    WMRect frame = wm_layout_compute_snap(type, screen, &g_config);
    mac_effects_apply_frame(pid, frame);

    // re-apply dwindle to remaining non-floating apps
    apply_layout_to_active_buffer();
    break;
  }

  case WM_ACTION_RETILE: {
    pid_t pid = mac_effects_get_focused_pid();
    if (pid <= 0)
      return;

    // return to dwindle layout
    wm_state_set_floating(&g_state, pid, false);
    apply_layout_to_active_buffer();
    break;
  }

  case WM_ACTION_TOGGLE_PASSTHROUGH:
    g_state.is_passthrough_mode = !g_state.is_passthrough_mode;
    break;

  default:
    break;
  }
}

// register currently running GUI apps into state
static void register_running_apps(void) {
  NSArray<NSRunningApplication *> *runningApps =
      [[NSWorkspace sharedWorkspace] runningApplications];

  for (NSRunningApplication *app in runningApps) {
    if (!is_app_manageable(app))
      continue;

    pid_t pid = app.processIdentifier;
    const char *bundle_id = [app.bundleIdentifier UTF8String];

    int idx = wm_state_register_app(&g_state, pid, bundle_id);
    if (idx >= 0) {
      wm_state_assign_to_buffer(&g_state, pid, 0);
    }
  }
}

// called by macos when app is ready
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  (void)notification;

  // check for duplicate instance
  if ([self isDuplicateInstance]) {
    [NSApp terminate:nil];
    return;
  }

  // init state and config
  wm_state_init(&g_state);
  wm_config_init(&g_config);

  // setup menu status bar
  self.statusBar = [[MacStatusBar alloc] init];
  [self.statusBar setup];

  // register running apps
  register_running_apps();
  // switch to buffer 0 (this shows all apps in buffer 0)
  mac_switch_buffer(&g_state, 0);
  // apply layout to active buffer
  apply_layout_to_active_buffer();

  // capture app activation from external sources
  [[[NSWorkspace sharedWorkspace] notificationCenter]
      addObserver:self
         selector:@selector(handleAppActivated:)
             name:NSWorkspaceDidActivateApplicationNotification
           object:nil];

  [[[NSWorkspace sharedWorkspace] notificationCenter]
      addObserver:self
         selector:@selector(handleAppTerminated:)
             name:NSWorkspaceDidTerminateApplicationNotification
           object:nil];

  // start event tap for global hotkeys
  if (!mac_event_tap_start(&g_config, handle_action)) {
    [self showAccessibilityAlert];
    return;
  }
}

- (void)handleAppTerminated:(NSNotification *)notification {
  NSRunningApplication *application =
      notification.userInfo[NSWorkspaceApplicationKey];
  if (!application)
    return;

  pid_t pid = application.processIdentifier;

  // check if app was in active buffer and apply layout if it was
  const WMApp *app = wm_state_find_app(&g_state, pid);
  bool was_in_active_buffer =
      app != NULL && app->buffer_index == g_state.active_buffer;
  wm_state_unregister_app(&g_state, pid);

  if (was_in_active_buffer) {
    apply_layout_to_active_buffer();
  }
}

- (void)handleAppActivated:(NSNotification *)notification {
  extern bool g_is_switching_buffer;

  NSRunningApplication *application =
      notification.userInfo[NSWorkspaceApplicationKey];
  if (!application)
    return;

  pid_t pid = application.processIdentifier;

  // ignore during buffer switch
  if (g_is_switching_buffer)
    return;

  // debounce activation to prevent multiple activations
  static NSDate *lastActivation = nil;
  static pid_t lastActivatedPid = 0;
  NSDate *now = [NSDate date];
  if (lastActivation && [now timeIntervalSinceDate:lastActivation] < 0.2)
    return;

  // duplicate filter, skip if same pid as last event
  if (pid == lastActivatedPid && lastActivation &&
      [now timeIntervalSinceDate:lastActivation] < 1.0)
    return;

  lastActivatedPid = pid;

  // find which buffer this app belongs to
  const WMApp *app = wm_state_find_app(&g_state, pid);
  if (app != NULL) {
    int app_buffer = app->buffer_index;
    if (app_buffer < 0 || app_buffer >= WM_MAX_BUFFERS)
      return;

    // only update last focused if app is in the active buffer
    if (app_buffer == g_state.active_buffer) {
      wm_state_set_focused(&g_state, pid);
    } else {
      // switch to app's buffer if user activated it from another buffer
      lastActivation = now;
      mac_switch_buffer(&g_state, app_buffer);
    }
  } else {
    // new app, register and assign to active buffer
    if (is_app_manageable(application)) {
      int idx = wm_state_register_app(
          &g_state, pid, [application.bundleIdentifier UTF8String]);
      if (idx >= 0) {
        wm_state_assign_to_buffer(&g_state, pid, g_state.active_buffer);
        wm_state_set_focused(&g_state, pid);
        apply_layout_to_active_buffer();
      }
    } else {
      [self retryRegisterApp:pid name:application.localizedName attempt:1];
    }
  }
}

- (void)retryRegisterApp:(pid_t)pid name:(NSString *)name attempt:(int)attempt {
  int maxAttempts = 5;
  double delayMs = 100;

  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW,
                    (int64_t)(delayMs / 1000.0 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        NSRunningApplication *application =
            [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
        if (!application)
          return;

        if (is_app_manageable(application)) {
          int idx = wm_state_register_app(
              &g_state, pid, [application.bundleIdentifier UTF8String]);
          if (idx >= 0) {
            wm_state_assign_to_buffer(&g_state, pid, g_state.active_buffer);
            wm_state_set_focused(&g_state, pid);
            apply_layout_to_active_buffer();
          }
        } else if (attempt < maxAttempts) {
          [self retryRegisterApp:pid name:name attempt:attempt + 1];
        }
      });
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
