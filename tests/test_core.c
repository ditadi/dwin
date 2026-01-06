#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "wm_actions.h"
#include "wm_config.h"
#include "wm_layout.h"
#include "wm_state.h"

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name)                                                         \
  do {                                                                         \
    printf("    %s... ", #name);                                               \
    test_##name();                                                             \
    printf("✓ OK\n");                                                          \
  } while (0)

TEST(state_init) {
  WMState state;
  wm_state_init(&state);
  assert(state.active_buffer == 0);
  assert(state.is_passthrough_mode == false);
  assert(state.app_registry.app_count == 0);
}

TEST(state_register_app) {
  WMState state;
  wm_state_init(&state);

  // register first app
  int index = wm_state_register_app(&state, 1234, "com.apple.Terminal");
  assert(index == 0);
  assert(state.app_registry.app_count == 1);

  // find it
  const WMApp *app = wm_state_find_app(&state, 1234);
  assert(app != NULL);
  assert(app->pid == 1234);
  assert(strcmp(app->bundle_identifier, "com.apple.Terminal") == 0);
  assert(app->buffer_index == -1);
  assert(app->is_managed == true);

  // regist second app
  index = wm_state_register_app(&state, 5678, "com.google.Chrome");
  assert(index == 1);
  assert(state.app_registry.app_count == 2);

  // re-register same pid returns existing index
  int16_t idx_dup = wm_state_register_app(&state, 1234, "com.apple.Terminal");
  assert(idx_dup == 0);
  assert(state.app_registry.app_count == 2); // count unchanged
}

TEST(state_unregister_app) {
  WMState state;
  wm_state_init(&state);

  // register 3 apps
  wm_state_register_app(&state, 1234, "com.apple.Terminal");
  wm_state_register_app(&state, 5678, "com.google.Chrome");
  wm_state_register_app(&state, 9012, "com.spotify.client");

  // unregister middle one (tests swap with last)
  wm_state_unregister_app(&state, 5678);
  assert(state.app_registry.app_count == 2);

  // app 5678 should be gone
  assert(wm_state_find_app(&state, 5678) == NULL);

  // app 1234 and 9012 should still be there
  assert(wm_state_find_app(&state, 1234) != NULL);
  assert(wm_state_find_app(&state, 9012) != NULL);

  // unregister non-existent (should be no-op)
  wm_state_unregister_app(&state, 12345);
  assert(state.app_registry.app_count == 2);
}

TEST(state_assign_to_buffer) {
  WMState state;
  wm_state_init(&state);

  // register 3 apps
  wm_state_register_app(&state, 1234, "com.apple.Terminal");
  wm_state_register_app(&state, 5678, "com.google.Chrome");
  wm_state_register_app(&state, 9012, "com.spotify.client");

  // assign to buffers
  wm_state_assign_to_buffer(&state, 1234, 0);
  wm_state_assign_to_buffer(&state, 5678, 0);
  wm_state_assign_to_buffer(&state, 9012, 1);

  // verify assignments
  assert(wm_state_find_app(&state, 1234)->buffer_index == 0);
  assert(wm_state_find_app(&state, 5678)->buffer_index == 0);
  assert(wm_state_find_app(&state, 9012)->buffer_index == 1);

  // reassign
  wm_state_assign_to_buffer(&state, 5678, 2);
  assert(wm_state_find_app(&state, 5678)->buffer_index == 2);

  // unassign
  wm_state_assign_to_buffer(&state, 5678, -1);
  assert(wm_state_find_app(&state, 5678)->buffer_index == -1);

  // invalid buffer index (should be no-op)
  wm_state_assign_to_buffer(&state, 5678, 99);
  assert(wm_state_find_app(&state, 5678)->buffer_index == -1);
}

TEST(state_get_buffer_pids) {
  WMState state;
  wm_state_init(&state);

  // register 3 apps
  wm_state_register_app(&state, 1234, "com.apple.Terminal");
  wm_state_register_app(&state, 5678, "com.google.Chrome");
  wm_state_register_app(&state, 9012, "com.spotify.client");

  wm_state_assign_to_buffer(&state, 1234, 0);
  wm_state_assign_to_buffer(&state, 5678, 0);
  wm_state_assign_to_buffer(&state, 9012, 1);

  // get pids from buffer 0
  pid_t pids[WM_MAX_APPS];
  int count = wm_state_get_buffer_pids(&state, 0, pids, WM_MAX_APPS);
  assert(count == 2);

  // order may vary, so check both exists
  bool found1 = false, found2 = false;
  for (int i = 0; i < count; i++) {
    if (pids[i] == 1234)
      found1 = true;
    if (pids[i] == 5678)
      found2 = true;
  }
  assert(found1 && found2);

  // get buffer 1 pids
  count = wm_state_get_buffer_pids(&state, 1, pids, WM_MAX_APPS);
  assert(count == 1);
  assert(pids[0] == 9012);

  // get empty buffer
  count = wm_state_get_buffer_pids(&state, 2, pids, WM_MAX_APPS);
  assert(count == 0);
}

TEST(state_set_focused) {
  WMState state;
  wm_state_init(&state);

  // register app
  wm_state_register_app(&state, 1234, "com.apple.Terminal");
  wm_state_assign_to_buffer(&state, 1234, 0);

  // set focused
  wm_state_set_focused(&state, 1234);
  assert(state.buffers[0].last_focused_pid == 1234);

  // focus unregistered app (should be no-op)
  wm_state_set_focused(&state, 5678);
  assert(state.buffers[0].last_focused_pid == 1234);
}

TEST(state_set_floating) {
  WMState state;
  wm_state_init(&state);

  // register app
  wm_state_register_app(&state, 1234, "com.apple.Terminal");

  // default is not floating
  const WMApp *app = wm_state_find_app(&state, 1234);
  assert(app->is_floating == false);

  // set floating
  wm_state_set_floating(&state, 1234, true);
  app = wm_state_find_app(&state, 1234);
  assert(app->is_floating == true);

  // set back to tiled
  wm_state_set_floating(&state, 1234, false);
  app = wm_state_find_app(&state, 1234);
  assert(app->is_floating == false);

  // set floating on non-existent app (should be no-op)
  wm_state_set_floating(&state, 9999, true);
  // no crash = success
}

TEST(state_pid_map_collision) {
  WMState state;
  wm_state_init(&state);

  // PIDs that will collide (same has = pid % 256)
  // 100, 356, 612 all have hash = 100
  wm_state_register_app(&state, 100, "com.apple.Terminal");
  wm_state_register_app(&state, 356, "com.google.Chrome");
  wm_state_register_app(&state, 612, "com.spotify.client");

  // verify all are registered
  assert(state.app_registry.app_count == 3);

  // all should be findable
  assert(wm_state_find_app(&state, 100) != NULL);
  assert(wm_state_find_app(&state, 356) != NULL);
  assert(wm_state_find_app(&state, 612) != NULL);

  // remove middle collision
  wm_state_unregister_app(&state, 356);
  assert(state.app_registry.app_count == 2);
  assert(wm_state_find_app(&state, 356) == NULL);
  assert(wm_state_find_app(&state, 100) != NULL);
  assert(wm_state_find_app(&state, 612) != NULL);

  // remove last collision
  wm_state_unregister_app(&state, 612);

  assert(wm_state_find_app(&state, 612) == NULL);
  assert(wm_state_find_app(&state, 100) != NULL);
}

TEST(config_init) {
  WMConfig config;
  wm_config_init(&config);
  assert(config.gaps_outer.top == 12);
  assert(config.rules_count == 0);
  assert(config.bindings_count == 20); // 10 buffer + 1 passthrough + 9 snap
}

TEST(config_add_rule) {
  WMConfig config;
  wm_config_init(&config);
  bool ok = wm_config_add_rule(&config, "com.apple.Terminal", 1);
  assert(ok);
  assert(config.rules_count == 1);

  int buffer = wm_config_match_rule(&config, "com.apple.Terminal");
  assert(buffer == 1);

  buffer = wm_config_match_rule(&config, "com.unknown.app");
  assert(buffer == -1);
}

TEST(config_add_binding) {
  WMConfig config;
  wm_config_init(&config);

  bool ok = wm_config_add_binding(&config, WM_MOD_OPT, 18,
                                  WM_ACTION_SWITCH_BUFFER, 0, NULL);
  assert(ok);

  WMAction action;
  bool found = wm_config_match_binding(&config, WM_MOD_OPT, 18, &action);
  assert(found);
  assert(action.type == WM_ACTION_SWITCH_BUFFER);
}

TEST(config_default_bindings) {
  WMConfig config;
  wm_config_init(&config);

  WMAction action;

  // opt+1 = switch to buffer 0
  bool found = wm_config_match_binding(&config, WM_MOD_OPT, 18, &action);
  assert(found);
  assert(action.type == WM_ACTION_SWITCH_BUFFER);
  assert(action.target_buffer == 0);

  // opt + 5 = switch to buffer 4
  found = wm_config_match_binding(&config, WM_MOD_OPT, 23, &action);
  assert(found);
  assert(action.type == WM_ACTION_SWITCH_BUFFER);
  assert(action.target_buffer == 4);

  // shift+opt+1 = move app to buffer 0
  found =
      wm_config_match_binding(&config, WM_MOD_SHIFT | WM_MOD_OPT, 18, &action);
  assert(found);
  assert(action.type == WM_ACTION_MOVE_BUFFER);

  // shift+opt+5 = move app to buffer 4
  found =
      wm_config_match_binding(&config, WM_MOD_SHIFT | WM_MOD_OPT, 23, &action);
  assert(found);
  assert(action.type == WM_ACTION_MOVE_BUFFER);
  assert(action.target_buffer == 4);

  // shift+opt+p = toggle passthrough mode
  found =
      wm_config_match_binding(&config, WM_MOD_SHIFT | WM_MOD_OPT, 35, &action);
  assert(found);
  assert(action.type == WM_ACTION_TOGGLE_PASSTHROUGH);

  // unknown binding = no action
  found = wm_config_match_binding(&config, WM_MOD_NONE, 0, &action);
  assert(!found);
}

TEST(effects_init) {
  WMEffects effects;
  wm_effects_init(&effects);

  assert(effects.to_hide_count == 0);
  assert(effects.to_show_count == 0);
  assert(effects.to_raise_count == 0);
}

TEST(effects_add) {
  WMEffects effects;
  wm_effects_init(&effects);
  wm_effects_add_hide(&effects, 1234);
  wm_effects_add_show(&effects, 5678);

  assert(effects.to_hide_count == 1);
  assert(effects.to_hide[0] == 1234);
  assert(effects.to_show_count == 1);
  assert(effects.to_show[0] == 5678);
}

TEST(action_switch_buffer) {
  WMState state;
  wm_state_init(&state);

  wm_state_register_app(&state, 1234, "com.apple.Terminal");
  wm_state_register_app(&state, 5678, "com.google.Chrome");
  wm_state_register_app(&state, 9012, "com.spotify.client");

  wm_state_assign_to_buffer(&state, 1234, 0);
  wm_state_assign_to_buffer(&state, 5678, 0);
  wm_state_assign_to_buffer(&state, 9012, 1);

  state.active_buffer = 0;

  // switch to buffer 1
  WMEffects effects;
  bool ok = wm_action_switch_buffer(&state, 1, &effects);

  assert(ok);
  assert(state.active_buffer == 1);

  // should show app 9012
  assert(effects.to_show_count == 1);
  assert(effects.to_show[0] == 9012);

  // should hide app 1234, 5678
  assert(effects.to_hide_count == 2);

  // should raise app 9012 (only app in buffer, uses fallback to first)
  assert(effects.to_raise_count == 1);
  assert(effects.to_raise[0] == 9012);

  assert(effects.needs_layout);
  assert(effects.layout_buffer == 1);
}

TEST(action_switch_buffer_same) {
  WMState state;
  wm_state_init(&state);
  state.active_buffer = 0;

  WMEffects effects;
  bool ok = wm_action_switch_buffer(&state, 0, &effects);

  assert(!ok); // no-op, same buffer
}

TEST(action_switch_buffer_empty) {
  WMState state;
  wm_state_init(&state);

  // buffer 1 is empty
  state.active_buffer = 0;

  WMEffects effects;
  bool ok = wm_action_switch_buffer(&state, 1, &effects);

  assert(ok);
  assert(state.active_buffer == 1);
  assert(effects.to_raise_count == 0); // nothing to raise
}

TEST(action_switch_buffer_with_last_focused) {
  WMState state;
  wm_state_init(&state);

  wm_state_register_app(&state, 1234, "com.apple.Terminal");
  wm_state_register_app(&state, 5678, "com.google.Chrome");
  wm_state_assign_to_buffer(&state, 1234, 1);
  wm_state_assign_to_buffer(&state, 5678, 1);

  // set last focused to 5678
  state.buffers[1].last_focused_pid = 5678;
  state.active_buffer = 0;

  WMEffects effects;
  wm_action_switch_buffer(&state, 1, &effects);

  // should raise last_focused_pid (5678), not first app
  assert(effects.to_raise_count == 1);
  assert(effects.to_raise[0] == 5678);
}

TEST(action_process_move_buffer) {
  WMState state;
  wm_state_init(&state);

  // app in buffer 0, another in buffer 1
  wm_state_register_app(&state, 1234, "com.apple.Terminal");
  wm_state_register_app(&state, 5678, "com.google.Chrome");
  wm_state_assign_to_buffer(&state, 1234, 0);
  wm_state_assign_to_buffer(&state, 5678, 1);

  // move app from buffer 0 to buffer 1
  WMAction action = {
      .type = WM_ACTION_MOVE_BUFFER, .target_pid = 1234, .target_buffer = 1};
  WMEffects effects;
  bool ok = wm_action_process(&state, &action, &effects);
  assert(ok);

  // state checks
  assert(state.active_buffer == 1);
  assert(wm_state_find_app(&state, 1234)->buffer_index == 1);

  // effects checks
  assert(effects.to_show_count == 2);

  // check that the moved app is raised
  assert(effects.to_raise_count >= 1);

  // the moved app should appear in the raise list
  bool found_moved = false;
  for (int i = 0; i < effects.to_raise_count; i++) {
    if (effects.to_raise[i] == 1234)
      found_moved = true;
  }
  assert(found_moved);

  assert(effects.needs_layout);
  assert(effects.layout_buffer == 1);
}

TEST(action_process_switch) {
  WMState state;
  wm_state_init(&state);

  wm_state_register_app(&state, 1234, "com.apple.Terminal");
  wm_state_assign_to_buffer(&state, 1234, 1);
  state.active_buffer = 0;

  WMAction action = {.type = WM_ACTION_SWITCH_BUFFER, .target_buffer = 1};
  WMEffects effects;

  bool ok = wm_action_process(&state, &action, &effects);
  assert(ok);
  assert(state.active_buffer == 1);
}

TEST(action_process_passthrough) {
  WMState state;
  wm_state_init(&state);
  assert(state.is_passthrough_mode == false);

  WMAction action = {.type = WM_ACTION_TOGGLE_PASSTHROUGH};
  WMEffects effects;

  wm_action_process(&state, &action, &effects);
  assert(state.is_passthrough_mode == true);

  wm_action_process(&state, &action, &effects);
  assert(state.is_passthrough_mode == false);
}

TEST(layout_apply_gaps) {
  WMRect rect = {.x = 0, .y = 0, .width = 100, .height = 100};
  WMRect result = wm_layout_apply_gaps(rect, 10, 10, 10, 10);

  assert(result.x == 10);
  assert(result.y == 10);
  assert(result.width == 80);
  assert(result.height == 80);
}

TEST(layout_rect_valid) {
  WMRect valid = {.x = 0, .y = 0, .width = 100, .height = 100};
  WMRect invalid = {.x = 0, .y = 0, .width = 0, .height = 100};

  assert(wm_layout_rect_valid(valid) == true);
  assert(wm_layout_rect_valid(invalid) == false);
}

TEST(layout_snap_left) {
  WMConfig config;
  wm_config_init(&config);
  config.gaps_outer = (WMGap){.top = 10, .right = 10, .bottom = 10, .left = 10};
  config.gaps_inner = (WMGap){.top = 8, .right = 8, .bottom = 8, .left = 8};

  WMRect screen = {.x = 0, .y = 0, .width = 1920, .height = 1080};
  WMRect result = wm_layout_compute_snap(WM_ACTION_SNAP_LEFT, screen, &config);

  // x = 0 + 10 = 10
  // y = 0 + 10 = 10
  // width = (1920 - 10 - 10 - 8) / 2 = 946
  // height = 1080 - 10 - 10 = 1060
  assert(result.x == 10);
  assert(result.y == 10);
  assert(result.width == 946);
  assert(result.height == 1060);
}

TEST(layout_snap_right) {
  WMConfig config;
  wm_config_init(&config);
  config.gaps_outer = (WMGap){.top = 10, .right = 10, .bottom = 10, .left = 10};
  config.gaps_inner = (WMGap){.top = 8, .right = 8, .bottom = 8, .left = 8};

  WMRect screen = {.x = 0, .y = 0, .width = 1920, .height = 1080};
  WMRect result = wm_layout_compute_snap(WM_ACTION_SNAP_RIGHT, screen, &config);

  // x = 10 + 946 + 8 = 964
  assert(result.x == 964);
  assert(result.y == 10);
  assert(result.width == 946);
  assert(result.height == 1060);
}

TEST(layout_snap_maximize) {
  WMConfig config;
  wm_config_init(&config);
  config.gaps_outer = (WMGap){.top = 10, .right = 10, .bottom = 10, .left = 10};

  WMRect screen = {.x = 0, .y = 0, .width = 1920, .height = 1080};
  WMRect result =
      wm_layout_compute_snap(WM_ACTION_SNAP_MAXIMIZE, screen, &config);

  assert(result.x == 10);
  assert(result.y == 10);
  assert(result.width == 1900);
  assert(result.height == 1060);
}

TEST(layout_snap_center) {
  WMConfig config;
  wm_config_init(&config);

  WMRect screen = {.x = 0, .y = 0, .width = 1920, .height = 1080};
  WMRect result =
      wm_layout_compute_snap(WM_ACTION_SNAP_CENTER, screen, &config);

  // width = 1920 * 0.6 = 1152
  // height = 1080 * 0.7 = 756
  // x = (1920 - 1152) / 2 = 384
  // y = (1080 - 756) / 2 = 162
  assert(result.width == 1152);
  assert(result.height == 756);
  assert(result.x == 384);
  assert(result.y == 162);
}

TEST(layout_snap_top) {
  WMConfig config;
  wm_config_init(&config);
  config.gaps_outer = (WMGap){.top = 10, .right = 10, .bottom = 10, .left = 10};
  config.gaps_inner = (WMGap){.top = 8, .right = 8, .bottom = 8, .left = 8};

  WMRect screen = {.x = 0, .y = 0, .width = 1920, .height = 1080};
  WMRect result = wm_layout_compute_snap(WM_ACTION_SNAP_TOP, screen, &config);

  // height = (1080 - 10 - 10 - 8) / 2 = 526
  // y = 10 + 526 + 8 = 544
  assert(result.x == 10);
  assert(result.y == 544);
  assert(result.width == 1900);
  assert(result.height == 526);
}

TEST(layout_snap_bottom) {
  WMConfig config;
  wm_config_init(&config);
  config.gaps_outer = (WMGap){.top = 10, .right = 10, .bottom = 10, .left = 10};
  config.gaps_inner = (WMGap){.top = 8, .right = 8, .bottom = 8, .left = 8};

  WMRect screen = {.x = 0, .y = 0, .width = 1920, .height = 1080};
  WMRect result =
      wm_layout_compute_snap(WM_ACTION_SNAP_BOTTOM, screen, &config);

  assert(result.x == 10);
  assert(result.y == 10);
  assert(result.width == 1900);
  assert(result.height == 526);
}

TEST(layout_snap_corners) {
  WMConfig config;
  wm_config_init(&config);
  config.gaps_outer = (WMGap){.top = 10, .right = 10, .bottom = 10, .left = 10};
  config.gaps_inner = (WMGap){.top = 8, .right = 8, .bottom = 8, .left = 8};

  WMRect screen = {.x = 0, .y = 0, .width = 1920, .height = 1080};

  // top-left
  WMRect tl =
      wm_layout_compute_snap(WM_ACTION_SNAP_TOP_LEFT, screen, &config);
  assert(tl.x == 10);
  assert(tl.y == 544);
  assert(tl.width == 946);
  assert(tl.height == 526);

  // top-right
  WMRect tr =
      wm_layout_compute_snap(WM_ACTION_SNAP_TOP_RIGHT, screen, &config);
  assert(tr.x == 964);
  assert(tr.y == 544);
  assert(tr.width == 946);
  assert(tr.height == 526);

  // bottom-left
  WMRect bl =
      wm_layout_compute_snap(WM_ACTION_SNAP_BOTTOM_LEFT, screen, &config);
  assert(bl.x == 10);
  assert(bl.y == 10);
  assert(bl.width == 946);
  assert(bl.height == 526);

  // bottom-right
  WMRect br =
      wm_layout_compute_snap(WM_ACTION_SNAP_BOTTOM_RIGHT, screen, &config);
  assert(br.x == 964);
  assert(br.y == 10);
  assert(br.width == 946);
  assert(br.height == 526);
}

TEST(layout_dwindle_single) {
  WMState state;
  wm_state_init(&state);
  WMConfig config;
  wm_config_init(&config);
  config.gaps_outer = (WMGap){.top = 10, .right = 10, .bottom = 10, .left = 10};
  config.gaps_inner = (WMGap){.top = 8, .right = 8, .bottom = 8, .left = 8};

  wm_state_register_app(&state, 1234, "com.test.app");
  wm_state_assign_to_buffer(&state, 1234, 0);

  WMRect screen = {.x = 0, .y = 0, .width = 1920, .height = 1080};
  WMFrameChange frames[WM_MAX_APPS];
  int count =
      wm_layout_compute_dwindle(&state, 0, &config, screen, frames, WM_MAX_APPS);

  assert(count == 1);
  assert(frames[0].pid == 1234);
  assert(frames[0].frame.x == 10);
  assert(frames[0].frame.y == 10);
  assert(frames[0].frame.width == 1900);
  assert(frames[0].frame.height == 1060);
}

TEST(layout_dwindle_two) {
  WMState state;
  wm_state_init(&state);
  WMConfig config;
  wm_config_init(&config);
  config.gaps_outer = (WMGap){.top = 10, .right = 10, .bottom = 10, .left = 10};
  config.gaps_inner = (WMGap){.top = 8, .right = 8, .bottom = 8, .left = 8};

  wm_state_register_app(&state, 1234, "com.test.app1");
  wm_state_register_app(&state, 5678, "com.test.app2");
  wm_state_assign_to_buffer(&state, 1234, 0);
  wm_state_assign_to_buffer(&state, 5678, 0);

  WMRect screen = {.x = 0, .y = 0, .width = 1920, .height = 1080};
  WMFrameChange frames[WM_MAX_APPS];
  int count =
      wm_layout_compute_dwindle(&state, 0, &config, screen, frames, WM_MAX_APPS);

  assert(count == 2);
  // first app: left half
  assert(frames[0].frame.x == 10);
  assert(frames[0].frame.width == 946);
  // second app: right half
  assert(frames[1].frame.x == 964);
  assert(frames[1].frame.width == 946);
}

TEST(layout_dwindle_three) {
  WMState state;
  wm_state_init(&state);
  WMConfig config;
  wm_config_init(&config);
  config.gaps_outer = (WMGap){.top = 0, .right = 0, .bottom = 0, .left = 0};
  config.gaps_inner = (WMGap){.top = 0, .right = 0, .bottom = 0, .left = 0};

  wm_state_register_app(&state, 1, "com.test.app1");
  wm_state_register_app(&state, 2, "com.test.app2");
  wm_state_register_app(&state, 3, "com.test.app3");
  wm_state_assign_to_buffer(&state, 1, 0);
  wm_state_assign_to_buffer(&state, 2, 0);
  wm_state_assign_to_buffer(&state, 3, 0);

  WMRect screen = {.x = 0, .y = 0, .width = 1000, .height = 1000};
  WMFrameChange frames[WM_MAX_APPS];
  int count =
      wm_layout_compute_dwindle(&state, 0, &config, screen, frames, WM_MAX_APPS);

  assert(count == 3);
  // app 1: left half (500x1000)
  assert(frames[0].frame.width == 500);
  assert(frames[0].frame.height == 1000);
  // app 2: top-right (500x500)
  assert(frames[1].frame.width == 500);
  assert(frames[1].frame.height == 500);
  // app 3: bottom-right (500x500)
  assert(frames[2].frame.width == 500);
  assert(frames[2].frame.height == 500);
}

TEST(layout_dwindle_empty) {
  WMState state;
  wm_state_init(&state);
  WMConfig config;
  wm_config_init(&config);

  WMRect screen = {.x = 0, .y = 0, .width = 1920, .height = 1080};
  WMFrameChange frames[WM_MAX_APPS];
  int count =
      wm_layout_compute_dwindle(&state, 0, &config, screen, frames, WM_MAX_APPS);

  assert(count == 0);
}

TEST(layout_dwindle_floating_skipped) {
  WMState state;
  wm_state_init(&state);
  WMConfig config;
  wm_config_init(&config);
  config.gaps_outer = (WMGap){.top = 0, .right = 0, .bottom = 0, .left = 0};
  config.gaps_inner = (WMGap){.top = 0, .right = 0, .bottom = 0, .left = 0};

  wm_state_register_app(&state, 1, "com.test.app1");
  wm_state_register_app(&state, 2, "com.test.app2");
  wm_state_assign_to_buffer(&state, 1, 0);
  wm_state_assign_to_buffer(&state, 2, 0);

  // make app 2 floating - it should be skipped from dwindle
  wm_state_set_floating(&state, 2, true);

  WMRect screen = {.x = 0, .y = 0, .width = 1000, .height = 1000};
  WMFrameChange frames[WM_MAX_APPS];
  int count =
      wm_layout_compute_dwindle(&state, 0, &config, screen, frames, WM_MAX_APPS);

  // only 1 app should be laid out (the non-floating one)
  assert(count == 1);
  assert(frames[0].pid == 1);
  assert(frames[0].frame.width == 1000);
  assert(frames[0].frame.height == 1000);
}

TEST(layout_dwindle_after_snap_retile) {
  WMState state;
  wm_state_init(&state);
  WMConfig config;
  wm_config_init(&config);
  config.gaps_outer = (WMGap){.top = 0, .right = 0, .bottom = 0, .left = 0};
  config.gaps_inner = (WMGap){.top = 0, .right = 0, .bottom = 0, .left = 0};

  wm_state_register_app(&state, 1, "com.test.app1");
  wm_state_register_app(&state, 2, "com.test.app2");
  wm_state_register_app(&state, 3, "com.test.app3");
  wm_state_assign_to_buffer(&state, 1, 0);
  wm_state_assign_to_buffer(&state, 2, 0);
  wm_state_assign_to_buffer(&state, 3, 0);

  WMRect screen = {.x = 0, .y = 0, .width = 1000, .height = 1000};
  WMFrameChange frames[WM_MAX_APPS];

  // initially all 3 apps are tiled
  int count =
      wm_layout_compute_dwindle(&state, 0, &config, screen, frames, WM_MAX_APPS);
  assert(count == 3);

  // snap app 2 (makes it floating)
  wm_state_set_floating(&state, 2, true);

  // now only 2 apps should be in dwindle
  count =
      wm_layout_compute_dwindle(&state, 0, &config, screen, frames, WM_MAX_APPS);
  assert(count == 2);
  assert(frames[0].pid == 1);
  assert(frames[1].pid == 3);
  // with 2 apps, they split horizontally
  assert(frames[0].frame.width == 500);
  assert(frames[1].frame.width == 500);

  // retile app 2 (returns to dwindle)
  wm_state_set_floating(&state, 2, false);

  // now all 3 apps should be in dwindle again
  count =
      wm_layout_compute_dwindle(&state, 0, &config, screen, frames, WM_MAX_APPS);
  assert(count == 3);
}

int main(void) {
  printf("Running core tests...\n");
  printf("\nState:\n");
  RUN_TEST(state_init);
  RUN_TEST(state_register_app);
  RUN_TEST(state_unregister_app);
  RUN_TEST(state_assign_to_buffer);
  RUN_TEST(state_get_buffer_pids);
  RUN_TEST(state_set_focused);
  RUN_TEST(state_set_floating);
  RUN_TEST(state_pid_map_collision);
  printf("\nConfig:\n");
  RUN_TEST(config_init);
  RUN_TEST(config_add_rule);
  RUN_TEST(config_add_binding);
  RUN_TEST(config_default_bindings);
  printf("\nEffects:\n");
  RUN_TEST(effects_init);
  RUN_TEST(effects_add);
  printf("\nActions:\n");
  RUN_TEST(action_switch_buffer);
  RUN_TEST(action_switch_buffer_same);
  RUN_TEST(action_switch_buffer_empty);
  RUN_TEST(action_switch_buffer_with_last_focused);
  RUN_TEST(action_process_move_buffer);
  RUN_TEST(action_process_switch);
  RUN_TEST(action_process_passthrough);
  printf("\nLayout:\n");
  RUN_TEST(layout_apply_gaps);
  RUN_TEST(layout_rect_valid);
  RUN_TEST(layout_snap_left);
  RUN_TEST(layout_snap_right);
  RUN_TEST(layout_snap_maximize);
  RUN_TEST(layout_snap_center);
  RUN_TEST(layout_snap_top);
  RUN_TEST(layout_snap_bottom);
  RUN_TEST(layout_snap_corners);
  RUN_TEST(layout_dwindle_single);
  RUN_TEST(layout_dwindle_two);
  RUN_TEST(layout_dwindle_three);
  RUN_TEST(layout_dwindle_empty);
  RUN_TEST(layout_dwindle_floating_skipped);
  RUN_TEST(layout_dwindle_after_snap_retile);
  printf("\nAll tests passed\n");
  return 0;
}
