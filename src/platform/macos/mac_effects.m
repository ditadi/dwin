#include "mac_effects.h"
#include "wm_layout.h"
#include "wm_runtime.h"
#include "wm_state.h"
#include <AppKit/AppKit.h>

// flag to prevent race conditions during buffer switch
bool g_is_switching_buffer = false;

// cleanup timer to hide apps
static dispatch_source_t g_cleanup_timer = NULL;
static WMState *g_cleanup_state = NULL;

#pragma mark - private functions

// get the app for a given pid
static NSRunningApplication *app_for_pid(pid_t pid) {
  return [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
}

// check if an app should be raised
static bool should_raise_app(NSRunningApplication *app, pid_t focused_pid) {
  if (!app)
    return false;

  pid_t pid = app.processIdentifier;
  return (pid == focused_pid) || app.isHidden;
}

// find the app to focus in a buffer
static pid_t find_app_to_focus(WMState *state, int buffer_index,
                               pid_t *buffer_pids, int buffer_count) {
  if (!state || buffer_index < 0 || buffer_index >= WM_MAX_BUFFERS ||
      !buffer_pids || buffer_count <= 0)
    return 0;

  WMBuffer *buffer = &state->buffers[buffer_index];

  // try last focused app (if in buffer)
  if (buffer->last_focused_pid > 0) {
    for (int i = 0; i < buffer_count; i++) {
      if (buffer_pids[i] == buffer->last_focused_pid) {
        return buffer->last_focused_pid;
      }
    }
  }

  // fallback to first app in buffer
  if (buffer_count > 0)
    return buffer_pids[0];

  // empty buffer -> no focus target
  return 0;
}

// reorder apps so focused app is LAST
static int reorder_apps_for_raise(pid_t *input_pids, int count,
                                  pid_t focused_pid, pid_t *output_pids) {
  int output_count = 0;

  // add all non-focused apps first
  for (int i = 0; i < count; i++) {
    if (input_pids[i] != focused_pid) {
      output_pids[output_count++] = input_pids[i];
    }
  }

  // add focused app last
  if (focused_pid > 0)
    output_pids[output_count++] = focused_pid;

  return output_count;
}

#pragma mark - atomic operations

// raise an app by pid, unhiding and unminimizing if needed
static void raise_app(pid_t pid) {
  NSRunningApplication *nsapp = app_for_pid(pid);

  // unhide app if needed
  if (nsapp) {
    if (nsapp.isHidden) {
      [nsapp unhide];
    }
  }

  AXUIElementRef ax_app = AXUIElementCreateApplication(pid);
  if (ax_app == NULL) {
    return;
  }

  // get main window
  AXUIElementRef main_window = NULL;
  AXError err = AXUIElementCopyAttributeValue(ax_app, kAXMainWindowAttribute,
                                              (CFTypeRef *)&main_window);

  if (err == kAXErrorSuccess && main_window != NULL) {
    // check if window is minimized and unminimize it
    CFBooleanRef minimized = NULL;
    AXUIElementCopyAttributeValue(main_window, kAXMinimizedAttribute,
                                  (CFTypeRef *)&minimized);
    if (minimized == kCFBooleanTrue) {
      AXUIElementSetAttributeValue(main_window, kAXMinimizedAttribute,
                                   kCFBooleanFalse);
      if (minimized)
        CFRelease(minimized);
    }

    // raise the window
    AXUIElementPerformAction(main_window, kAXRaiseAction);
    CFRelease(main_window);
  }

  CFRelease(ax_app);
}

// activate an app by pid
static void activate_app(pid_t pid) {
  NSRunningApplication *app = app_for_pid(pid);
  if (app) {
    [app activateWithOptions:NSApplicationActivateAllWindows];
  }
}

// hide an app by pid
static void hide_app(pid_t pid) {
  NSRunningApplication *app = app_for_pid(pid);
  if (app) {
    [app hide];
  }
}

#pragma mark - buffer switch phases
static pid_t show_buffer_apps(WMState *state, int buffer_index,
                              pid_t *buffer_pids, int buffer_count) {

  if (buffer_count == 0) {
    // empty buffer -> don't activate anything (leave it empty)
    return 0;
  }

  // find app to focus
  pid_t focused_pid =
      find_app_to_focus(state, buffer_index, buffer_pids, buffer_count);

  // reorder apps so focused app is LAST
  pid_t ordered_pids[WM_MAX_APPS];
  int ordered_count = reorder_apps_for_raise(buffer_pids, buffer_count,
                                             focused_pid, ordered_pids);

  // raise apps that need it
  for (int i = 0; i < ordered_count; i++) {
    pid_t pid = ordered_pids[i];
    NSRunningApplication *app = app_for_pid(pid);
    if (should_raise_app(app, focused_pid)) {
      raise_app(pid);
    }
  }

  // activate focused app last
  if (focused_pid > 0) {
    activate_app(focused_pid);
  }

  return focused_pid;
}

static void hide_buffer_apps(WMState *state, int old_buffer_index,
                             int new_buffer_index) {
  // startup state, no old buffer
  if (old_buffer_index < 0 || old_buffer_index >= WM_MAX_BUFFERS)
    return;

  pid_t old_pids[WM_MAX_APPS];
  int old_count =
      wm_state_get_buffer_pids(state, old_buffer_index, old_pids, WM_MAX_APPS);

  for (int i = 0; i < old_count; i++) {
    pid_t pid = old_pids[i];
    const WMApp *app = wm_state_find_app(state, pid);

    // hide if app is no longer in the new buffer
    if (!app || app->buffer_index != new_buffer_index) {
      hide_app(pid);
    }
  }
}

// hide apps not in current buffer
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
  if (g_cleanup_state)
    hide_apps_not_in_current_buffer(g_cleanup_state);
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

#pragma mark - public api

void mac_switch_buffer(WMState *state, int new_buffer_index) {
  // validate input
  if (new_buffer_index < 0 || new_buffer_index >= WM_MAX_BUFFERS)
    return;

  int old_buffer_index = state->active_buffer;
  if (old_buffer_index == new_buffer_index)
    return; // no-op if same buffer

  // set flag to block focus tracking
  g_is_switching_buffer = true;

  // update state first
  state->active_buffer = new_buffer_index;

  // get pids from new buffer
  pid_t new_pids[WM_MAX_APPS];
  int new_count =
      wm_state_get_buffer_pids(state, new_buffer_index, new_pids, WM_MAX_APPS);

  // show new buffer apps (raise and activate)
  show_buffer_apps(state, new_buffer_index, new_pids, new_count);

  // hide old buffer aps
  hide_buffer_apps(state, old_buffer_index, new_buffer_index);

  // schedule cleanup
  schedule_cleanup(state);

  // remember which app should have focus
  pid_t final_focused_pid = 0;
  if (new_count > 0) {
    WMBuffer *new_buffer = &state->buffers[new_buffer_index];
    if (new_buffer->last_focused_pid > 0) {
      for (int i = 0; i < new_count; i++) {
        if (new_pids[i] == new_buffer->last_focused_pid) {
          final_focused_pid = new_buffer->last_focused_pid;
          break;
        }
      }
    }
    if (final_focused_pid == 0 && new_count > 0) {
      final_focused_pid = new_pids[0];
    }
  }

  // clear flag after delay, then re-activate focused app
  int clear_delay_ms = (new_count > 0) ? (new_count * 50 + 50) : 50;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW,
                    (int64_t)(clear_delay_ms * NSEC_PER_MSEC)),
      dispatch_get_main_queue(), ^{
        g_is_switching_buffer = false;

        // re-activate focused app to override any finder activation
        if (final_focused_pid > 0) {
          NSRunningApplication *refocus_app = [NSRunningApplication
              runningApplicationWithProcessIdentifier:final_focused_pid];
          if (refocus_app) {
            [refocus_app activateWithOptions:NSApplicationActivateAllWindows];
          }
        }
      });
}

WMRect mac_effects_get_visible_screen_rect(void) {
  NSScreen *screen = [NSScreen mainScreen];
  NSRect frame = [screen frame];

  return (WMRect){.x = frame.origin.x,
                  .y = frame.origin.y,
                  .width = frame.size.width,
                  .height = frame.size.height};
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
