#include "wm_actions.h"
#include "wm_state.h"
#include <stdint.h>
#include <string.h>

static bool move_app_state(WMState *state, pid_t pid, int target_buffer,
                           int *out_old_buffer);

void wm_effects_init(WMEffects *effects) {
  memset(effects, 0, sizeof(WMEffects));
  effects->capture_z_order_buffer = -1;
}

void wm_effects_add_hide(WMEffects *effects, pid_t pid) {
  if (effects->to_hide_count < WM_MAX_APPS) {
    effects->to_hide[effects->to_hide_count++] = pid;
  }
}

void wm_effects_add_show(WMEffects *effects, pid_t pid) {
  if (effects->to_show_count < WM_MAX_APPS) {
    effects->to_show[effects->to_show_count++] = pid;
  }
}

void wm_effects_add_raise(WMEffects *effects, pid_t pid) {
  if (effects->to_raise_count < WM_MAX_APPS) {
    effects->to_raise[effects->to_raise_count++] = pid;
  }
}

static void wm_effects_add_raise_unique(WMEffects *effects, pid_t pid) {
  for (int i = 0; i < effects->to_raise_count; i++) {
    if (effects->to_raise[i] == pid)
      return;
  }
  wm_effects_add_raise(effects, pid);
}

void wm_effects_add_frame(WMEffects *effects, pid_t pid, WMRect frame) {
  if (effects->frame_change_count < WM_MAX_APPS) {
    WMFrameChange *change =
        &effects->frame_changes[effects->frame_change_count++];
    change->pid = pid;
    change->frame = frame;
  }
}

bool wm_action_switch_buffer(WMState *state, int target_buffer,
                             WMEffects *effects) {
  if (state == NULL || effects == NULL)
    return false;

  wm_effects_init(effects);

  // validate
  if (target_buffer < 0 || target_buffer >= WM_MAX_BUFFERS)
    return false;

  // no-op if same buffer
  if (target_buffer == state->active_buffer)
    return false;

  int old_buffer = state->active_buffer;

  // capture z-order from old buffer to preserve window stacking order
  effects->capture_z_order_buffer = (int8_t)old_buffer;

  // collect PIDs to show (all apps in new buffer)
  pid_t new_pids[WM_MAX_APPS];
  int new_count =
      wm_state_get_buffer_pids(state, target_buffer, new_pids, WM_MAX_APPS);

  for (int i = 0; i < new_count; i++) {
    wm_effects_add_show(effects, new_pids[i]);
  }

  // collect PIDs to hide (all apps in old buffer)
  pid_t old_pids[WM_MAX_APPS];
  int old_count =
      wm_state_get_buffer_pids(state, old_buffer, old_pids, WM_MAX_APPS);

  for (int i = 0; i < old_count; i++) {
    wm_effects_add_hide(effects, old_pids[i]);
  }

  // determine app to raise (use last_focused_pid or first app)
  WMBuffer *new_buf = &state->buffers[target_buffer];

  if (new_buf->last_focused_pid > 0) {
    // use last focused app
    wm_effects_add_raise(effects, new_buf->last_focused_pid);
  } else if (new_count > 0) {
    // fallback use first app
    wm_effects_add_raise(effects, new_pids[0]);
  }

  // mark layout needed for new buffer
  effects->needs_layout = true;
  effects->layout_buffer = target_buffer;

  // update active buffer
  state->active_buffer = target_buffer;
  return true;
}

bool wm_action_process(WMState *state, const WMAction *action,
                       WMEffects *effects) {
  // validate
  if (state == NULL || action == NULL || effects == NULL)
    return false;

  switch (action->type) {
  case WM_ACTION_SWITCH_BUFFER:
    return wm_action_switch_buffer(state, action->target_buffer, effects);
  case WM_ACTION_MOVE_BUFFER: {
    // move app to target buffer, then switch to it
    if (!move_app_state(state, action->target_pid, action->target_buffer,
                        NULL)) {
      wm_effects_init(effects);
      return false;
    }

    int target_buffer = action->target_buffer;

    // if moving to active buffer, just relayout
    if (target_buffer == state->active_buffer) {
      wm_effects_init(effects);
      wm_effects_add_show(effects, action->target_pid);
      wm_effects_add_raise_unique(effects, action->target_pid);
      effects->needs_layout = true;
      effects->layout_buffer = target_buffer;
      return true;
    }

    // switch to target buffer
    bool ok = wm_action_switch_buffer(state, target_buffer, effects);
    if (ok)
      wm_effects_add_raise_unique(effects, action->target_pid);

    return ok;
  }
  case WM_ACTION_TOGGLE_PASSTHROUGH:
    state->is_passthrough_mode = !state->is_passthrough_mode;
    wm_effects_init(effects);
    return true;
  default:
    wm_effects_init(effects);
    return false;
  }
}

static bool move_app_state(WMState *state, pid_t pid, int target_buffer,
                           int *out_old_buffer) {
  // validate target buffer
  if (target_buffer < 0 || target_buffer >= WM_MAX_BUFFERS)
    return false;

  // validate app
  const WMApp *app = wm_state_find_app(state, pid);
  if (app == NULL)
    return false;

  int current_buffer = app->buffer_index;
  if (current_buffer == target_buffer)
    return false;

  // move app to target buffer
  wm_state_assign_to_buffer(state, pid, target_buffer);

  // return old buffer if requested
  if (out_old_buffer)
    *out_old_buffer = current_buffer;

  return true;
}
