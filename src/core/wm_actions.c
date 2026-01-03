#include "wm_actions.h"
#include <string.h>

void wm_effects_init(WMEffects *effects) {
  memset(effects, 0, sizeof(WMEffects));
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

void wm_effects_add_frame(WMEffects *effects, pid_t pid, WMRect frame) {
  if (effects->frame_change_count < WM_MAX_APPS) {
    WMFrameChange *change =
        &effects->frame_changes[effects->frame_change_count++];
    change->pid = pid;
    change->frame = frame;
  }
}
