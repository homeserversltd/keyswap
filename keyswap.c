#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
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

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] [CONFIG_FILE]\n", program_name);
    printf("\n");
    printf("Options:\n");
    printf("  -l, --list          List all available input devices\n");
    printf("  -L, --listen [ID]   Listen/monitor mode: display events from device(s)\n");
    printf("                      If ID (vendor:product hex) provided, monitor that device\n");
    printf("                      If no ID, monitor all devices from config file\n");
    printf("  -r, --run FILE      Run key mapper with specified config file (full path)\n");
    printf("  -h, --help          Show this help message\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  CONFIG_FILE         Path to configuration file (default: index.json)\n");
    printf("                      Note: Use --run/-r to explicitly specify config file\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --listen           # Monitor all devices from config\n", program_name);
    printf("  %s --listen 046d:c08b # Monitor specific device by vendor:product\n", program_name);
    printf("\n");
}

int main(int argc, char *argv[]) {
    const char *config_path = "index.json";
    int list_devices = 0;
    int listen_mode = 0;
    const char *listen_identifier = NULL;
    int run_specified = 0;
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"list", no_argument, 0, 'l'},
        {"listen", optional_argument, 0, 'L'},
        {"run", required_argument, 0, 'r'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "lL::r:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'l':
                list_devices = 1;
                break;
            case 'L':
                listen_mode = 1;
                if (optarg) {
                    listen_identifier = optarg;
                }
                break;
            case 'r':
                config_path = optarg;
                run_specified = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Handle --listen with identifier as non-option argument (--listen 046d:c08b format)
    if (listen_mode && !listen_identifier && optind < argc) {
        listen_identifier = argv[optind];
        optind++;
    }
    
    // Handle non-option arguments (config file path) - only if --run wasn't specified and not in listen mode
    if (!run_specified && !listen_mode && optind < argc) {
        config_path = argv[optind];
    }
    
    // Check for root privileges (required for device access)
    if (geteuid() != 0) {
        fprintf(stderr, "WARNING: Not running as root. Device access may be limited.\n");
        fprintf(stderr, "Some devices may not be accessible. Consider running with sudo.\n\n");
    }
    
    // Handle --list command
    if (list_devices) {
        return list_all_devices() == 0 ? 0 : 1;
    }
    
    // Handle --listen command
    if (listen_mode) {
        signal(SIGINT, signal_handler);
        
        if (listen_identifier) {
            // Monitor specific device by identifier
            char device_path[256];
            if (find_matching_device(listen_identifier, "", device_path, sizeof(device_path)) != 0) {
                fprintf(stderr, "ERROR: Could not find device with identifier '%s'\n", listen_identifier);
                fprintf(stderr, "Use --list to see available devices\n");
                return 1;
            }
            
            return listen_device(device_path, &running) == 0 ? 0 : 1;
        } else {
            // Monitor all devices from config file
            config_t *config = load_config(config_path);
            if (!config) {
                fprintf(stderr, "ERROR: Failed to load configuration from %s\n", config_path);
                fprintf(stderr, "Use --listen <identifier> to monitor a specific device\n");
                return 1;
            }
            
            if (config->device_count == 0) {
                fprintf(stderr, "ERROR: No devices configured in %s\n", config_path);
                fprintf(stderr, "Use --listen <identifier> to monitor a specific device\n");
                config_free(config);
                return 1;
            }
            
            // For now, monitor the first device (multi-device monitoring would require select/poll)
            // In the future, we could enhance this to monitor all devices
            if (config->device_count > 1) {
                printf("Note: Multiple devices in config. Monitoring first device only.\n");
                printf("Use --listen <identifier> to monitor a specific device.\n\n");
            }
            
            device_config_t *device_cfg = &config->devices[0];
            char device_path[256];
            
            const char *match_str = strlen(device_cfg->identifier) > 0 ? device_cfg->identifier : device_cfg->name_match;
            if (find_matching_device(device_cfg->identifier, device_cfg->name_match, device_path, sizeof(device_path)) != 0) {
                fprintf(stderr, "ERROR: Could not find device matching '%s'\n", match_str);
                config_free(config);
                return 1;
            }
            
            int ret = listen_device(device_path, &running);
            config_free(config);
            return ret == 0 ? 0 : 1;
        }
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
        
        printf("\nProcessing device: %s", device_cfg->uuid);
        if (strlen(device_cfg->identifier) > 0) {
            printf(" (identifier: %s)", device_cfg->identifier);
        } else if (strlen(device_cfg->name_match) > 0) {
            printf(" (name: %s)", device_cfg->name_match);
        }
        printf("\n");
        
        // Find matching device (prefers identifier, falls back to name_match)
        if (find_matching_device(device_cfg->identifier, device_cfg->name_match, device_path, sizeof(device_path)) != 0) {
            const char *match_str = strlen(device_cfg->identifier) > 0 ? device_cfg->identifier : device_cfg->name_match;
            fprintf(stderr, "WARNING: Could not find device matching '%s'\n", match_str);
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