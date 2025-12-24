#include "config-loader.h"
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <unistd.h>

// Expand environment variables in path (supports ${VAR:-default} syntax)
char* expand_path(const char *path) {
    if (!path) return NULL;

    char *result = strdup(path);
    if (!result) return NULL;

    // Handle ${VAR:-default} pattern
    regex_t regex;
    regmatch_t matches[2];

    if (regcomp(&regex, "\\$\\{([^}]+)\\}", REG_EXTENDED) == 0) {
        char *temp = result;
        result = (char*)malloc(strlen(temp) * 2 + 1); // Allocate extra space
        if (!result) {
            regfree(&regex);
            free(temp);
            return NULL;
        }
        result[0] = '\0';

        const char *p = temp;
        char *out = result;

        while (regexec(&regex, p, 2, matches, 0) == 0) {
            // Copy text before match
            strncat(out, p, matches[0].rm_so);
            out += matches[0].rm_so;

            // Extract variable expression
            char var_expr[256];
            size_t len = matches[1].rm_eo - matches[1].rm_so;
            if (len < sizeof(var_expr)) {
                strncpy(var_expr, p + matches[1].rm_so, len);
                var_expr[len] = '\0';

                char *value = NULL;
                if (strstr(var_expr, ":-")) {
                    // Handle ${VAR:-default} syntax
                    char *colon = strstr(var_expr, ":-");
                    *colon = '\0';
                    char *var_name = var_expr;
                    char *default_val = colon + 2;
                    value = getenv(var_name);
                    if (!value) value = default_val;
                } else {
                    value = getenv(var_expr);
                }

                if (value) {
                    strcat(out, value);
                    out += strlen(value);
                }
            }

            p += matches[0].rm_eo;
        }

        // Copy remaining text
        strcat(out, p);

        regfree(&regex);
        free(temp);
    }

    return result;
}

// Resolve key name from JSON value (can be string or number)
static int resolve_json_key(json_t *json_key, int *code, int *type) {
    if (json_is_string(json_key)) {
        const char *name = json_string_value(json_key);
        return resolve_key_name(name, code, type);
    } else if (json_is_integer(json_key)) {
        *code = (int)json_integer_value(json_key);
        *type = EV_KEY; // Default assumption
        return 0;
    }
    return -1;
}

