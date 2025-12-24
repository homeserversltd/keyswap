#include "device-matcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <glob.h>
#include <ctype.h>
#include <libevdev/libevdev.h>

// Case-insensitive substring search
static int strcasestr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return 0;
    
    size_t needle_len = strlen(needle);
    size_t haystack_len = strlen(haystack);
    
    if (needle_len > haystack_len) return 0;
    
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        int match = 1;
        for (size_t j = 0; j < needle_len; j++) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) {
                match = 0;
                break;
            }
        }
        if (match) return 1;
    }
    
    return 0;
}

int find_matching_device(const char *identifier, const char *name_match, char *device_path, size_t path_size) {
    if ((!identifier || strlen(identifier) == 0) && (!name_match || strlen(name_match) == 0)) {
        return -1;
    }
    if (!device_path || path_size == 0) return -1;
    
    glob_t glob_result;
    memset(&glob_result, 0, sizeof(glob_result));
    
    // Find all /dev/input/event* devices
    int glob_ret = glob("/dev/input/event*", GLOB_NOSORT, NULL, &glob_result);
    if (glob_ret != 0) {
        return -1;
    }
    
    for (size_t i = 0; i < glob_result.gl_pathc; i++) {
        const char *event_path = glob_result.gl_pathv[i];
        
        // Try to open device
        int fd = open(event_path, O_RDONLY);
        if (fd < 0) continue;
        
        struct libevdev *dev = NULL;
        int rc = libevdev_new_from_fd(fd, &dev);
        if (rc < 0) {
            close(fd);
            continue;
        }
        
        // Try to match by identifier first (vendor:product or unique)
        if (identifier && strlen(identifier) > 0) {
            int vendor_id = libevdev_get_id_vendor(dev);
            int product_id = libevdev_get_id_product(dev);
            const char *device_uniq = libevdev_get_uniq(dev);
            
            // Check vendor:product format (e.g., "046d:c08b")
            if (vendor_id > 0 && product_id > 0) {
                char vendor_product[64];
                snprintf(vendor_product, sizeof(vendor_product), "%04x:%04x", vendor_id, product_id);
                if (strcmp(vendor_product, identifier) == 0) {
                    // Found matching device by vendor:product
                    strncpy(device_path, event_path, path_size - 1);
                    device_path[path_size - 1] = '\0';
                    
                    libevdev_free(dev);
                    close(fd);
                    globfree(&glob_result);
                    return 0;
                }
            }
            
            // Check unique identifier
            if (device_uniq && strlen(device_uniq) > 0 && strcmp(device_uniq, identifier) == 0) {
                // Found matching device by unique
                strncpy(device_path, event_path, path_size - 1);
                device_path[path_size - 1] = '\0';
                
                libevdev_free(dev);
                close(fd);
                globfree(&glob_result);
                return 0;
            }
        }
        
        // Fallback to name_match
        if (name_match && strlen(name_match) > 0) {
            const char *device_name = libevdev_get_name(dev);
            if (device_name && strcasestr(device_name, name_match)) {
                // Found matching device by name
                strncpy(device_path, event_path, path_size - 1);
                device_path[path_size - 1] = '\0';
                
                libevdev_free(dev);
                close(fd);
                globfree(&glob_result);
                return 0;
            }
        }
        
        libevdev_free(dev);
        close(fd);
    }
    
    globfree(&glob_result);
    return -1;
}

