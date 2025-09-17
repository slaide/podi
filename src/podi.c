#include "podi.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>

static bool podi_initialized = false;

static void ensure_initialized(void) {
    if (!podi_initialized) {
        podi_init_platform();
        podi_initialized = true;
        atexit(podi_cleanup_platform);
    }
}

podi_application *podi_application_create(void) {
    ensure_initialized();
    return podi_platform->application_create();
}

void podi_application_destroy(podi_application *app) {
    if (!app) return;
    podi_platform->application_destroy(app);
}

bool podi_application_should_close(podi_application *app) {
    if (!app) return true;
    return podi_platform->application_should_close(app);
}

void podi_application_close(podi_application *app) {
    if (!app) return;
    podi_platform->application_close(app);
}

bool podi_application_poll_event(podi_application *app, podi_event *event) {
    if (!app || !event) return false;
    return podi_platform->application_poll_event(app, event);
}

podi_window *podi_window_create(podi_application *app, const char *title, int width, int height) {
    if (!app) return NULL;
    return podi_platform->window_create(app, title, width, height);
}

void podi_window_destroy(podi_window *window) {
    if (!window) return;
    podi_platform->window_destroy(window);
}

void podi_window_close(podi_window *window) {
    if (!window) return;
    podi_platform->window_close(window);
}

void podi_window_set_title(podi_window *window, const char *title) {
    if (!window) return;
    podi_platform->window_set_title(window, title);
}

void podi_window_set_size(podi_window *window, int width, int height) {
    if (!window) return;
    podi_platform->window_set_size(window, width, height);
}

void podi_window_get_size(podi_window *window, int *width, int *height) {
    if (!window) return;
    podi_platform->window_get_size(window, width, height);
}

bool podi_window_should_close(podi_window *window) {
    if (!window) return true;
    return podi_platform->window_should_close(window);
}

podi_key podi_translate_native_keycode(uint32_t native_keycode) {
    // This will be implemented by platform-specific code via vtable if needed
    // For now, return UNKNOWN as the platform-specific translation should be done
    // in the event handling itself
    (void)native_keycode;
    return PODI_KEY_UNKNOWN;
}

const char *podi_get_key_name(podi_key key) {
    switch (key) {
        case PODI_KEY_A: return "A";
        case PODI_KEY_B: return "B";
        case PODI_KEY_C: return "C";
        case PODI_KEY_D: return "D";
        case PODI_KEY_E: return "E";
        case PODI_KEY_F: return "F";
        case PODI_KEY_G: return "G";
        case PODI_KEY_H: return "H";
        case PODI_KEY_I: return "I";
        case PODI_KEY_J: return "J";
        case PODI_KEY_K: return "K";
        case PODI_KEY_L: return "L";
        case PODI_KEY_M: return "M";
        case PODI_KEY_N: return "N";
        case PODI_KEY_O: return "O";
        case PODI_KEY_P: return "P";
        case PODI_KEY_Q: return "Q";
        case PODI_KEY_R: return "R";
        case PODI_KEY_S: return "S";
        case PODI_KEY_T: return "T";
        case PODI_KEY_U: return "U";
        case PODI_KEY_V: return "V";
        case PODI_KEY_W: return "W";
        case PODI_KEY_X: return "X";
        case PODI_KEY_Y: return "Y";
        case PODI_KEY_Z: return "Z";
        case PODI_KEY_0: return "0";
        case PODI_KEY_1: return "1";
        case PODI_KEY_2: return "2";
        case PODI_KEY_3: return "3";
        case PODI_KEY_4: return "4";
        case PODI_KEY_5: return "5";
        case PODI_KEY_6: return "6";
        case PODI_KEY_7: return "7";
        case PODI_KEY_8: return "8";
        case PODI_KEY_9: return "9";
        case PODI_KEY_SPACE: return "Space";
        case PODI_KEY_ENTER: return "Enter";
        case PODI_KEY_ESCAPE: return "Escape";
        case PODI_KEY_BACKSPACE: return "Backspace";
        case PODI_KEY_TAB: return "Tab";
        case PODI_KEY_SHIFT: return "Shift";
        case PODI_KEY_CTRL: return "Ctrl";
        case PODI_KEY_ALT: return "Alt";
        case PODI_KEY_UP: return "Up";
        case PODI_KEY_DOWN: return "Down";
        case PODI_KEY_LEFT: return "Left";
        case PODI_KEY_RIGHT: return "Right";
        case PODI_KEY_UNKNOWN:
        default: return "Unknown";
    }
}

const char *podi_get_mouse_button_name(podi_mouse_button button) {
    switch (button) {
        case PODI_MOUSE_BUTTON_LEFT: return "Left";
        case PODI_MOUSE_BUTTON_RIGHT: return "Right";
        case PODI_MOUSE_BUTTON_MIDDLE: return "Middle";
        case PODI_MOUSE_BUTTON_X1: return "X1";
        case PODI_MOUSE_BUTTON_X2: return "X2";
        default: return "Unknown";
    }
}

const char *podi_get_modifiers_string(uint32_t modifiers) {
    static char modifier_buffer[64];
    modifier_buffer[0] = '\0';

    if (modifiers == 0) {
        return "";
    }

    bool first = true;
    if (modifiers & PODI_MOD_CTRL) {
        if (!first) strcat(modifier_buffer, "+");
        strcat(modifier_buffer, "Ctrl");
        first = false;
    }
    if (modifiers & PODI_MOD_SHIFT) {
        if (!first) strcat(modifier_buffer, "+");
        strcat(modifier_buffer, "Shift");
        first = false;
    }
    if (modifiers & PODI_MOD_ALT) {
        if (!first) strcat(modifier_buffer, "+");
        strcat(modifier_buffer, "Alt");
        first = false;
    }
    if (modifiers & PODI_MOD_SUPER) {
        if (!first) strcat(modifier_buffer, "+");
        strcat(modifier_buffer, "Super");
        first = false;
    }

    return modifier_buffer;
}

int podi_main(podi_main_func main_func) {
    if (!main_func) return -1;
    
    podi_application *app = podi_application_create();
    if (!app) return -1;
    
    int result = main_func(app);
    
    podi_application_destroy(app);
    return result;
}