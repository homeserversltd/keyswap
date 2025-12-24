#ifndef DEVICE_MATCHER_H
#define DEVICE_MATCHER_H

#include "config-loader.h"
#include <libevdev/libevdev.h>

// Find a device matching identifier (vendor:product or unique) or name pattern
// Returns 0 on success (device_path filled), -1 on failure
// Prefers identifier match, falls back to name_match if identifier is empty
int find_matching_device(const char *identifier, const char *name_match, char *device_path, size_t path_size);

// Count how many devices match the given identifier or name pattern
// Returns the number of matching devices, -1 on error
int count_matching_devices(const char *identifier, const char *name_match);

// Get device configuration for a given device name
// Returns device_config_t* if found, NULL otherwise
device_config_t* get_device_config(config_t *config, const char *device_name);

// List all available input devices
// Prints device information to stdout
// Returns 0 on success, -1 on error
int list_all_devices(void);

#endif // DEVICE_MATCHER_H
