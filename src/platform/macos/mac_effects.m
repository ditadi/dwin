#include "mac_effects.h"
#include "wm_state.h"
#include <AppKit/AppKit.h>

bool g_is_switching_buffer = false;

// cleanup timer to hide apps
static dispatch_source_t g_cleanup_timer = NULL;
static WMState *g_cleanup_state = NULL;

static NSRunningApplication *app_for_pid(pid_t pid) {
  return [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
}

static void raise_app(pid_t pid) {
  AXUIElementRef ax_app = AXUIElementCreateApplication(pid);
  if (ax_app == NULL) {
    NSRunningApplication *app = app_for_pid(pid);
    [app unhide];
    return;
  }

  // get main window
  AXUIElementRef main_window = NULL;
  AXError err = AXUIElementCopyAttributeValue(ax_app, kAXMainWindowAttribute,
                                              (CFTypeRef *)&main_window);

  if (err == kAXErrorSuccess && main_window != NULL) {
    AXUIElementPerformAction(main_window, kAXRaiseAction);
    CFRelease(main_window);
  } else {
    // no main window, just unhide
    NSRunningApplication *app = app_for_pid(pid);
    [app unhide];
  }

  CFRelease(ax_app);
}

static void activate_app(pid_t pid) {
  NSRunningApplication *app = app_for_pid(pid);
  if (app) {
    [app activateWithOptions:NSApplicationActivateAllWindows];
  }
}

static void hide_app(pid_t pid) {
  NSRunningApplication *app = app_for_pid(pid);
  if (app) {
    [app hide];
  }
}

// hide all apps not in current buffer
static void hide_apps_not_in_current_buffer(WMState *state) {
  for (int i = 0; i < state->app_registry.app_count; i++) {
    WMApp *wm_app = &state->app_registry.apps[i];

    // skip apps in current buffer
    if (wm_app->buffer_index == state->active_buffer)
      continue;

    NSRunningApplication *app = app_for_pid(wm_app->pid);
    if (app && !app.isHidden) {
      [app hide];
    }
  }
}

static void cleanup_handler(void) {
  if (g_cleanup_state) {
    hide_apps_not_in_current_buffer(g_cleanup_state);
  }
  g_cleanup_timer = NULL;
}

static void schedule_cleanup(WMState *state) {
  g_cleanup_state = state;

  // cancel existing timer
  if (g_cleanup_timer != NULL) {
    dispatch_source_cancel(g_cleanup_timer);
    g_cleanup_timer = NULL;
  }

  // create and start new timer
  g_cleanup_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
                                           dispatch_get_main_queue());
  dispatch_source_set_timer(
      g_cleanup_timer,
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)),
      DISPATCH_TIME_FOREVER, 0);
  dispatch_source_set_event_handler(g_cleanup_timer, ^{
    cleanup_handler();
  });
  dispatch_resume(g_cleanup_timer);
}

