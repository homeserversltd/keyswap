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

int find_matching_device(const char *name_match, char *device_path, size_t path_size) {
    if (!name_match || !device_path || path_size == 0) return -1;
    
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
        
        // Get device name
        const char *device_name = libevdev_get_name(dev);
        if (device_name && strcasestr(device_name, name_match)) {
            // Found matching device
            strncpy(device_path, event_path, path_size - 1);
            device_path[path_size - 1] = '\0';
            
            libevdev_free(dev);
            close(fd);
            globfree(&glob_result);
            return 0;
        }
        
        libevdev_free(dev);
        close(fd);
    }
    
    globfree(&glob_result);
    return -1;
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