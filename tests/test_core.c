#include <assert.h>
#include <stdio.h>

#include "wm_actions.h"
#include "wm_config.h"
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

TEST(config_init) {
  WMConfig config;
  wm_config_init(&config);
  assert(config.gaps_outer.top == 12);
  assert(config.rules_count == 0);
  assert(config.bindings_count == 0);
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

TEST(effects_init) {
  WMEffects effects;
  wm_effects_init(&effects);

  assert(effects.to_hide_count == 0);
  assert(effects.to_show_count == 0);
  assert(effects.to_raise == 0);
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

int main(void) {
  printf("Running core tests...\n");
  printf("\nState:\n");
  RUN_TEST(state_init);
  printf("\nConfig:\n");
  RUN_TEST(config_init);
  RUN_TEST(config_add_rule);
  RUN_TEST(config_add_binding);
  printf("\nEffects:\n");
  RUN_TEST(effects_init);
  RUN_TEST(effects_add);
  printf("\nLayout:\n");
  RUN_TEST(layout_apply_gaps);
  RUN_TEST(layout_rect_valid);
  printf("\nAll tests passed\n");
  return 0;
}
