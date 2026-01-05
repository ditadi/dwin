#ifndef WM_STATE_H
#define WM_STATE_H

#include "wm_runtime.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

// runtime state of the dwin
typedef struct WMState {
  WMAppRegistry app_registry;       // registry of all apps
  WMBuffer buffers[WM_MAX_BUFFERS]; // array of buffers
  int active_buffer;                // index of the active buffer
  bool is_passthrough_mode;         // disable all hotkeys
} WMState;

// initialization of the dwin state
void wm_state_init(WMState *state);

// register an app
int wm_state_register_app(WMState *state, pid_t pid,
                          const char *bundle_identifier);

// unregister an app
void wm_state_unregister_app(WMState *state, pid_t pid);

// find app by pid
const WMApp *wm_state_find_app(const WMState *state, pid_t pid);

// find app index by pid, return -1 not found
int8_t wm_state_find_app_index(const WMState *state, pid_t pid);

// assign app to buffer, pass -1 to unassign
void wm_state_assign_to_buffer(WMState *state, pid_t pid, int buffer_index);

// get all pids in a buffer, returns count
int wm_state_get_buffer_pids(const WMState *state, int buffer_index,
                             pid_t *out_pids, int max_pids);

// update z-order for a buffer. Called whenw indow order changes
void wm_state_set_z_order(WMState *state, int buffer_index, const pid_t *pids,
                          int count);

// record that an app was focused in its buffer
void wm_state_set_focused(WMState *state, pid_t pid);

// debug
void wm_state_check_invariants(const WMState *state);

#endif
