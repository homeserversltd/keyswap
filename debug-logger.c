#include "debug-logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <linux/input.h>

const char* get_event_type_name(int type) {
    switch (type) {
        case EV_SYN: return "EV_SYN";
        case EV_KEY: return "EV_KEY";
        case EV_REL: return "EV_REL";
        case EV_ABS: return "EV_ABS";
        case EV_MSC: return "EV_MSC";
        case EV_SW: return "EV_SW";
        case EV_LED: return "EV_LED";
        case EV_SND: return "EV_SND";
        case EV_REP: return "EV_REP";
        case EV_FF: return "EV_FF";
        case EV_PWR: return "EV_PWR";
        case EV_FF_STATUS: return "EV_FF_STATUS";
        default: return "UNKNOWN";
    }
}

FILE* debug_log_open(const char *log_path) {
    if (!log_path) return NULL;
    
    FILE *fp = fopen(log_path, "w");
    if (!fp) {
        fprintf(stderr, "WARNING: Failed to open debug log %s: %s\n", log_path, strerror(errno));
        return NULL;
    }
    
    return fp;
}

void log_event(FILE *fp, struct input_event *ev, const char *device_name) {
    if (!fp || !ev) return;
    
    // Get timestamp
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    double timestamp = ts.tv_sec + ts.tv_nsec / 1e9;
    
    // Get canonical name for code if available
    const char *canonical_name = NULL;
    if (ev->type == EV_KEY) {
        canonical_name = get_canonical_name(ev->code, ev->type);
    }
    
    // Format: [timestamp] device: type=EV_KEY(1) code=BTN_SIDE(275) value=1
    fprintf(fp, "[%.6f] %s: type=%s(%d) code=", 
            timestamp, device_name ? device_name : "unknown", 
            get_event_type_name(ev->type), ev->type);
    
    if (canonical_name) {
        fprintf(fp, "%s(%d)", canonical_name, ev->code);
    } else {
        fprintf(fp, "%d", ev->code);
    }
    
    fprintf(fp, " value=%d\n", ev->value);
    
    // Flush for real-time viewing
    fflush(fp);
}