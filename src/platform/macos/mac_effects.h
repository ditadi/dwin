#ifndef MAC_EFFECTS_H
#define MAC_EFFECTS_H

#include "wm_actions.h"
#include "wm_layout.h"
#include "wm_state.h"

// switch to a new buffer
void mac_switch_buffer(WMState *state, int new_buffer_index);

// get currently focused PID
pid_t mac_effects_get_focused_pid(void);

// get the visible screen rect
WMRect mac_effects_get_visible_screen_rect(void);

// apply a frame
bool mac_effects_apply_frame(pid_t pid, WMRect frame);

#endif
