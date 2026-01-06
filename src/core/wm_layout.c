#include "wm_layout.h"
#include "wm_config.h"
#include "wm_runtime.h"
#include "wm_state.h"

// recursive helper for dwindle
static int dwindle_recurse(pid_t *pids, int count, WMRect area,
                           const struct WMConfig *config, int depth,
                           WMFrameChange *out_frames, int frame_idx,
                           int max_frames) {
  if (count <= 0 || frame_idx >= max_frames)
    return frame_idx;

  if (count == 1) {
    // single app fills entire area
    out_frames[frame_idx].pid = pids[0];
    out_frames[frame_idx].frame = area;
    return frame_idx + 1;
  }

  // split: even depth = horizontal, odd depth = vertical
  bool horizontal = (depth % 2 == 0);

  if (horizontal) {
    double gap = config->gaps_inner.left;
    double half_width = (area.width - gap) / 2.0;

    // first app gets left half
    WMRect left = {
        .x = area.x, .y = area.y, .width = half_width, .height = area.height};
    out_frames[frame_idx].pid = pids[0];
    out_frames[frame_idx].frame = left;
    frame_idx++;

    // rest get right half
    WMRect right = {.x = area.x + half_width + gap,
                    .y = area.y,
                    .width = area.width - half_width - gap,
                    .height = area.height};
    return dwindle_recurse(pids + 1, count - 1, right, config, depth + 1,
                           out_frames, frame_idx, max_frames);
  } else {
    double gap = config->gaps_inner.top;
    double half_height = (area.height - gap) / 2.0;

    // first app gets top half
    WMRect top = {.x = area.x,
                  .y = area.y + half_height + gap,
                  .width = area.width,
                  .height = half_height};
    out_frames[frame_idx].pid = pids[0];
    out_frames[frame_idx].frame = top;
    frame_idx++;

    // rest get bottom half
    WMRect bottom = {.x = area.x,
                     .y = area.y,
                     .width = area.width,
                     .height = area.height - half_height - gap};
    return dwindle_recurse(pids + 1, count - 1, bottom, config, depth + 1,
                           out_frames, frame_idx, max_frames);
  }
}

int wm_layout_compute_dwindle(const struct WMState *state, int8_t buffer_index,
                              const struct WMConfig *config, WMRect screen,
                              WMFrameChange *out_frames, int max_frame) {
  if (!state || !config || !out_frames || buffer_index < 0 ||
      buffer_index >= WM_MAX_BUFFERS || max_frame <= 0)
    return 0;

  // get non-floating pids in this buffer
  pid_t pids[WM_MAX_APPS];
  int count = 0;

  for (int i = 0; i < state->app_registry.app_count && count <= WM_MAX_APPS;
       i++) {
    const WMApp *app = &state->app_registry.apps[i];
    if (app->buffer_index == buffer_index && !app->is_floating)
      pids[count++] = app->pid;
  }

  if (count == 0)
    return 0;

  // compute usable area
  WMRect usable = {.x = screen.x + config->gaps_outer.left,
                   .y = screen.y + config->gaps_outer.bottom,
                   .width = screen.width - config->gaps_outer.left -
                            config->gaps_outer.right,
                   .height = screen.height - config->gaps_outer.top -
                             config->gaps_outer.bottom};

  return dwindle_recurse(pids, count, usable, config, 0, out_frames, 0,
                         max_frame);
}

WMRect wm_layout_compute_snap(int snap_action, WMRect screen,
                              const struct WMConfig *config) {
  // apply outer gaps
  double x = screen.x + config->gaps_outer.left;
  double y = screen.y + config->gaps_outer.top;
  double width =
      screen.width - config->gaps_outer.left - config->gaps_outer.right;
  double height =
      screen.height - config->gaps_outer.top - config->gaps_outer.bottom;

  double half_width = (width - config->gaps_inner.left) / 2.0;
  double half_height = (height - config->gaps_inner.top) / 2.0;

  switch (snap_action) {
  case WM_ACTION_SNAP_LEFT:
    return (WMRect){.x = x, .y = y, .width = half_width, .height = height};
  case WM_ACTION_SNAP_RIGHT:
    return (WMRect){.x = x + half_width + config->gaps_inner.left,
                    .y = y,
                    .width = half_width,
                    .height = height};
  case WM_ACTION_SNAP_TOP:
    return (WMRect){.x = x,
                    .y = y + half_height + config->gaps_inner.top,
                    .width = width,
                    .height = half_height};
  case WM_ACTION_SNAP_BOTTOM:
    return (WMRect){.x = x, .y = y, .width = width, .height = half_height};
  case WM_ACTION_SNAP_MAXIMIZE:
    return (WMRect){.x = x, .y = y, .width = width, .height = height};
  case WM_ACTION_SNAP_CENTER: {
    double center_width = screen.width * 0.6;
    double center_height = screen.height * 0.7;
    return (WMRect){.x = screen.x + (screen.width - center_width) / 2.0,
                    .y = screen.y + (screen.height - center_height) / 2.0,
                    .width = center_width,
                    .height = center_height};
  }
  case WM_ACTION_SNAP_TOP_LEFT:
    return (WMRect){.x = x,
                    .y = y + half_height + config->gaps_inner.top,
                    .width = half_width,
                    .height = half_height};

  case WM_ACTION_SNAP_TOP_RIGHT:
    return (WMRect){.x = x + half_width + config->gaps_inner.left,
                    .y = y + half_height + config->gaps_inner.top,
                    .width = half_width,
                    .height = half_height};

  case WM_ACTION_SNAP_BOTTOM_LEFT:
    return (WMRect){.x = x, .y = y, .width = half_width, .height = half_height};

  case WM_ACTION_SNAP_BOTTOM_RIGHT:
    return (WMRect){.x = x + half_width + config->gaps_inner.left,
                    .y = y,
                    .width = half_width,
                    .height = half_height};
  default:
    return screen;
  }
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
