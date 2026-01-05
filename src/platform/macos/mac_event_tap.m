#import "mac_event_tap.h"
#import "wm_config.h"
#import <ApplicationServices/ApplicationServices.h>

// global state for the event tap
static CFMachPortRef g_event_tap = NULL;
static CFRunLoopSourceRef g_run_loop_source = NULL;
static WMActionCallback g_action_callback = NULL;
static bool g_passthrough_mode = false;

static WMConfig *g_config = NULL;

// convert CGEventFlags to WMModifier
static int flags_to_modifiers(CGEventFlags flags) {
  int mods = WM_MOD_NONE;

  if (flags & kCGEventFlagMaskAlternate)
    mods |= WM_MOD_OPT;
  if (flags & kCGEventFlagMaskShift)
    mods |= WM_MOD_SHIFT;
  if (flags & kCGEventFlagMaskCommand)
    mods |= WM_MOD_CMD;
  if (flags & kCGEventFlagMaskControl)
    mods |= WM_MOD_CTRL;

  return mods;
}

static CGEventRef event_tap_callback(CGEventTapProxy proxy, CGEventType type,
                                     CGEventRef event, void *user_info) {
  (void)proxy;
  (void)user_info;

  // handle tap being disabled by system
  if (type == kCGEventTapDisabledByTimeout ||
      type == kCGEventTapDisabledByUserInput) {
    NSLog(@"[EventTap] Tap disabled by system. Re-enabling...");
    if (g_event_tap) {
      CGEventTapEnable(g_event_tap, true);
    }
    return event;
  }

  // only handle key down events
  if (type != kCGEventKeyDown) {
    return event;
  }

  // passthrough mode - all keys pass through
  CGKeyCode keycode =
      (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
  CGEventFlags flags = CGEventGetFlags(event);
  int modifiers = flags_to_modifiers(flags);
  WMAction action;

  // no binding found - pass through
  if (!wm_config_match_binding(g_config, modifiers, keycode, &action)) {
    return event;
  }

  // handle passthrough toggle
  if (action.type == WM_ACTION_TOGGLE_PASSTHROUGH) {
    g_passthrough_mode = !g_passthrough_mode;
    NSLog(@"[EventTap] Passthrough mode %s",
          g_passthrough_mode ? "enabled" : "disabled");
    return NULL;
  }

  // passthrough mode - all keys pass through
  if (g_passthrough_mode) {
    return event;
  }

  // dispatch action to main thread
  if (g_action_callback) {
    // copy action data to avoid race conditions
    WMActionType action_type = action.type;
    int action_argument = action.target_buffer;
    dispatch_async(dispatch_get_main_queue(), ^{
      g_action_callback(action_type, action_argument, action.bundle_identifier);
    });
  }

  return NULL;
}

// start the event tap for global hotkeys
bool mac_event_tap_start(WMConfig *config, WMActionCallback callback) {
  if (g_event_tap) {
    NSLog(@"[EventTap] Already running. Stopping...");
    return true;
  }

  g_config = config;
  g_action_callback = callback;

  // create event tap for key down events
  CGEventMask event_mask = CGEventMaskBit(kCGEventKeyDown);

  g_event_tap = CGEventTapCreate(kCGHIDEventTap, kCGHeadInsertEventTap,
                                 kCGEventTapOptionDefault, event_mask,
                                 event_tap_callback, NULL);
  if (!g_event_tap) {
    NSLog(@"[EventTap] Failed to create event tap. Error: %d", (int)errno);
    return false;
  }

  // create run loop source and add to main run loop
  g_run_loop_source =
      CFMachPortCreateRunLoopSource(kCFAllocatorDefault, g_event_tap, 0);
  CFRunLoopAddSource(CFRunLoopGetCurrent(), g_run_loop_source,
                     kCFRunLoopDefaultMode);

  // enable event tap
  CGEventTapEnable(g_event_tap, true);

  NSLog(@"[EventTap] Started successfully.");
  return true;
}

// stop and clean up the event tap
void mac_event_tap_stop(void) {
  if (g_run_loop_source) {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), g_run_loop_source,
                          kCFRunLoopDefaultMode);
    CFRelease(g_run_loop_source);
    g_run_loop_source = NULL;
  }

  if (g_event_tap) {
    CGEventTapEnable(g_event_tap, false);
    CFRelease(g_event_tap);
    g_event_tap = NULL;
  }

  g_action_callback = NULL;
  NSLog(@"[EventTap] Stopped successfully.");
}

// check if event tap is currently running
bool mac_event_tap_is_running(void) {
  return g_event_tap != NULL;
}

// enable/disable passthrough mode (all keys pass through)
void mac_event_tap_set_passthrough(bool enabled) {
  g_passthrough_mode = enabled;
  NSLog(@"[EventTap] Passthrough mode %s", enabled ? "enabled" : "disabled");
}

// get the current passthrough mode
bool mac_event_tap_get_passthrough(void) {
  return g_passthrough_mode;
}
