#include "wm_state.h"
#include <string.h>

void wm_state_init(WMState *state) {
  memset(state, 0, sizeof(WMState));
  state->active_buffer = 0;
  state->is_passthrough_mode = false;
}

int wm_state_register_app(WMState *state, pid_t pid,
                          const char *bundle_identifier) {
  // @TODO: implement
  (void)state;
  (void)pid;
  (void)bundle_identifier;
  return -1;
}

void wm_state_unregister_app(WMState *state, pid_t pid) {
  // @TODO: implement
  (void)state;
  (void)pid;
}

const WMApp *wm_state_find_app(const WMState *state, pid_t pid) {
  // @TODO: implement
  (void)state;
  (void)pid;
  return NULL;
}

int8_t wm_state_find_app_index(const WMState *state, pid_t pid) {
  // @TODO: implement
  (void)state;
  (void)pid;
  return -1;
}

void wm_state_assign_to_buffer(WMState *state, pid_t pid, int buffer_index) {
  // @TODO: implement
  (void)state;
  (void)pid;
  (void)buffer_index;
}

int wm_state_get_buffer_pids(const WMState *state, int buffer_index,
                             pid_t *out_pids, int max_pids) {
  // @TODO: implement
  (void)state;
  (void)buffer_index;
  (void)out_pids;
  (void)max_pids;
  return 0;
}

void wm_state_set_z_order(WMState *state, int buffer_index, const pid_t *pids,
                          int count) {
  // @TODO: implement
  (void)state;
  (void)buffer_index;
  (void)pids;
  (void)count;
}

void wm_state_set_focused(WMState *state, pid_t pid) {
  // @TODO: implement
  (void)state;
  (void)pid;
}

void wm_state_check_invariants(const WMState *state) {
  // @TODO: implement
  (void)state;
}
