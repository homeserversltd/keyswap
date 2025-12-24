#include "key-database.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <linux/input.h>

// Case-insensitive string comparison
static int my_strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

// Built-in key name database
static const key_name_entry_t key_database[] = {
    // Mouse buttons
    {"back", {"back_button", "side_button", "btn_side"}, 3, BTN_SIDE, EV_KEY, "BTN_SIDE"},
    {"forward", {"forward_button", "extra_button", "btn_extra"}, 3, BTN_EXTRA, EV_KEY, "BTN_EXTRA"},
    {"left_click", {"left_button", "btn_left", "left"}, 3, BTN_LEFT, EV_KEY, "BTN_LEFT"},
    {"right_click", {"right_button", "btn_right", "right"}, 3, BTN_RIGHT, EV_KEY, "BTN_RIGHT"},
    {"middle_click", {"middle_button", "btn_middle", "middle"}, 3, BTN_MIDDLE, EV_KEY, "BTN_MIDDLE"},
    {"task", {"task_button", "btn_task"}, 2, BTN_TASK, EV_KEY, "BTN_TASK"},
    
    // Keyboard keys - special keys
    {"enter", {"return", "key_enter"}, 2, KEY_ENTER, EV_KEY, "KEY_ENTER"},
    {"space", {"key_space", "spc"}, 2, KEY_SPACE, EV_KEY, "KEY_SPACE"},
    {"tab", {"key_tab"}, 1, KEY_TAB, EV_KEY, "KEY_TAB"},
    {"escape", {"esc", "key_esc"}, 2, KEY_ESC, EV_KEY, "KEY_ESC"},
    {"backspace", {"key_backspace", "bs"}, 2, KEY_BACKSPACE, EV_KEY, "KEY_BACKSPACE"},
    {"delete", {"del", "key_delete"}, 2, KEY_DELETE, EV_KEY, "KEY_DELETE"},
    {"caps_lock", {"capslock", "key_capslock"}, 2, KEY_CAPSLOCK, EV_KEY, "KEY_CAPSLOCK"},
    
    // Modifier keys
    {"left_control", {"lctrl", "left_ctrl", "key_leftctrl"}, 3, KEY_LEFTCTRL, EV_KEY, "KEY_LEFTCTRL"},
    {"right_control", {"rctrl", "right_ctrl", "key_rightctrl"}, 3, KEY_RIGHTCTRL, EV_KEY, "KEY_RIGHTCTRL"},
    {"left_shift", {"lshift", "left_shift", "key_leftshift"}, 3, KEY_LEFTSHIFT, EV_KEY, "KEY_LEFTSHIFT"},
    {"right_shift", {"rshift", "right_shift", "key_rightshift"}, 3, KEY_RIGHTSHIFT, EV_KEY, "KEY_RIGHTSHIFT"},
    {"left_alt", {"lalt", "left_alt", "key_leftalt"}, 3, KEY_LEFTALT, EV_KEY, "KEY_LEFTALT"},
    {"right_alt", {"ralt", "right_alt", "key_rightalt"}, 3, KEY_RIGHTALT, EV_KEY, "KEY_RIGHTALT"},
    {"left_super", {"lwin", "left_meta", "left_super", "key_leftmeta"}, 4, KEY_LEFTMETA, EV_KEY, "KEY_LEFTMETA"},
    {"right_super", {"rwin", "right_meta", "right_super", "key_rightmeta"}, 4, KEY_RIGHTMETA, EV_KEY, "KEY_RIGHTMETA"},
    
    // Letter keys a-z
    {"a", {"key_a"}, 1, KEY_A, EV_KEY, "KEY_A"},
    {"b", {"key_b"}, 1, KEY_B, EV_KEY, "KEY_B"},
    {"c", {"key_c"}, 1, KEY_C, EV_KEY, "KEY_C"},
    {"d", {"key_d"}, 1, KEY_D, EV_KEY, "KEY_D"},
    {"e", {"key_e"}, 1, KEY_E, EV_KEY, "KEY_E"},
    {"f", {"key_f"}, 1, KEY_F, EV_KEY, "KEY_F"},
    {"g", {"key_g"}, 1, KEY_G, EV_KEY, "KEY_G"},
    {"h", {"key_h"}, 1, KEY_H, EV_KEY, "KEY_H"},
    {"i", {"key_i"}, 1, KEY_I, EV_KEY, "KEY_I"},
    {"j", {"key_j"}, 1, KEY_J, EV_KEY, "KEY_J"},
    {"k", {"key_k"}, 1, KEY_K, EV_KEY, "KEY_K"},
    {"l", {"key_l"}, 1, KEY_L, EV_KEY, "KEY_L"},
    {"m", {"key_m"}, 1, KEY_M, EV_KEY, "KEY_M"},
    {"n", {"key_n"}, 1, KEY_N, EV_KEY, "KEY_N"},
    {"o", {"key_o"}, 1, KEY_O, EV_KEY, "KEY_O"},
    {"p", {"key_p"}, 1, KEY_P, EV_KEY, "KEY_P"},
    {"q", {"key_q"}, 1, KEY_Q, EV_KEY, "KEY_Q"},
    {"r", {"key_r"}, 1, KEY_R, EV_KEY, "KEY_R"},
    {"s", {"key_s"}, 1, KEY_S, EV_KEY, "KEY_S"},
    {"t", {"key_t"}, 1, KEY_T, EV_KEY, "KEY_T"},
    {"u", {"key_u"}, 1, KEY_U, EV_KEY, "KEY_U"},
    {"v", {"key_v"}, 1, KEY_V, EV_KEY, "KEY_V"},
    {"w", {"key_w"}, 1, KEY_W, EV_KEY, "KEY_W"},
    {"x", {"key_x"}, 1, KEY_X, EV_KEY, "KEY_X"},
    {"y", {"key_y"}, 1, KEY_Y, EV_KEY, "KEY_Y"},
    {"z", {"key_z"}, 1, KEY_Z, EV_KEY, "KEY_Z"},
    
    // Number keys 0-9
    {"0", {"key_0"}, 1, KEY_0, EV_KEY, "KEY_0"},
    {"1", {"key_1"}, 1, KEY_1, EV_KEY, "KEY_1"},
    {"2", {"key_2"}, 1, KEY_2, EV_KEY, "KEY_2"},
    {"3", {"key_3"}, 1, KEY_3, EV_KEY, "KEY_3"},
    {"4", {"key_4"}, 1, KEY_4, EV_KEY, "KEY_4"},
    {"5", {"key_5"}, 1, KEY_5, EV_KEY, "KEY_5"},
    {"6", {"key_6"}, 1, KEY_6, EV_KEY, "KEY_6"},
    {"7", {"key_7"}, 1, KEY_7, EV_KEY, "KEY_7"},
    {"8", {"key_8"}, 1, KEY_8, EV_KEY, "KEY_8"},
    {"9", {"key_9"}, 1, KEY_9, EV_KEY, "KEY_9"},
    
    // Function keys F1-F12
    {"f1", {"key_f1"}, 1, KEY_F1, EV_KEY, "KEY_F1"},
    {"f2", {"key_f2"}, 1, KEY_F2, EV_KEY, "KEY_F2"},
    {"f3", {"key_f3"}, 1, KEY_F3, EV_KEY, "KEY_F3"},
    {"f4", {"key_f4"}, 1, KEY_F4, EV_KEY, "KEY_F4"},
    {"f5", {"key_f5"}, 1, KEY_F5, EV_KEY, "KEY_F5"},
    {"f6", {"key_f6"}, 1, KEY_F6, EV_KEY, "KEY_F6"},
    {"f7", {"key_f7"}, 1, KEY_F7, EV_KEY, "KEY_F7"},
    {"f8", {"key_f8"}, 1, KEY_F8, EV_KEY, "KEY_F8"},
    {"f9", {"key_f9"}, 1, KEY_F9, EV_KEY, "KEY_F9"},
    {"f10", {"key_f10"}, 1, KEY_F10, EV_KEY, "KEY_F10"},
    {"f11", {"key_f11"}, 1, KEY_F11, EV_KEY, "KEY_F11"},
    {"f12", {"key_f12"}, 1, KEY_F12, EV_KEY, "KEY_F12"},
    
    // Arrow keys
    {"up", {"up_arrow", "key_up"}, 2, KEY_UP, EV_KEY, "KEY_UP"},
    {"down", {"down_arrow", "key_down"}, 2, KEY_DOWN, EV_KEY, "KEY_DOWN"},
    {"left", {"left_arrow", "key_left"}, 2, KEY_LEFT, EV_KEY, "KEY_LEFT"},
    {"right", {"right_arrow", "key_right"}, 2, KEY_RIGHT, EV_KEY, "KEY_RIGHT"},
};

