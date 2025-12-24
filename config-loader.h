#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include "key-database.h"

// Remap rule structure
typedef struct {
    char source_name[64];
    char target_name[64];
    int source_code;
    int target_code;
    int source_type;
    int target_type;
    char description[128];
} remap_rule_t;

// Device configuration structure
typedef struct {
    char uuid[64];
    char name_match[128];
    remap_rule_t *remaps;
    int remap_count;
} device_config_t;

// Main configuration structure
typedef struct {
    int debug;
    char debug_log[256];
    device_config_t *devices;
    int device_count;
} config_t;

// Load configuration from index.json file
// Returns config_t* on success, NULL on error
// Caller must free with config_free()
config_t* load_config(const char *config_path);

// Free configuration structure
void config_free(config_t *config);

// Expand environment variables in path (supports ${VAR:-default} syntax)
char* expand_path(const char *path);

#endif // CONFIG_LOADER_H