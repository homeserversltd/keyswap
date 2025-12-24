#ifndef EVENT_PROCESSOR_H
#define EVENT_PROCESSOR_H

#include "config-loader.h"
#include "debug-logger.h"
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <linux/input.h>

// Setup device and create libevdev instance
// Returns 0 on success, -1 on error
// Sets *dev and *device_fd on success
int setup_device(const char *device_path, struct libevdev **dev, int *device_fd);

// Setup uinput devices (keyboard for injection, mouse for forwarding)
// Returns 0 on success, -1 on error
int setup_uinput_devices(struct libevdev *dev, struct libevdev_uinput **keyboard, struct libevdev_uinput **mouse, device_config_t *device_cfg);

// Process events loop
// Returns 0 on success, -1 on error
// running_ptr points to flag that controls loop (set to 0 to stop)
int process_events(struct libevdev *dev, device_config_t *device_cfg, config_t *config, 
                   struct libevdev_uinput *keyboard, struct libevdev_uinput *mouse, FILE *debug_fp, int *running_ptr);

// Inject an event to uinput device
void inject_event(struct libevdev_uinput *uinput, int type, int code, int value);

// Forward an event to virtual mouse device
void forward_event(struct libevdev_uinput *mouse, struct input_event *ev);

#endif // EVENT_PROCESSOR_H