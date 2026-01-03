#ifndef WM_CONFIG_H
#define WM_CONFIG_H

#include "wm_actions.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define WM_MAX_RULES 64    // max rules for auto-assignment
#define WM_MAX_BINDINGS 64 // max global hotkey bindings

// gaps - CSS style gaps
typedef struct {
  int top;
  int right;
  int bottom;
  int left;
} WMGap;

// rules - auto-assignment rules
typedef struct {
  char bundle_identifier[128]; // e.g., com.spotify.client
  int8_t target_buffer;        // e.g., 3 (buffer 4)
} WMRule;

// bindings
typedef enum {
  WM_MOD_NONE = 0,       // no modifiers
  WM_MOD_OPT = 1 << 0,   // Option/Alt
  WM_MOD_SHIFT = 1 << 1, // Shift
  WM_MOD_CMD = 1 << 2,   // Command/Super
  WM_MOD_CTRL = 1 << 3,  // Control
} WMModifier;

// bindings - global hotkey bindings
typedef struct {
  int modifiers;               // combined modifier bitmask
  int keycode;                 // keycode
  WMActionType action;         // action to perform
  int action_argument;         // argument for the action
  char bundle_identifier[128]; // bundle identifier for app launch
} WMBinding;

// config - global configuration (default + user overrides)
typedef struct {
  WMGap gaps_outer;
  WMGap gaps_inner;

  WMRule rules[WM_MAX_RULES];
  int rules_count;

  WMBinding bindings[WM_MAX_BINDINGS];
  int bindings_count;
} WMConfig;

// initialize the config with defaults
void wm_config_init(WMConfig *config);

// load config from file. Returns false on error
// path is ~/.config/.dwin by default
bool wm_config_load(WMConfig *config, const char *path);

// add a rule programatically
bool wm_config_add_rule(WMConfig *config, const char *bundle_identifier,
                        int target_buffer);

// match a bundle identifier against rules. Return buffers index or -1
int wm_config_match_rule(const WMConfig *config, const char *bundle_identifier);

// add a binding programatically
bool wm_config_add_binding(WMConfig *config, int modifiers, int keycode,
                           WMActionType action, int action_argument,
                           const char *bundle_identifier);
// match a keypress against bindings. Return action type
bool wm_config_match_binding(const WMConfig *config, int modifiers, int keycode,
                             WMAction *out_action);

#endif
