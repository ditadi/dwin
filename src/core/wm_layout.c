#include "wm_layout.h"

int wm_layout_compute_dwindle(const struct WMState *state,
                              const struct WMConfig *config,
                              int8_t buffer_index, WMRect screen,
                              WMFrameChange *out_frames, int max_frame) {
  // @TODO: implement
  (void)state;
  (void)config;
  (void)buffer_index;
  (void)screen;
  (void)out_frames;
  (void)max_frame;
  return 0;
}

WMRect wm_layout_compute_snap(int snap_action, WMRect screen,
                              const struct WMConfig *config) {
  // @TODO: implement
  (void)snap_action;
  (void)screen;
  (void)config;
  return screen;
}

WMRect wm_layout_apply_gaps(WMRect rect, int top, int right, int bottom,
                            int left) {
  return (WMRect){.x = rect.x + left,
                  .y = rect.y + top,
                  .width = rect.width - left - right,
                  .height = rect.height - top - bottom};
}

bool wm_layout_rect_valid(WMRect rect) {
  return rect.width > 0 && rect.height > 0;
}
