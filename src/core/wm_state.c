#include "wm_state.h"
#include "wm_runtime.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

// hash function for pid
static uint32_t pid_hash(pid_t pid) {
  return (uint32_t)pid % WM_PID_MAP_SIZE;
}

// insert pid into pid map pointing to app index or return false if full
static bool pid_map_insert(WMPidMapEntry *map, int16_t app_index, pid_t pid) {
  uint32_t start = pid_hash(pid);
  uint32_t i = start;

  do {
    // empty slot or same pid
    if (map[i].pid == 0 || map[i].pid == pid) {
      map[i].pid = pid;
      map[i].app_index = app_index;
      return true;
    }

    // collision - try next slot
    i = (i + 1) % WM_PID_MAP_SIZE;
  } while (i != start);

  // map is full
  return false;
}

// search pid in pid map and return app index or -1 not found
static int16_t pid_map_search(const WMPidMapEntry *map, pid_t pid) {
  if (pid == 0)
    return -1; // pid 0 is not valid or empty slot

  uint32_t start = pid_hash(pid);
  uint32_t i = start;

  do {
    // found
    if (map[i].pid == pid) {
      return map[i].app_index;
    }

    // empty slot - not found
    if (map[i].pid == 0) {
      return -1;
    }

    // collision - try next slot
    i = (i + 1) % WM_PID_MAP_SIZE;
  } while (i != start);

  return -1;
}

// remove pid from pid map
// rehash the next entries to avoid open addressing
static void pid_map_remove(WMPidMapEntry *map, pid_t pid) {
  if (pid == 0)
    return; // pid 0 is not valid

  uint32_t start = pid_hash(pid);
  uint32_t i = start;

  do {
    // found
    if (map[i].pid == pid) {
      map[i].pid = 0;
      map[i].app_index = -1;
      uint32_t j = (i + 1) % WM_PID_MAP_SIZE;
      while (map[j].pid != 0) {
        pid_t rehash_pid = map[j].pid;
        int16_t rehash_app_index = map[j].app_index;
        map[j].pid = 0;
        map[j].app_index = -1;
        pid_map_insert(map, rehash_app_index, rehash_pid);
        j = (j + 1) % WM_PID_MAP_SIZE;
      }
      return;
    }
    // empty slot - not found
    if (map[i].pid == 0) {
      return;
    }

    // collision - try next slot
    i = (i + 1) % WM_PID_MAP_SIZE;
  } while (i != start);
}

void wm_state_init(WMState *state) {
  memset(state, 0, sizeof(WMState));
  state->active_buffer = -1; // -1 means no buffer active yet (startup state)
  state->is_passthrough_mode = false;

  // initialize app registry
  for (int i = 0; i < WM_PID_MAP_SIZE; i++) {
    state->app_registry.pid_map[i].app_index = -1;
  }
}

int wm_state_register_app(WMState *state, pid_t pid,
                          const char *bundle_identifier) {
  // validate input
  if (pid == 0 || bundle_identifier == NULL)
    return -1;

  WMAppRegistry *registry = &state->app_registry;

  // check if app already registered
  int16_t existing = pid_map_search(registry->pid_map, pid);
  if (existing != -1)
    return existing;

  // check capacity
  if (registry->app_count >= WM_MAX_APPS)
    return -1;

  // allocate new app
  int16_t index = registry->app_count;
  WMApp *app = &registry->apps[index];
  app->pid = pid;
  app->buffer_index = -1; // unassigned yet
  app->is_managed = true;
  app->is_floating = false;

  // copy bundle identifier safely
  strncpy(app->bundle_identifier, bundle_identifier,
          sizeof(app->bundle_identifier) - 1);
  app->bundle_identifier[sizeof(app->bundle_identifier) - 1] = '\0';

  // add to pid map
  bool inserted = pid_map_insert(registry->pid_map, index, pid);
  assert(inserted && "pid_map_insert failed - map full?");

  registry->app_count++;

  return index;
}

