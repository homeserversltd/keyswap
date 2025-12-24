#ifndef DEBUG_LOGGER_H
#define DEBUG_LOGGER_H

#include <stdio.h>
#include <linux/input.h>
#include "key-database.h"

// Open debug log file (clobber in place, not append)
// Returns FILE* on success, NULL on error
FILE* debug_log_open(const char *log_path);

// Log an event to the debug file
// Format: [timestamp] device: type=EV_KEY(1) code=BTN_SIDE(275) value=1
void log_event(FILE *fp, struct input_event *ev, const char *device_name);

// Get event type name string
const char* get_event_type_name(int type);

#endif // DEBUG_LOGGER_H