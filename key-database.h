#ifndef KEY_DATABASE_H
#define KEY_DATABASE_H

#include <linux/input.h>

// Maximum number of aliases per key
#define MAX_ALIASES 8

// Key name entry structure
typedef struct {
    const char *name;              // Primary human-readable name
    const char *aliases[MAX_ALIASES]; // Alternative names
    int alias_count;
    int code;                      // Event code (e.g., BTN_SIDE, KEY_ENTER)
    int type;                      // Event type (e.g., EV_KEY)
    const char *canonical_name;    // Canonical Linux name (e.g., "BTN_SIDE", "KEY_ENTER")
} key_name_entry_t;

// Resolve a key name to event code and type
// Returns 0 on success, -1 if not found
// Supports: human-readable names, canonical names (BTN_*, KEY_*), numeric strings
int resolve_key_name(const char *name, int *code, int *type);

// Get canonical name for a code/type pair
// Returns canonical name string, or NULL if not found
const char* get_canonical_name(int code, int type);

#endif // KEY_DATABASE_H