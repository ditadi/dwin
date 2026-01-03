#ifndef WM_RUNTIME_H
#define WM_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

// limits
#define WM_MAX_BUFFERS 5    // only 5 buffers for now
#define WM_MAX_APPS 128     // max apps tracked
#define WM_PID_MAP_SIZE 256 // max PIDs to track

// tracked application
typedef struct {
  pid_t pid;                   // process identifier
  char bundle_identifier[128]; // e.g., com.spotify.client
  int8_t buffer_index;         // -1 = unassigned, 0-4 = buffer index
  bool is_managed;             // false = WM ignore this app
  bool is_floating; // true = manual position, false = tiled (dwindle)
} WMApp;

// hash map entry for pid lookup
typedef struct {
  pid_t pid;         // 0 = empty slot
  int16_t app_index; // index into apps[]
} WMPidMapEntry;

// all tracked apps + fast pid lookup
typedef struct {
  WMApp apps[WM_MAX_APPS];                // array of apps
  int16_t app_count;                      // number of apps in the array
  WMPidMapEntry pid_map[WM_PID_MAP_SIZE]; // hash map for O(1) pid lookup
} WMAppRegistry;

// buffer = lightweight workspace with z-order tracking
typedef struct {
  pid_t last_focused_pid;     // cached last focused pid
  pid_t z_order[WM_MAX_APPS]; // z-order of pids in this buffer for layout
  int z_order_count;          // number of pids in the z-order array
} WMBuffer;

#endif