int count_matching_devices(const char *identifier, const char *name_match) {
    if ((!identifier || strlen(identifier) == 0) && (!name_match || strlen(name_match) == 0)) {
        return -1;
    }
    
    glob_t glob_result;
    memset(&glob_result, 0, sizeof(glob_result));
    
    // Find all /dev/input/event* devices
    int glob_ret = glob("/dev/input/event*", GLOB_NOSORT, NULL, &glob_result);
    if (glob_ret != 0) {
        return -1;
    }
    
    int match_count = 0;
    
    for (size_t i = 0; i < glob_result.gl_pathc; i++) {
        const char *event_path = glob_result.gl_pathv[i];
        
        // Try to open device
        int fd = open(event_path, O_RDONLY);
        if (fd < 0) continue;
        
        struct libevdev *dev = NULL;
        int rc = libevdev_new_from_fd(fd, &dev);
        if (rc < 0) {
            close(fd);
            continue;
        }
        
        int is_match = 0;
        
        // Try to match by identifier first (vendor:product or unique)
        if (identifier && strlen(identifier) > 0) {
            int vendor_id = libevdev_get_id_vendor(dev);
            int product_id = libevdev_get_id_product(dev);
            const char *device_uniq = libevdev_get_uniq(dev);
            
            // Check vendor:product format (e.g., "046d:c08b")
            if (vendor_id > 0 && product_id > 0) {
                char vendor_product[64];
                snprintf(vendor_product, sizeof(vendor_product), "%04x:%04x", vendor_id, product_id);
                if (strcmp(vendor_product, identifier) == 0) {
                    is_match = 1;
                }
            }
            
            // Check unique identifier
            if (!is_match && device_uniq && strlen(device_uniq) > 0 && strcmp(device_uniq, identifier) == 0) {
                is_match = 1;
            }
        }
        
        // Fallback to name_match
        if (!is_match && name_match && strlen(name_match) > 0) {
            const char *device_name = libevdev_get_name(dev);
            if (device_name && strcasestr(device_name, name_match)) {
                is_match = 1;
            }
        }
        
        if (is_match) {
            match_count++;
        }
        
        libevdev_free(dev);
        close(fd);
    }
    
    globfree(&glob_result);
    return match_count;
}

device_config_t* get_device_config(config_t *config, const char *device_name) {
    if (!config || !device_name) return NULL;
    
    for (int i = 0; i < config->device_count; i++) {
        if (strcasestr(device_name, config->devices[i].name_match)) {
            return &config->devices[i];
        }
    }
    
    return NULL;
}

// Check if device should be filtered out (not useful for remapping)
static int should_filter_device(const char *device_name, const char *device_phys, struct libevdev *dev) {
    if (!device_name) return 1;
    
    // Filter out virtual devices created by remapping tools
    if (strstr(device_name, "virtual") || strstr(device_name, "remap")) {
        return 1;
    }
    
    // Filter out audio devices
    if (strstr(device_name, "HD-Audio") || strstr(device_name, "HDA ATI HDMI") || 
        strstr(device_name, "ALSA") || (device_phys && strstr(device_phys, "ALSA"))) {
        return 1;
    }
    
    // Filter out power buttons
    if (strstr(device_name, "Power Button")) {
        return 1;
    }
    
    // Filter out PC Speaker
    if (strstr(device_name, "PC Speaker")) {
        return 1;
    }
    
    // Filter out devices that don't have useful input capabilities
    // We want devices with keys, relative motion (mice), or absolute motion (touchpads)
    if (!libevdev_has_event_type(dev, EV_KEY) && 
        !libevdev_has_event_type(dev, EV_REL) && 
        !libevdev_has_event_type(dev, EV_ABS)) {
        return 1;
    }
    
    return 0;
}

// Simple structure to hold device info for grouping
typedef struct {
    char name[256];
    char identifier[64];  // vendor:product or unique, whichever is available
    char event_path[64];  // /dev/input/event* path
    int has_identifier;
} device_info_t;

// Compare function for qsort (sort by name, then identifier)
static int compare_devices(const void *a, const void *b) {
    const device_info_t *da = (const device_info_t *)a;
    const device_info_t *db = (const device_info_t *)b;
    
    int name_cmp = strcmp(da->name, db->name);
    if (name_cmp != 0) return name_cmp;
    
    // If names are same, sort by identifier (empty identifier comes last)
    if (da->has_identifier && db->has_identifier) {
        return strcmp(da->identifier, db->identifier);
    }
    if (da->has_identifier) return -1;
    if (db->has_identifier) return 1;
    return 0;
}

