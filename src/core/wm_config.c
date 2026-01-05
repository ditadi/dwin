#include "wm_config.h"
#include <string.h>

void wm_config_init(WMConfig *config) {
  memset(config, 0, sizeof(WMConfig));

  // default gaps
  config->gaps_outer =
      (WMGap){.top = 12, .right = 12, .bottom = 12, .left = 12};
  config->gaps_inner = (WMGap){.top = 8, .right = 8, .bottom = 8, .left = 8};

  /**
   * default bindings:
   * Opt+1-5 = switch to buffer 1-5
   * Shift+Opt+1-5 = move app to buffer 1-5
   * keycodes: 1=18, 2=19, 3=20, 4=21, 5=23
   */
  static const int keycodes[] = {18, 19, 20, 21, 23};
  for (int i = 0; i < 5; i++) {
    wm_config_add_binding(config, WM_MOD_OPT, keycodes[i],
                          WM_ACTION_SWITCH_BUFFER, i, NULL);
    wm_config_add_binding(config, WM_MOD_SHIFT | WM_MOD_OPT, keycodes[i],
                          WM_ACTION_MOVE_BUFFER, i, NULL);
  }

  // default binding: Shift+Opt+P = toggle passthrough mode
  wm_config_add_binding(config, WM_MOD_SHIFT | WM_MOD_OPT, 35,
                        WM_ACTION_TOGGLE_PASSTHROUGH, 0, NULL);
}

bool wm_config_load(WMConfig *config, const char *path) {
  // @TODO: implement
  (void)path;
  wm_config_init(config);
  return true;
}

bool wm_config_add_rule(WMConfig *config, const char *bundle_identifier,
                        int target_buffer) {
  if (config->rules_count >= WM_MAX_RULES) {
    return false;
  }

  WMRule *rule = &config->rules[config->rules_count++];
  strncpy(rule->bundle_identifier, bundle_identifier,
          sizeof(rule->bundle_identifier) - 1);
  rule->bundle_identifier[sizeof(rule->bundle_identifier) - 1] = '\0';
  rule->target_buffer = (int8_t)target_buffer;
  return true;
}

int wm_config_match_rule(const WMConfig *config,
                         const char *bundle_identifier) {
  for (int i = 0; i < config->rules_count; i++) {
    if (strcmp(config->rules[i].bundle_identifier, bundle_identifier) == 0) {
      return config->rules[i].target_buffer;
    }
  }
  return -1;
}

bool wm_config_add_binding(WMConfig *config, int modifiers, int keycode,
                           WMActionType action, int action_argument,
                           const char *bundle_identifier) {
  if (config->bindings_count >= WM_MAX_BINDINGS) {
    return false;
  }

  WMBinding *binding = &config->bindings[config->bindings_count++];
  binding->modifiers = modifiers;
  binding->keycode = keycode;
  binding->action = action;
  binding->action_argument = action_argument;

  if (bundle_identifier) {
    strncpy(binding->bundle_identifier, bundle_identifier,
            sizeof(binding->bundle_identifier) - 1);
    binding->bundle_identifier[sizeof(binding->bundle_identifier) - 1] = '\0';
  } else {
    binding->bundle_identifier[0] = '\0';
  }

  return true;
}

bool wm_config_match_binding(const WMConfig *config, int modifiers, int keycode,
                             WMAction *out_action) {
  for (int i = 0; i < config->bindings_count; i++) {
    const WMBinding *binding = &config->bindings[i];
    if (binding->modifiers == modifiers && binding->keycode == keycode) {
      out_action->type = binding->action;
      out_action->target_buffer = binding->action_argument;
      out_action->target_pid = 0;
      strncpy(out_action->bundle_identifier, binding->bundle_identifier,
              sizeof(out_action->bundle_identifier) - 1);
      out_action->bundle_identifier[sizeof(out_action->bundle_identifier) - 1] =
          '\0';
      return true;
    }
  }
  return false;
}
