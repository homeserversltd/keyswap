#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include "config-loader.h"
#include "device-matcher.h"
#include "event-processor.h"
#include "debug-logger.h"

// Global state for cleanup
static int running = 1;
static config_t *g_config = NULL;
static FILE *g_debug_fp = NULL;
static struct libevdev **g_devices = NULL;
static struct libevdev_uinput **g_keyboards = NULL;
static struct libevdev_uinput **g_mice = NULL;
static int *g_device_fds = NULL;
static int g_device_count = 0;

void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

void cleanup(void) {
    // Close debug log
    if (g_debug_fp) {
        fclose(g_debug_fp);
        g_debug_fp = NULL;
    }
    
    // Cleanup devices
    for (int i = 0; i < g_device_count; i++) {
        if (g_devices && g_devices[i]) {
            libevdev_grab(g_devices[i], LIBEVDEV_UNGRAB);
            libevdev_free(g_devices[i]);
        }
        if (g_keyboards && g_keyboards[i]) {
            libevdev_uinput_destroy(g_keyboards[i]);
        }
        if (g_mice && g_mice[i]) {
            libevdev_uinput_destroy(g_mice[i]);
        }
        if (g_device_fds && g_device_fds[i] >= 0) {
            close(g_device_fds[i]);
        }
    }
    
    if (g_devices) free(g_devices);
    if (g_keyboards) free(g_keyboards);
    if (g_mice) free(g_mice);
    if (g_device_fds) free(g_device_fds);
    
    if (g_config) {
        config_free(g_config);
        g_config = NULL;
    }
}

int main(int argc, char *argv[]) {
    const char *config_path = "index.json";
    
    // Parse command line arguments
    if (argc > 1) {
        config_path = argv[1];
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    atexit(cleanup);
    
    // Load configuration
    g_config = load_config(config_path);
    if (!g_config) {
        fprintf(stderr, "ERROR: Failed to load configuration from %s\n", config_path);
        return 1;
    }
    
    printf("Loaded configuration: %d device(s)\n", g_config->device_count);
    
    // Open debug log if enabled
    if (g_config->debug) {
        g_debug_fp = debug_log_open(g_config->debug_log);
        if (g_debug_fp) {
            printf("Debug logging enabled: %s\n", g_config->debug_log);
        }
    }
    
    // Allocate arrays for device management
    g_devices = calloc(g_config->device_count, sizeof(struct libevdev*));
    g_keyboards = calloc(g_config->device_count, sizeof(struct libevdev_uinput*));
    g_mice = calloc(g_config->device_count, sizeof(struct libevdev_uinput*));
    g_device_fds = calloc(g_config->device_count, sizeof(int));
    
    if (!g_devices || !g_keyboards || !g_mice || !g_device_fds) {
        fprintf(stderr, "ERROR: Failed to allocate device arrays\n");
        return 1;
    }
    
    // Process each device
    for (int i = 0; i < g_config->device_count; i++) {
        device_config_t *device_cfg = &g_config->devices[i];
        char device_path[256];
        
        printf("\nProcessing device: %s (match: %s)\n", device_cfg->uuid, device_cfg->name_match);
        
        // Find matching device
        if (find_matching_device(device_cfg->name_match, device_path, sizeof(device_path)) != 0) {
            fprintf(stderr, "WARNING: Could not find device matching '%s'\n", device_cfg->name_match);
            continue;
        }
        
        printf("Found device at: %s\n", device_path);
        
        // Setup device
        if (setup_device(device_path, &g_devices[g_device_count], &g_device_fds[g_device_count]) != 0) {
            fprintf(stderr, "ERROR: Failed to setup device %s\n", device_path);
            continue;
        }
        
        // Setup uinput devices
        if (setup_uinput_devices(g_devices[g_device_count], &g_keyboards[g_device_count], 
                                  &g_mice[g_device_count], device_cfg) != 0) {
            fprintf(stderr, "ERROR: Failed to setup uinput devices for %s\n", device_path);
            libevdev_free(g_devices[g_device_count]);
            g_devices[g_device_count] = NULL;
            close(g_device_fds[g_device_count]);
            continue;
        }
        
        g_device_count++;
    }
    
    if (g_device_count == 0) {
        fprintf(stderr, "ERROR: No devices successfully configured\n");
        return 1;
    }
    
    printf("\nSuccessfully configured %d device(s)\n", g_device_count);
    printf("Processing events (press Ctrl+C to stop)...\n\n");
    
    // Process events for the first device (for now, single device processing)
    // TODO: Support multiple devices with select/poll
    if (g_device_count > 0) {
        process_events(g_devices[0], &g_config->devices[0], g_config, 
                      g_keyboards[0], g_mice[0], g_debug_fp, &running);
    }
    
    return 0;
}