int list_all_devices(void) {
    glob_t glob_result;
    memset(&glob_result, 0, sizeof(glob_result));
    
    // Find all /dev/input/event* devices
    int glob_ret = glob("/dev/input/event*", GLOB_NOSORT, NULL, &glob_result);
    if (glob_ret != 0) {
        fprintf(stderr, "ERROR: Could not scan /dev/input/event* devices\n");
        return -1;
    }
    
    if (glob_result.gl_pathc == 0) {
        printf("No input devices found.\n");
        globfree(&glob_result);
        return 0;
    }
    
    // Collect all valid devices
    device_info_t *devices = calloc(glob_result.gl_pathc, sizeof(device_info_t));
    if (!devices) {
        fprintf(stderr, "ERROR: Failed to allocate device array\n");
        globfree(&glob_result);
        return -1;
    }
    
    int device_count = 0;
    for (size_t i = 0; i < glob_result.gl_pathc; i++) {
        const char *event_path = glob_result.gl_pathv[i];
        
        // Try to open device
        int fd = open(event_path, O_RDONLY);
        if (fd < 0) continue;
        
        struct libevdev *dev = NULL;
        int rc = libevdev_new_from_fd(fd, &dev);
        if (rc < 0) {
            close(fd);
            continue;
        }
        
        // Get device information
        const char *device_name = libevdev_get_name(dev);
        const char *device_phys = libevdev_get_phys(dev);
        const char *device_uniq = libevdev_get_uniq(dev);
        
        // Filter out non-useful devices
        if (should_filter_device(device_name, device_phys, dev)) {
            libevdev_free(dev);
            close(fd);
            continue;
        }
        
        // Store device info
        if (device_name) {
            strncpy(devices[device_count].name, device_name, sizeof(devices[device_count].name) - 1);
            devices[device_count].name[sizeof(devices[device_count].name) - 1] = '\0';
            
            // Store event path
            strncpy(devices[device_count].event_path, event_path, sizeof(devices[device_count].event_path) - 1);
            devices[device_count].event_path[sizeof(devices[device_count].event_path) - 1] = '\0';
            
            // Get vendor/product IDs (permanent identifier for USB devices)
            int vendor_id = libevdev_get_id_vendor(dev);
            int product_id = libevdev_get_id_product(dev);
            
            // Prefer vendor:product ID (format: "046d:c08b"), fallback to unique
            if (vendor_id > 0 && product_id > 0) {
                snprintf(devices[device_count].identifier, sizeof(devices[device_count].identifier),
                        "%04x:%04x", vendor_id, product_id);
                devices[device_count].has_identifier = 1;
            } else if (device_uniq && strlen(device_uniq) > 0) {
                // Fallback to unique identifier if vendor/product not available
                strncpy(devices[device_count].identifier, device_uniq, sizeof(devices[device_count].identifier) - 1);
                devices[device_count].identifier[sizeof(devices[device_count].identifier) - 1] = '\0';
                devices[device_count].has_identifier = 1;
            } else {
                devices[device_count].identifier[0] = '\0';
                devices[device_count].has_identifier = 0;
            }
            device_count++;
        }
        
        libevdev_free(dev);
        close(fd);
    }
    
    if (device_count == 0) {
        printf("No accessible input devices found.\n");
        free(devices);
        globfree(&glob_result);
        return 0;
    }
    
    // Sort devices by name, then unique
    qsort(devices, device_count, sizeof(device_info_t), compare_devices);
    
    printf("Available input devices:\n\n");
    printf("Use the identifier in brackets for index.json configuration\n");
    printf("Format: vendor:product (e.g., 046d:c08b) or unique identifier\n\n");
    
    // Group and display devices
    const char *current_name = NULL;
    int group_count = 0;
    
    for (int i = 0; i < device_count; i++) {
        // Check if this is a new device name
        if (current_name == NULL || strcmp(current_name, devices[i].name) != 0) {
            // New device group
            if (current_name != NULL) {
                printf("\n");
            }
            current_name = devices[i].name;
            group_count++;
            printf("%s\n", devices[i].name);
        }
        
        // Display event path and identifier (vendor:product or unique)
        printf("  %s", devices[i].event_path);
        if (devices[i].has_identifier) {
            printf(" [%s]\n", devices[i].identifier);
        } else {
            printf(" [no identifier available]\n");
        }
    }
    
    printf("\nTotal: %d device group(s)\n", group_count);
    
    free(devices);
    globfree(&glob_result);
    return 0;
}