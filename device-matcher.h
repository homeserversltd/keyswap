#ifndef DEVICE_MATCHER_H
#define DEVICE_MATCHER_H

#include "config-loader.h"
#include <libevdev/libevdev.h>

// Find a device matching the name pattern
// Returns 0 on success (device_path filled), -1 on failure
int find_matching_device(const char *name_match, char *device_path, size_t path_size);

// Get device configuration for a given device name
// Returns device_config_t* if found, NULL otherwise
device_config_t* get_device_config(config_t *config, const char *device_name);

#endif // DEVICE_MATCHER_H