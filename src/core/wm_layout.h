#ifndef WM_LAYOUT_H
#define WM_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

// rectangle for frame calculations
typedef struct {
  double x;      // x coordinate
  double y;      // y coordinate
  double width;  // width of the frame
  double height; // height of the frame
} WMRect;

// frame changes to apply after an action is processed
typedef struct {
  pid_t pid;     // pid to change frame
  WMRect frame;  // new frame
} WMFrameChange; // array of frame changes

struct WMState;
struct WMConfig;

// compute dwindle layout for a buffer. Returns frame count.
int wm_layout_compute_dwindle(const struct WMState *state,
                              const struct WMConfig *config,
                              int8_t buffer_index, WMRect screen,
                              WMFrameChange *out_frames, int max_frame);

// compute snap frame for a single window
WMRect wm_layout_compute_snap(int snap_action, WMRect screen,
                              const struct WMConfig *config);

// apply inner/outer gaps to a rect
WMRect wm_layout_apply_gaps(WMRect rect, int top, int right, int bottom,
                            int left);

// check if rect is valid
bool wm_layout_rect_valid(WMRect rect);

#endif