#define KEY_DATABASE_SIZE (sizeof(key_database) / sizeof(key_database[0]))

// Resolve canonical Linux name to code/type
static int resolve_canonical_name(const char *name, int *code, int *type) {
    // Try BTN_* names
    if (my_strcasecmp(name, "BTN_LEFT") == 0) { *code = BTN_LEFT; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "BTN_RIGHT") == 0) { *code = BTN_RIGHT; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "BTN_MIDDLE") == 0) { *code = BTN_MIDDLE; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "BTN_SIDE") == 0) { *code = BTN_SIDE; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "BTN_EXTRA") == 0) { *code = BTN_EXTRA; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "BTN_TASK") == 0) { *code = BTN_TASK; *type = EV_KEY; return 0; }
    
    // Try KEY_* names - common ones
    if (my_strcasecmp(name, "KEY_ENTER") == 0) { *code = KEY_ENTER; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_SPACE") == 0) { *code = KEY_SPACE; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_TAB") == 0) { *code = KEY_TAB; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_ESC") == 0) { *code = KEY_ESC; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_BACKSPACE") == 0) { *code = KEY_BACKSPACE; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_DELETE") == 0) { *code = KEY_DELETE; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_CAPSLOCK") == 0) { *code = KEY_CAPSLOCK; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_LEFTCTRL") == 0) { *code = KEY_LEFTCTRL; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_RIGHTCTRL") == 0) { *code = KEY_RIGHTCTRL; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_LEFTSHIFT") == 0) { *code = KEY_LEFTSHIFT; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_RIGHTSHIFT") == 0) { *code = KEY_RIGHTSHIFT; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_LEFTALT") == 0) { *code = KEY_LEFTALT; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_RIGHTALT") == 0) { *code = KEY_RIGHTALT; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_LEFTMETA") == 0) { *code = KEY_LEFTMETA; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_RIGHTMETA") == 0) { *code = KEY_RIGHTMETA; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_UP") == 0) { *code = KEY_UP; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_DOWN") == 0) { *code = KEY_DOWN; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_LEFT") == 0) { *code = KEY_LEFT; *type = EV_KEY; return 0; }
    if (my_strcasecmp(name, "KEY_RIGHT") == 0) { *code = KEY_RIGHT; *type = EV_KEY; return 0; }
    
    // Try KEY_A through KEY_Z
    if (strlen(name) == 5 && name[0] == 'K' && name[1] == 'E' && name[2] == 'Y' && name[3] == '_') {
        char letter = toupper((unsigned char)name[4]);
        if (letter >= 'A' && letter <= 'Z') {
            *code = KEY_A + (letter - 'A');
            *type = EV_KEY;
            return 0;
        }
    }
    
    // Try KEY_0 through KEY_9
    if (strlen(name) == 5 && name[0] == 'K' && name[1] == 'E' && name[2] == 'Y' && name[3] == '_') {
        char digit = name[4];
        if (digit >= '0' && digit <= '9') {
            *code = KEY_0 + (digit - '0');
            *type = EV_KEY;
            return 0;
        }
    }
    
    // Try KEY_F1 through KEY_F12
    if (strlen(name) == 6 && name[0] == 'K' && name[1] == 'E' && name[2] == 'Y' && name[3] == '_' && name[4] == 'F') {
        char digit = name[5];
        if (digit >= '1' && digit <= '9') {
            *code = KEY_F1 + (digit - '1');
            *type = EV_KEY;
            return 0;
        }
        if (strlen(name) == 7 && name[5] == '1') {
            if (name[6] == '0') { *code = KEY_F10; *type = EV_KEY; return 0; }
            if (name[6] == '1') { *code = KEY_F11; *type = EV_KEY; return 0; }
            if (name[6] == '2') { *code = KEY_F12; *type = EV_KEY; return 0; }
        }
    }
    
    return -1;
}

