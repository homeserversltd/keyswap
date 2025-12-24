#include "event-processor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <linux/input.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

int setup_device(const char *device_path, struct libevdev **dev, int *device_fd) {
    if (!device_path || !dev || !device_fd) return -1;
    
    *device_fd = open(device_path, O_RDONLY);
    if (*device_fd < 0) {
        fprintf(stderr, "ERROR: Failed to open device %s: %s\n", device_path, strerror(errno));
        return -1;
    }
    
    int rc = libevdev_new_from_fd(*device_fd, dev);
    if (rc < 0) {
        fprintf(stderr, "ERROR: Failed to create libevdev device: %s\n", strerror(-rc));
        close(*device_fd);
        *device_fd = -1;
        return -1;
    }
    
    printf("Opened device: %s\n", libevdev_get_name(*dev));
    
    // Grab device exclusively
    rc = libevdev_grab(*dev, LIBEVDEV_GRAB);
    if (rc < 0) {
        fprintf(stderr, "WARNING: Could not grab device: %s\n", strerror(-rc));
        fprintf(stderr, "Remapped buttons may still be visible to other applications\n");
    } else {
        printf("Grabbed device exclusively - remapped buttons will be consumed\n");
    }
    
    return 0;
}

int setup_uinput_devices(struct libevdev *dev, struct libevdev_uinput **keyboard, struct libevdev_uinput **mouse, device_config_t *device_cfg) {
    if (!dev || !keyboard || !mouse || !device_cfg) return -1;
    
    // Create keyboard device for key injection
    struct libevdev *keyboard_dev = libevdev_new();
    libevdev_set_name(keyboard_dev, "keyswap-keyboard");
    libevdev_enable_event_type(keyboard_dev, EV_KEY);
    
    // Enable all target keys that might be injected
    for (int i = 0; i < device_cfg->remap_count; i++) {
        if (device_cfg->remaps[i].target_type == EV_KEY) {
            libevdev_enable_event_code(keyboard_dev, EV_KEY, device_cfg->remaps[i].target_code, NULL);
        }
    }
    
    int rc = libevdev_uinput_create_from_device(keyboard_dev, LIBEVDEV_UINPUT_OPEN_MANAGED, keyboard);
    if (rc < 0) {
        fprintf(stderr, "ERROR: Failed to create keyboard uinput device: %s\n", strerror(-rc));
        libevdev_free(keyboard_dev);
        return -1;
    }
    libevdev_free(keyboard_dev);
    printf("Created uinput keyboard device for key injection\n");
    
    // Create virtual mouse/input device to forward other events
    struct libevdev *mouse_dev = libevdev_new();
    libevdev_set_name(mouse_dev, "keyswap-forward");
    
    // Collect all source codes that should be excluded (consumed, not forwarded)
    int excluded_codes[256];
    int excluded_count = 0;
    for (int i = 0; i < device_cfg->remap_count; i++) {
        if (device_cfg->remaps[i].source_type == EV_KEY) {
            excluded_codes[excluded_count++] = device_cfg->remaps[i].source_code;
        }
    }
    
    // Copy capabilities from original device (excluding remapped buttons/keys)
    const struct input_absinfo *absinfo;
    unsigned int code;
    
    // Copy EV_KEY (buttons/keys) - exclude remapped ones
    if (libevdev_has_event_type(dev, EV_KEY)) {
        libevdev_enable_event_type(mouse_dev, EV_KEY);
        for (code = 0; code < KEY_MAX; code++) {
            // Check if this code should be excluded
            int is_excluded = 0;
            for (int i = 0; i < excluded_count; i++) {
                if (excluded_codes[i] == (int)code) {
                    is_excluded = 1;
                    break;
                }
            }
            
            if (libevdev_has_event_code(dev, EV_KEY, code) && !is_excluded) {
                libevdev_enable_event_code(mouse_dev, EV_KEY, code, NULL);
            }
        }
    }
    
    // Copy EV_REL (relative movement)
    if (libevdev_has_event_type(dev, EV_REL)) {
        libevdev_enable_event_type(mouse_dev, EV_REL);
        for (code = 0; code < REL_MAX; code++) {
            if (libevdev_has_event_code(dev, EV_REL, code)) {
                libevdev_enable_event_code(mouse_dev, EV_REL, code, NULL);
            }
        }
    }
    
    // Copy EV_ABS (absolute positioning)
    if (libevdev_has_event_type(dev, EV_ABS)) {
        libevdev_enable_event_type(mouse_dev, EV_ABS);
        for (code = 0; code < ABS_MAX; code++) {
            if (libevdev_has_event_code(dev, EV_ABS, code)) {
                absinfo = libevdev_get_abs_info(dev, code);
                libevdev_enable_event_code(mouse_dev, EV_ABS, code, absinfo);
            }
        }
    }
    
    rc = libevdev_uinput_create_from_device(mouse_dev, LIBEVDEV_UINPUT_OPEN_MANAGED, mouse);
    if (rc < 0) {
        fprintf(stderr, "WARNING: Could not create virtual forward device: %s\n", strerror(-rc));
        fprintf(stderr, "Events will not be forwarded - device may not work normally\n");
        libevdev_free(mouse_dev);
        *mouse = NULL;
    } else {
        printf("Created virtual forward device for forwarding events\n");
    }
    libevdev_free(mouse_dev);
    
    return 0;
}