void mac_switch_buffer(WMState *state, int new_buffer_index) {
  if (new_buffer_index < 0 || new_buffer_index >= WM_MAX_BUFFERS)
    return;

  int old_buffer_index = state->active_buffer;
  if (old_buffer_index == new_buffer_index)
    return;

  g_is_switching_buffer = true;

  state->active_buffer = new_buffer_index;

  WMBuffer *new_buffer = &state->buffers[new_buffer_index];

  // get pids from new buffer
  pid_t new_pids[WM_MAX_APPS];
  int new_count =
      wm_state_get_buffer_pids(state, new_buffer_index, new_pids, WM_MAX_APPS);

  // get pids from old buffer
  pid_t old_pids[WM_MAX_APPS];
  int old_count =
      wm_state_get_buffer_pids(state, old_buffer_index, old_pids, WM_MAX_APPS);

  // hide apps from old buffer
  for (int i = 0; i < old_count; i++) {
    pid_t pid = old_pids[i];
    const WMApp *app = wm_state_find_app(state, pid);
    if (!app || app->buffer_index != new_buffer_index) {
      hide_app(pid);
    }
  }

  pid_t focused_pid = 0;
  if (new_buffer->last_focused_pid > 0) {
    // check if last-focused app is in new buffer
    for (int i = 0; i < new_count; i++) {
      if (new_pids[i] == new_buffer->last_focused_pid) {
        focused_pid = new_buffer->last_focused_pid;
        break;
      }
    }
  }
  if (focused_pid == 0 && new_count > 0) {
    // fallback to first app
    focused_pid = new_pids[0];
  }

  // raise all apps in correct order (focused first, then background)
  pid_t ordered_pids[WM_MAX_APPS];
  int ordered_count = 0;

  // add focused app first
  if (focused_pid > 0) {
    ordered_pids[ordered_count++] = focused_pid;
  }

  // add non-focused apps after
  for (int i = 0; i < new_count; i++) {
    if (new_pids[i] != focused_pid) {
      ordered_pids[ordered_count++] = new_pids[i];
    }
  }

  // build list of pids that need raising
  pid_t raise_pids[WM_MAX_APPS];
  int raise_count = 0;
  int focused_raise_index = -1;

  for (int i = 0; i < ordered_count; i++) {
    pid_t pid = ordered_pids[i];
    NSRunningApplication *app = app_for_pid(pid);
    bool is_focused = (pid == focused_pid);
    bool is_hidden = (app && app.isHidden);

    // raise if focused or hidden
    if (is_focused || is_hidden) {
      if (is_focused)
        focused_raise_index = raise_count;
      raise_pids[raise_count++] = pid;
    }
  }

  // raise apps
  for (int i = 0; i < raise_count; i++) {
    pid_t pid = raise_pids[i];
    bool should_activate = (i == focused_raise_index);

    if (should_activate) {
      activate_app(pid);
    }
    raise_app(pid);
  }

  if (new_count == 0) {
    // activate finder if buffer is empty
    NSArray *apps = [NSRunningApplication
        runningApplicationsWithBundleIdentifier:@"com.apple.finder"];
    if (apps.count > 0) {
      [apps[0] activateWithOptions:0];
    }
  }

  // schedule cleanup
  schedule_cleanup(state);

  // clear flag
  int clear_delay_ms = (raise_count > 0) ? (raise_count * 50 + 50) : 50;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                               (int64_t)(clear_delay_ms * NSEC_PER_MSEC)),
                 dispatch_get_main_queue(), ^{
                   g_is_switching_buffer = false;
                 });
}

WMRect mac_effects_get_visible_screen_rect(void) {
  NSScreen *screen = [NSScreen mainScreen];
  NSRect frame = [screen frame];
  return (WMRect){
      .x = frame.origin.x,
      .y = frame.origin.y,
      .width = frame.size.width,
      .height = frame.size.height,
  };
}

bool mac_effects_apply_frame(pid_t pid, WMRect frame) {
  AXUIElementRef app = AXUIElementCreateApplication(pid);
  if (app == NULL)
    return false;

  // get main window or first window
  AXUIElementRef window = NULL;
  AXError err = AXUIElementCopyAttributeValue(app, kAXMainWindowAttribute,
                                              (CFTypeRef *)&window);
  if (err != kAXErrorSuccess || window == NULL) {
    // fallback to first window
    CFArrayRef windows = NULL;
    err = AXUIElementCopyAttributeValue(app, kAXWindowsAttribute,
                                        (CFTypeRef *)&windows);
    if (err != kAXErrorSuccess || windows == NULL ||
        CFArrayGetCount(windows) == 0) {
      if (windows)
        CFRelease(windows);
      if (window)
        CFRelease(window);
      CFRelease(app);
      return false;
    }
    window = (AXUIElementRef)CFRetain(CFArrayGetValueAtIndex(windows, 0));
    CFRelease(windows);
  }
  // set position
  CGPoint position = CGPointMake(frame.x, frame.y);
  AXValueRef position_value = AXValueCreate(kAXValueTypeCGPoint, &position);
  AXUIElementSetAttributeValue(window, kAXPositionAttribute, position_value);
  CFRelease(position_value);

  // set size
  CGSize size = CGSizeMake(frame.width, frame.height);
  AXValueRef size_value = AXValueCreate(kAXValueTypeCGSize, &size);
  AXUIElementSetAttributeValue(window, kAXSizeAttribute, size_value);
  CFRelease(size_value);
  CFRelease(window);
  CFRelease(app);
  return true;
}

pid_t mac_effects_get_focused_pid(void) {
  NSRunningApplication *app =
      [[NSWorkspace sharedWorkspace] frontmostApplication];
  return app ? app.processIdentifier : -1;
}