int resolve_key_name(const char *name, int *code, int *type) {
    if (!name || !code || !type) return -1;
    
    // 1. Try human-readable database lookup (case-insensitive)
    for (size_t i = 0; i < KEY_DATABASE_SIZE; i++) {
        // Check primary name
        if (my_strcasecmp(name, key_database[i].name) == 0) {
            *code = key_database[i].code;
            *type = key_database[i].type;
            return 0;
        }
        
        // Check aliases
        for (int j = 0; j < key_database[i].alias_count; j++) {
            if (key_database[i].aliases[j] && my_strcasecmp(name, key_database[i].aliases[j]) == 0) {
                *code = key_database[i].code;
                *type = key_database[i].type;
                return 0;
            }
        }
    }
    
    // 2. Try canonical Linux key names (BTN_*, KEY_*)
    if (resolve_canonical_name(name, code, type) == 0) {
        return 0;
    }
    
    // 3. Try numeric string parsing
    char *endptr;
    long num = strtol(name, &endptr, 10);
    if (*endptr == '\0' && num >= 0 && num <= 0xFFFF) {
        *code = (int)num;
        *type = EV_KEY;  // Default assumption
        return 0;
    }
    
    return -1;  // Not found
}

const char* get_canonical_name(int code, int type) {
    if (type != EV_KEY) return NULL;
    
    // Search database for matching code
    for (size_t i = 0; i < KEY_DATABASE_SIZE; i++) {
        if (key_database[i].code == code && key_database[i].type == type) {
            return key_database[i].canonical_name;
        }
    }
    
    return NULL;
}