void wm_state_unregister_app(WMState *state, pid_t pid) {
  WMAppRegistry *registry = &state->app_registry;
  int16_t index = pid_map_search(registry->pid_map, pid);

  // check if exists
  if (index < 0)
    return;

  // remove from pid map
  pid_map_remove(registry->pid_map, pid);

  // if pid isn't the last one, move the last one to the empty slot (to avoid
  // holes)
  int16_t last_index = registry->app_count - 1;
  if (index != last_index) {
    // move last app to the empty slot
    WMApp *last_app = &registry->apps[last_index];
    registry->apps[index] = *last_app;

    // update pid map for the app moved
    pid_map_remove(registry->pid_map, last_app->pid);
    bool inserted = pid_map_insert(registry->pid_map, index, last_app->pid);
    assert(inserted && "pid_map_insert failed during unregister swap");
  }

  // clean last slot and decrement app count
  memset(&registry->apps[last_index], 0, sizeof(WMApp));
  registry->app_count--;
}

const WMApp *wm_state_find_app(const WMState *state, pid_t pid) {
  int16_t index = pid_map_search(state->app_registry.pid_map, pid);

  // check if exists
  if (index < 0)
    return NULL;

  return &state->app_registry.apps[index];
}

int8_t wm_state_find_app_index(const WMState *state, pid_t pid) {
  return (int8_t)pid_map_search(state->app_registry.pid_map, pid);
}

void wm_state_assign_to_buffer(WMState *state, pid_t pid, int buffer_index) {
  // validate if buffer index is valid
  if (buffer_index < -1 || buffer_index >= WM_MAX_BUFFERS)
    return;

  // validate if app exists
  int16_t index = pid_map_search(state->app_registry.pid_map, pid);
  if (index < 0)
    return;

  state->app_registry.apps[index].buffer_index = (int8_t)buffer_index;
}

int wm_state_get_buffer_pids(const WMState *state, int buffer_index,
                             pid_t *out_pids, int max_pids) {
  // validate if buffer index is valid
  if (buffer_index < 0 || buffer_index >= WM_MAX_BUFFERS)
    return 0;

  // validate if output array is valid
  if (out_pids == NULL || max_pids <= 0)
    return 0;

  const WMAppRegistry *registry = &state->app_registry;
  int count = 0;

  // linear array scan for pids in the buffer
  for (int i = 0; i < registry->app_count && count < max_pids; i++) {
    if (registry->apps[i].buffer_index == buffer_index) {
      out_pids[count++] = registry->apps[i].pid;
    }
  }

  return count;
}

void wm_state_set_focused(WMState *state, pid_t pid) {
  // find app buffer
  const WMApp *app = wm_state_find_app(state, pid);
  // check if app exists and is assigned to a buffer
  if (app == NULL || app->buffer_index < 0)
    return;

  // update buffer last focused pid
  state->buffers[app->buffer_index].last_focused_pid = pid;
}

void wm_state_set_floating(WMState *state, pid_t pid, bool is_floating) {
  if (!state || pid == 0)
    return;
  int8_t idx = wm_state_find_app_index(state, pid);
  if (idx < 0)
    return;

  state->app_registry.apps[idx].is_floating = is_floating;
}

void wm_state_check_invariants(const WMState *state) {
  const WMAppRegistry *registry = &state->app_registry;

  // check if all apps are assigned to a buffer
  for (int i = 0; i < registry->app_count; i++) {
    pid_t pid = registry->apps[i].pid;
    int16_t found_index = pid_map_search(registry->pid_map, pid);
    assert(found_index == i);
  }

  // buffer_index must be valid for all apps
  for (int i = 0; i < registry->app_count; i++) {
    int8_t buffer_index = registry->apps[i].buffer_index;
    assert(buffer_index >= -1 && buffer_index < WM_MAX_BUFFERS);
  }
}
