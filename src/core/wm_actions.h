#ifndef WM_ACTIONS_H
#define WM_ACTIONS_H

#include "wm_layout.h"
#include "wm_runtime.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

typedef enum {
  WM_ACTION_NONE = 0,

  // buffer actions
  WM_ACTION_SWITCH_BUFFER, // switch to buffer N
  WM_ACTION_MOVE_BUFFER,   // move focused app to buffer N, then switch

  // snapping - float a window to a position on the screen
  WM_ACTION_SNAP_LEFT,
  WM_ACTION_SNAP_RIGHT,
  WM_ACTION_SNAP_TOP,
  WM_ACTION_SNAP_BOTTOM,
  WM_ACTION_SNAP_MAXIMIZE,
  WM_ACTION_SNAP_CENTER,
  WM_ACTION_SNAP_TOP_LEFT,
  WM_ACTION_SNAP_TOP_RIGHT,
  WM_ACTION_SNAP_BOTTOM_LEFT,
  WM_ACTION_SNAP_BOTTOM_RIGHT,

  WM_ACTION_RETILE, // re-run dwindle layout

  WM_ACTION_TOGGLE_PASSTHROUGH, // disable all hotkeys
  WM_ACTION_TOGGLE_FLOATING,    // toggle focused app tiled/floating

  WM_ACTION_LAUNCH_BUNDLE, // launch app by bundle ID
} WMActionType;

// an action to process
typedef struct {
  WMActionType type;
  int target_buffer;           // for buffer actions
  pid_t target_pid;            // for app-specific actions
  char bundle_identifier[128]; // for LAUNCH_BUNDLE
} WMAction;

// effects to apply after an action is processed
typedef struct {
  // visibility changes
  pid_t to_hide[WM_MAX_APPS]; // pids to hide
  int16_t to_hide_count;      // number of pids to hide
  pid_t to_show[WM_MAX_APPS]; // pids to show
  int16_t to_show_count;      // number of pids to show

  pid_t to_raise[WM_MAX_APPS]; // pids to raise
  int16_t to_raise_count;      // number of pids to raise

  // layout
  bool needs_layout; // layout needs to be applied
  int layout_buffer; // buffer to layout

  // frame changes
  WMFrameChange frame_changes[WM_MAX_APPS]; // frame changes to apply
  int16_t frame_change_count;               // number of frame changes

  // app launch
  char launch_bundle[128]; // empty = no launch
} WMEffects;

// initialize effects to empty.
void wm_effects_init(WMEffects *effects);

// add a pid to the hide list
void wm_effects_add_hide(WMEffects *effects, pid_t pid);

// add a pid to the show list
void wm_effects_add_show(WMEffects *effects, pid_t pid);

// add a pid to the raise list
void wm_effects_add_raise(WMEffects *effects, pid_t pid);

// add a frame change
void wm_effects_add_frame(WMEffects *effects, pid_t pid, WMRect frame);

// process an action and compute effects
bool wm_action_process(struct WMState *state, const WMAction *action,
                       WMEffects *effects);

// switch to a different buffer
bool wm_action_switch_buffer(struct WMState *state, int target_buffer,
                             WMEffects *effects);

#endif