void inject_event(struct libevdev_uinput *uinput, int type, int code, int value) {
    if (uinput) {
        libevdev_uinput_write_event(uinput, type, code, value);
        libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
    }
}

void forward_event(struct libevdev_uinput *mouse, struct input_event *ev) {
    if (mouse && ev) {
        libevdev_uinput_write_event(mouse, ev->type, ev->code, ev->value);
        libevdev_uinput_write_event(mouse, EV_SYN, SYN_REPORT, 0);
    }
}

// Find remap rule for an event
static remap_rule_t* find_remap_rule(device_config_t *device_cfg, struct input_event *ev) {
    if (!device_cfg || !ev) return NULL;
    
    for (int i = 0; i < device_cfg->remap_count; i++) {
        if (device_cfg->remaps[i].source_type == ev->type && 
            device_cfg->remaps[i].source_code == ev->code) {
            return &device_cfg->remaps[i];
        }
    }
    
    return NULL;
}

int process_events(struct libevdev *dev, device_config_t *device_cfg, config_t *config, 
                   struct libevdev_uinput *keyboard, struct libevdev_uinput *mouse, FILE *debug_fp, int *running_ptr) {
    if (!dev || !device_cfg || !config) return -1;
    
    struct input_event ev;
    int rc;
    const char *device_name = libevdev_get_name(dev);
    
    printf("Processing events for %s\n", device_name);
    printf("Press Ctrl+C to stop\n");
    
    while (running_ptr == NULL || *running_ptr) {
        rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        
        if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
            // Log event if debug enabled
            if (debug_fp && config->debug) {
                log_event(debug_fp, &ev, device_name);
            }
            
            // Check if this event matches a remap rule
            remap_rule_t *remap = find_remap_rule(device_cfg, &ev);
            if (remap) {
                // CONSUME: Don't forward this event
                // INJECT: Send remapped event instead
                inject_event(keyboard, remap->target_type, remap->target_code, ev.value);
            } else {
                // FORWARD: Send event to virtual device
                forward_event(mouse, &ev);
            }
        } else if (rc == LIBEVDEV_READ_STATUS_SYNC) {
            // Handle sync events - forward them
            while (libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev) == LIBEVDEV_READ_STATUS_SUCCESS) {
                forward_event(mouse, &ev);
                if (debug_fp && config->debug) {
                    log_event(debug_fp, &ev, device_name);
                }
            }
        } else if (rc == -EAGAIN) {
            // No event available, continue
            continue;
        } else {
            fprintf(stderr, "ERROR: Failed to read event: %s\n", strerror(-rc));
            break;
        }
    }
    
    return 0;
}

