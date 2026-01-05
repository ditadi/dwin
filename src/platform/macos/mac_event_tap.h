#ifndef MAC_EVENT_TAP_H
#define MAC_EVENT_TAP_H

#include "wm_config.h"
#import <stdbool.h>

// callback type for when an action is triggered
typedef void (*WMActionCallback)(int action_type, int argument,
                                 const char *bundle_identifier);

// initialize and start the event tap
bool mac_event_tap_start(WMConfig *config, WMActionCallback callback);

// stop and clean up the event tap
void mac_event_tap_stop(void);

// check if event tap is currently running
bool mac_event_tap_is_running(void);

// enable/disable passthrough mode (all keys pass through)
void mac_event_tap_set_passthrough(bool enabled);
bool mac_event_tap_get_passthrough(void);

#endif
