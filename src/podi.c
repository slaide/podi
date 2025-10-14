#include "podi.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

float podi_get_display_scale_factor(podi_application *app) {
    if (!app) return 1.0f;
    return podi_platform->get_display_scale_factor(app);
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

void podi_window_set_position_and_size(podi_window *window, int x, int y, int width, int height) {
    if (!window) return;
    podi_platform->window_set_position_and_size(window, x, y, width, height);
}

void podi_window_get_size(podi_window *window, int *width, int *height) {
    if (!window) return;
    podi_platform->window_get_size(window, width, height);
}

void podi_window_get_framebuffer_size(podi_window *window, int *width, int *height) {
    if (!window) return;
    podi_platform->window_get_framebuffer_size(window, width, height);
}

void podi_window_get_surface_size(podi_window *window, int *width, int *height) {
    if (!window) return;
    podi_platform->window_get_surface_size(window, width, height);
}

float podi_window_get_scale_factor(podi_window *window) {
    if (!window) return 1.0f;
    return podi_platform->window_get_scale_factor(window);
}

bool podi_window_should_close(podi_window *window) {
    if (!window) return true;
    return podi_platform->window_should_close(window);
}

void podi_window_begin_interactive_resize(podi_window *window, int edge) {
    if (!window) return;
    podi_platform->window_begin_interactive_resize(window, edge);
}

void podi_window_begin_move(podi_window *window) {
    if (!window) return;
    podi_platform->window_begin_move(window);
}

void podi_window_set_cursor(podi_window *window, podi_cursor_shape cursor) {
    if (!window) return;
    podi_platform->window_set_cursor(window, cursor);
}

void podi_window_set_cursor_mode(podi_window *window, bool locked, bool visible) {
    if (!window) return;
    podi_platform->window_set_cursor_mode(window, locked, visible);
}

void podi_window_get_cursor_position(podi_window *window, double *x, double *y) {
    if (!window || !x || !y) return;
    podi_platform->window_get_cursor_position(window, x, y);
}

void podi_window_set_fullscreen_exclusive(podi_window *window, bool enabled) {
    if (!window) return;
    if (!podi_platform->window_set_fullscreen_exclusive) return;
    podi_platform->window_set_fullscreen_exclusive(window, enabled);
}

bool podi_window_is_fullscreen_exclusive(podi_window *window) {
    if (!window) return false;
    if (!podi_platform->window_is_fullscreen_exclusive) return false;
    return podi_platform->window_is_fullscreen_exclusive(window);
}

int podi_window_get_title_bar_height(podi_window *window) {
    if (!window) return 0;
    if (!podi_platform->window_get_title_bar_height) return 0;
    return podi_platform->window_get_title_bar_height(window);
}

#ifdef PODI_PLATFORM_LINUX
bool podi_window_get_x11_handles(podi_window *window, podi_x11_handles *handles) {
    if (!window || !handles) return false;
    return podi_platform->window_get_x11_handles(window, handles);
}

bool podi_window_get_wayland_handles(podi_window *window, podi_wayland_handles *handles) {
    if (!window || !handles) return false;
    return podi_platform->window_get_wayland_handles(window, handles);
}
#endif

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

podi_resize_edge podi_detect_resize_edge(podi_window *window, double x, double y) {
    if (!window) return PODI_RESIZE_EDGE_NONE;

    podi_window_common *common = (podi_window_common *)window;
    int border = common->resize_border_width;
    if (border <= 0) border = 8; // Default border width

    double physical_x = x;
    double physical_y = y;
    float scale = common->scale_factor;
    if (scale <= 0) scale = 1.0f;

    int physical_border = (int)(border * scale);
    int physical_width = common->width;
    int physical_height = common->height;

    bool near_left = physical_x < physical_border;
    bool near_right = physical_x > physical_width - physical_border;
    bool near_bottom = physical_y > physical_height - physical_border;
    bool near_top = physical_y < physical_border;

    // Check corners first (they take priority)
    if (near_top && near_left) return PODI_RESIZE_EDGE_TOP_LEFT;
    if (near_top && near_right) return PODI_RESIZE_EDGE_TOP_RIGHT;
    if (near_bottom && near_left) return PODI_RESIZE_EDGE_BOTTOM_LEFT;
    if (near_bottom && near_right) return PODI_RESIZE_EDGE_BOTTOM_RIGHT;

    // Check edges
    if (near_top) return PODI_RESIZE_EDGE_TOP;
    if (near_bottom) return PODI_RESIZE_EDGE_BOTTOM;
    if (near_left) return PODI_RESIZE_EDGE_LEFT;
    if (near_right) return PODI_RESIZE_EDGE_RIGHT;

    return PODI_RESIZE_EDGE_NONE;
}

podi_cursor_shape podi_resize_edge_to_cursor(podi_resize_edge edge) {
    switch (edge) {
        case PODI_RESIZE_EDGE_TOP: return PODI_CURSOR_RESIZE_N;
        case PODI_RESIZE_EDGE_BOTTOM: return PODI_CURSOR_RESIZE_S;
        case PODI_RESIZE_EDGE_LEFT: return PODI_CURSOR_RESIZE_W;
        case PODI_RESIZE_EDGE_RIGHT: return PODI_CURSOR_RESIZE_E;
        case PODI_RESIZE_EDGE_TOP_LEFT: return PODI_CURSOR_RESIZE_NW;
        case PODI_RESIZE_EDGE_TOP_RIGHT: return PODI_CURSOR_RESIZE_NE;
        case PODI_RESIZE_EDGE_BOTTOM_LEFT: return PODI_CURSOR_RESIZE_SW;
        case PODI_RESIZE_EDGE_BOTTOM_RIGHT: return PODI_CURSOR_RESIZE_SE;
        default: return PODI_CURSOR_DEFAULT;
    }
}

bool podi_handle_resize_event(podi_window *window, podi_event *event) {
    if (!window || !event) return false;

    podi_backend_type backend = podi_get_backend();

    // X11: Skip custom resize handling - let WM handle it natively
    if (backend == PODI_BACKEND_X11) {
        return false;
    }

    // Wayland: Handle edge detection and trigger native resize, but don't do custom dragging

    podi_window_common *common = (podi_window_common *)window;

    switch (event->type) {
        case PODI_EVENT_MOUSE_MOVE: {
            double x = event->mouse_move.x;
            double y = event->mouse_move.y;
            common->last_mouse_x = x;
            common->last_mouse_y = y;

            // For Wayland: Only update cursor, don't handle custom dragging
            // The compositor will handle the actual resize via xdg_toplevel_resize
            podi_resize_edge edge = podi_detect_resize_edge(window, x, y);
            podi_cursor_shape cursor = podi_resize_edge_to_cursor(edge);
            podi_window_set_cursor(window, cursor);
            return false; // Let event pass through
        }

        case PODI_EVENT_MOUSE_BUTTON_DOWN: {
            if (event->mouse_button.button == PODI_MOUSE_BUTTON_LEFT) {
                podi_resize_edge edge = podi_detect_resize_edge(window,
                    common->last_mouse_x, common->last_mouse_y);

                if (edge != PODI_RESIZE_EDGE_NONE) {
                    // For Wayland: trigger native resize via compositor
                    podi_window_begin_interactive_resize(window, edge);
                    return true; // Event consumed
                }
            }
            return false; // Let event pass through
        }

        case PODI_EVENT_MOUSE_BUTTON_UP: {
            // For Wayland: no custom resize state to clean up
            return false; // Let event pass through
        }

        default:
            return false; // Let event pass through
    }
}

int podi_main(podi_main_func main_func) {
    if (!main_func) return -1;
    
    podi_application *app = podi_application_create();
    if (!app) return -1;
    
    int result = main_func(app);
    
    podi_application_destroy(app);
    return result;
}