int listen_device(const char *device_path, int *running_ptr) {
    if (!device_path) return -1;
    
    int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "ERROR: Failed to open device %s: %s\n", device_path, strerror(errno));
        return -1;
    }
    
    struct libevdev *dev = NULL;
    int rc = libevdev_new_from_fd(fd, &dev);
    if (rc < 0) {
        fprintf(stderr, "ERROR: Failed to create libevdev device: %s\n", strerror(-rc));
        close(fd);
        return -1;
    }
    
    const char *device_name = libevdev_get_name(dev);
    int vendor_id = libevdev_get_id_vendor(dev);
    int product_id = libevdev_get_id_product(dev);
    const char *device_uniq = libevdev_get_uniq(dev);
    
    // Display device info
    printf("\n=== Listening to device ===\n");
    printf("Path: %s\n", device_path);
    printf("Name: %s\n", device_name ? device_name : "unknown");
    if (vendor_id > 0 && product_id > 0) {
        printf("Identifier: %04x:%04x\n", vendor_id, product_id);
    } else if (device_uniq && strlen(device_uniq) > 0) {
        printf("Identifier: %s\n", device_uniq);
    }
    printf("\n");
    
    // Create a virtual uinput device to forward events to (so device still works)
    struct libevdev *uinput_dev = libevdev_new();
    libevdev_set_name(uinput_dev, "keyswap-listen-forward");
    
    // Copy all capabilities from original device
    const struct input_absinfo *absinfo;
    unsigned int code;
    
    // Copy EV_KEY
    if (libevdev_has_event_type(dev, EV_KEY)) {
        libevdev_enable_event_type(uinput_dev, EV_KEY);
        for (code = 0; code < KEY_MAX; code++) {
            if (libevdev_has_event_code(dev, EV_KEY, code)) {
                libevdev_enable_event_code(uinput_dev, EV_KEY, code, NULL);
            }
        }
    }
    
    // Copy EV_REL
    if (libevdev_has_event_type(dev, EV_REL)) {
        libevdev_enable_event_type(uinput_dev, EV_REL);
        for (code = 0; code < REL_MAX; code++) {
            if (libevdev_has_event_code(dev, EV_REL, code)) {
                libevdev_enable_event_code(uinput_dev, EV_REL, code, NULL);
            }
        }
    }
    
    // Copy EV_ABS
    if (libevdev_has_event_type(dev, EV_ABS)) {
        libevdev_enable_event_type(uinput_dev, EV_ABS);
        for (code = 0; code < ABS_MAX; code++) {
            if (libevdev_has_event_code(dev, EV_ABS, code)) {
                absinfo = libevdev_get_abs_info(dev, code);
                libevdev_enable_event_code(uinput_dev, EV_ABS, code, absinfo);
            }
        }
    }
    
    struct libevdev_uinput *uinput = NULL;
    rc = libevdev_uinput_create_from_device(uinput_dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uinput);
    if (rc < 0) {
        fprintf(stderr, "WARNING: Failed to create virtual device for forwarding: %s\n", strerror(-rc));
        fprintf(stderr, "Events will be displayed but may not work normally\n");
        uinput = NULL;
    }
    libevdev_free(uinput_dev);
    
    // Grab device exclusively so we can read events
    rc = libevdev_grab(dev, LIBEVDEV_GRAB);
    if (rc < 0) {
        fprintf(stderr, "WARNING: Could not grab device: %s\n", strerror(-rc));
        fprintf(stderr, "Events may not be visible if another process is using the device\n");
    } else {
        printf("Device grabbed - events will be forwarded so device continues to work\n");
    }
    
    printf("\nPress buttons/keys on the device to see events...\n");
    printf("Press Ctrl+C to stop\n\n");
    
    struct input_event ev;
    
    while (running_ptr == NULL || *running_ptr) {
        rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        
        if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
            // Forward event to virtual device (so it still works)
            if (uinput) {
                libevdev_uinput_write_event(uinput, ev.type, ev.code, ev.value);
                libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
            }
            
            // Skip SYN events (they're just synchronization, not interesting)
            if (ev.type == EV_SYN) {
                continue;
            }
            
            // Get canonical name for code if available
            const char *canonical_name = NULL;
            if (ev.type == EV_KEY) {
                canonical_name = get_canonical_name(ev.code, ev.type);
            }
            
            // Format event display
            printf("[%s] ", get_event_type_name(ev.type));
            
            if (canonical_name) {
                printf("code=%s(%d)", canonical_name, ev.code);
            } else {
                printf("code=%d", ev.code);
            }
            
            printf(" value=%d", ev.value);
            
            // Add helpful state description for key events
            if (ev.type == EV_KEY) {
                if (ev.value == 1) {
                    printf(" [PRESSED]");
                } else if (ev.value == 0) {
                    printf(" [RELEASED]");
                } else if (ev.value == 2) {
                    printf(" [REPEAT]");
                }
            }
            
            printf("\n");
            fflush(stdout);
        } else if (rc == LIBEVDEV_READ_STATUS_SYNC) {
            // Handle sync events - forward them and skip display
            while (libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev) == LIBEVDEV_READ_STATUS_SUCCESS) {
                if (uinput) {
                    libevdev_uinput_write_event(uinput, ev.type, ev.code, ev.value);
                    libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
                }
            }
        } else if (rc == -EAGAIN) {
            // No event available, continue
            continue;
        } else {
            fprintf(stderr, "ERROR: Failed to read event: %s\n", strerror(-rc));
            break;
        }
    }
    
    // Ungrab device
    libevdev_grab(dev, LIBEVDEV_UNGRAB);
    
    if (uinput) {
        libevdev_uinput_destroy(uinput);
    }
    libevdev_free(dev);
    close(fd);
    
    return 0;
}