config_t* load_config(const char *config_path) {
    json_error_t error;
    json_t *root = json_load_file(config_path, 0, &error);
    
    if (!root) {
        fprintf(stderr, "ERROR: Failed to load config from %s: %s (line %d)\n", 
                config_path, error.text, error.line);
        return NULL;
    }
    
    config_t *config = calloc(1, sizeof(config_t));
    if (!config) {
        json_decref(root);
        return NULL;
    }
    
    // Get paths.debug_log (with expansion)
    json_t *paths = json_object_get(root, "paths");
    if (paths && json_is_object(paths)) {
        json_t *debug_log_json = json_object_get(paths, "debug_log");
        if (debug_log_json && json_is_string(debug_log_json)) {
            char *expanded = expand_path(json_string_value(debug_log_json));
            if (expanded) {
                strncpy(config->debug_log, expanded, sizeof(config->debug_log) - 1);
                free(expanded);
            } else {
                strncpy(config->debug_log, "/tmp/keyswap-debug.log", sizeof(config->debug_log) - 1);
            }
        } else {
            strncpy(config->debug_log, "/tmp/keyswap-debug.log", sizeof(config->debug_log) - 1);
        }
    } else {
        strncpy(config->debug_log, "/tmp/keyswap-debug.log", sizeof(config->debug_log) - 1);
    }
    
    // Get config.debug
    json_t *config_obj = json_object_get(root, "config");
    if (config_obj && json_is_object(config_obj)) {
        json_t *debug_json = json_object_get(config_obj, "debug");
        if (debug_json && json_is_boolean(debug_json)) {
            config->debug = json_is_true(debug_json) ? 1 : 0;
        }
        
        // Get devices array
        json_t *devices_json = json_object_get(config_obj, "devices");
        if (devices_json && json_is_array(devices_json)) {
            size_t device_count = json_array_size(devices_json);
            config->devices = calloc(device_count, sizeof(device_config_t));
            if (!config->devices) {
                json_decref(root);
                free(config);
                return NULL;
            }
            config->device_count = 0;
            
            for (size_t i = 0; i < device_count; i++) {
                json_t *device_json = json_array_get(devices_json, i);
                if (!device_json || !json_is_object(device_json)) continue;
                
                device_config_t *device = &config->devices[config->device_count];
                
                // Get uuid
                json_t *uuid_json = json_object_get(device_json, "uuid");
                if (uuid_json && json_is_string(uuid_json)) {
                    strncpy(device->uuid, json_string_value(uuid_json), sizeof(device->uuid) - 1);
                }
                
                // Get name_match
                json_t *name_match_json = json_object_get(device_json, "name_match");
                if (name_match_json && json_is_string(name_match_json)) {
                    strncpy(device->name_match, json_string_value(name_match_json), sizeof(device->name_match) - 1);
                } else {
                    fprintf(stderr, "WARNING: Device %zu missing name_match\n", i);
                    continue;
                }
                
                // Get remaps array
                json_t *remaps_json = json_object_get(device_json, "remaps");
                if (remaps_json && json_is_array(remaps_json)) {
                    size_t remap_count = json_array_size(remaps_json);
                    device->remaps = calloc(remap_count, sizeof(remap_rule_t));
                    if (!device->remaps) {
                        fprintf(stderr, "ERROR: Failed to allocate remaps\n");
                        continue;
                    }
                    device->remap_count = 0;
                    
                    for (size_t j = 0; j < remap_count; j++) {
                        json_t *remap_json = json_array_get(remaps_json, j);
                        if (!remap_json || !json_is_object(remap_json)) continue;
                        
                        remap_rule_t *remap = &device->remaps[device->remap_count];
                        
                        // Get source
                        json_t *source_json = json_object_get(remap_json, "source");
                        if (!source_json) {
                            fprintf(stderr, "WARNING: Remap %zu in device %zu missing source\n", j, i);
                            continue;
                        }
                        
                        // Get target
                        json_t *target_json = json_object_get(remap_json, "target");
                        if (!target_json) {
                            fprintf(stderr, "WARNING: Remap %zu in device %zu missing target\n", j, i);
                            continue;
                        }
                        
                        // Resolve source name/code
                        if (json_is_string(source_json)) {
                            const char *source_name = json_string_value(source_json);
                            strncpy(remap->source_name, source_name, sizeof(remap->source_name) - 1);
                        } else if (json_is_integer(source_json)) {
                            snprintf(remap->source_name, sizeof(remap->source_name), "%d", (int)json_integer_value(source_json));
                        }
                        
                        // Resolve target name/code
                        if (json_is_string(target_json)) {
                            const char *target_name = json_string_value(target_json);
                            strncpy(remap->target_name, target_name, sizeof(remap->target_name) - 1);
                        } else if (json_is_integer(target_json)) {
                            snprintf(remap->target_name, sizeof(remap->target_name), "%d", (int)json_integer_value(target_json));
                        }
                        
                        // Resolve key codes
                        if (resolve_json_key(source_json, &remap->source_code, &remap->source_type) != 0) {
                            fprintf(stderr, "ERROR: Failed to resolve source key '%s' for device %zu, remap %zu\n",
                                    remap->source_name, i, j);
                            continue;
                        }
                        
                        if (resolve_json_key(target_json, &remap->target_code, &remap->target_type) != 0) {
                            fprintf(stderr, "ERROR: Failed to resolve target key '%s' for device %zu, remap %zu\n",
                                    remap->target_name, i, j);
                            continue;
                        }
                        
                        // Get description (optional)
                        json_t *desc_json = json_object_get(remap_json, "description");
                        if (desc_json && json_is_string(desc_json)) {
                            strncpy(remap->description, json_string_value(desc_json), sizeof(remap->description) - 1);
                        }
                        
                        device->remap_count++;
                    }
                }
                
                config->device_count++;
            }
        }
    }
    
    json_decref(root);
    return config;
}

void config_free(config_t *config) {
    if (!config) return;
    
    for (int i = 0; i < config->device_count; i++) {
        if (config->devices[i].remaps) {
            free(config->devices[i].remaps);
        }
    }
    
    if (config->devices) {
        free(config->devices);
    }
    
    free(config);
}