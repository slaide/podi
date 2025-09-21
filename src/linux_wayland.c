#include "internal.h"
#include "podi.h"
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <wayland-cursor.h>
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-client-protocol.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <linux/input-event-codes.h>
#include <locale.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>

typedef struct {
    podi_application_common common;
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;
    struct xdg_wm_base *xdg_wm_base;
    struct zxdg_decoration_manager_v1 *decoration_manager;
    struct wl_shm *shm;
    uint32_t seat_capabilities;
    uint32_t modifier_state;

    // XKB context for keyboard handling
    struct xkb_context *xkb_context;
    struct xkb_keymap *xkb_keymap;
    struct xkb_state *xkb_state;

    // Compose support for dead keys
    struct xkb_compose_table *compose_table;
    struct xkb_compose_state *compose_state;

    // Output scale tracking
    struct wl_output **outputs;
    int32_t *output_scales;
    size_t output_count;
    size_t output_capacity;
    int32_t max_scale;
    uint32_t last_input_serial;

    // Cursor theme support
    struct wl_cursor_theme *cursor_theme;
    struct wl_surface *cursor_surface;
} podi_application_wayland;

typedef struct {
    podi_window_common common;
    podi_application_wayland *app;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct zxdg_toplevel_decoration_v1 *decoration;
    bool configured;
    uint32_t last_input_serial;

    // Client-side decoration support
    bool has_server_decorations;
    double last_mouse_x, last_mouse_y;
} podi_window_wayland;

// Forward declarations
static void wayland_window_begin_move(podi_window *window_generic);

static uint32_t wayland_mods_to_podi_modifiers(uint32_t mods_depressed) {
    uint32_t modifiers = 0;
    if (mods_depressed & 1) modifiers |= PODI_MOD_SHIFT;  // Shift
    if (mods_depressed & 4) modifiers |= PODI_MOD_CTRL;   // Control
    if (mods_depressed & 8) modifiers |= PODI_MOD_ALT;    // Alt
    if (mods_depressed & 64) modifiers |= PODI_MOD_SUPER; // Super/Meta
    return modifiers;
}

static podi_key wayland_keycode_to_podi_key(uint32_t key) {
    switch (key) {
        case KEY_A: return PODI_KEY_A;
        case KEY_B: return PODI_KEY_B;
        case KEY_C: return PODI_KEY_C;
        case KEY_D: return PODI_KEY_D;
        case KEY_E: return PODI_KEY_E;
        case KEY_F: return PODI_KEY_F;
        case KEY_G: return PODI_KEY_G;
        case KEY_H: return PODI_KEY_H;
        case KEY_I: return PODI_KEY_I;
        case KEY_J: return PODI_KEY_J;
        case KEY_K: return PODI_KEY_K;
        case KEY_L: return PODI_KEY_L;
        case KEY_M: return PODI_KEY_M;
        case KEY_N: return PODI_KEY_N;
        case KEY_O: return PODI_KEY_O;
        case KEY_P: return PODI_KEY_P;
        case KEY_Q: return PODI_KEY_Q;
        case KEY_R: return PODI_KEY_R;
        case KEY_S: return PODI_KEY_S;
        case KEY_T: return PODI_KEY_T;
        case KEY_U: return PODI_KEY_U;
        case KEY_V: return PODI_KEY_V;
        case KEY_W: return PODI_KEY_W;
        case KEY_X: return PODI_KEY_X;
        case KEY_Y: return PODI_KEY_Y;
        case KEY_Z: return PODI_KEY_Z;
        case KEY_0: return PODI_KEY_0;
        case KEY_1: return PODI_KEY_1;
        case KEY_2: return PODI_KEY_2;
        case KEY_3: return PODI_KEY_3;
        case KEY_4: return PODI_KEY_4;
        case KEY_5: return PODI_KEY_5;
        case KEY_6: return PODI_KEY_6;
        case KEY_7: return PODI_KEY_7;
        case KEY_8: return PODI_KEY_8;
        case KEY_9: return PODI_KEY_9;
        case KEY_SPACE: return PODI_KEY_SPACE;
        case KEY_ENTER: return PODI_KEY_ENTER;
        case KEY_ESC: return PODI_KEY_ESCAPE;
        case KEY_BACKSPACE: return PODI_KEY_BACKSPACE;
        case KEY_TAB: return PODI_KEY_TAB;
        case KEY_LEFTSHIFT: case KEY_RIGHTSHIFT: return PODI_KEY_SHIFT;
        case KEY_LEFTCTRL: case KEY_RIGHTCTRL: return PODI_KEY_CTRL;
        case KEY_LEFTALT: case KEY_RIGHTALT: return PODI_KEY_ALT;
        case KEY_UP: return PODI_KEY_UP;
        case KEY_DOWN: return PODI_KEY_DOWN;
        case KEY_LEFT: return PODI_KEY_LEFT;
        case KEY_RIGHT: return PODI_KEY_RIGHT;
        default: return PODI_KEY_UNKNOWN;
    }
}

#define MAX_PENDING_EVENTS 32
static podi_event pending_events[MAX_PENDING_EVENTS];
static size_t pending_event_count = 0;

static void add_pending_event(const podi_event *event) {
    if (pending_event_count < MAX_PENDING_EVENTS) {
        pending_events[pending_event_count++] = *event;
    }
}

static bool get_pending_event(podi_event *event) {
    if (pending_event_count > 0) {
        *event = pending_events[0];
        // Shift remaining events
        for (size_t i = 1; i < pending_event_count; i++) {
            pending_events[i-1] = pending_events[i];
        }
        pending_event_count--;
        return true;
    }
    return false;
}

static void keyboard_keymap(void *data,
                           struct wl_keyboard *keyboard __attribute__((unused)),
                           uint32_t format, int fd, uint32_t size) {
    podi_application_wayland *app = (podi_application_wayland *)data;

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    char *keymap_string = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (keymap_string == MAP_FAILED) {
        close(fd);
        return;
    }

    // Clean up previous keymap and state
    if (app->xkb_state) {
        xkb_state_unref(app->xkb_state);
        app->xkb_state = NULL;
    }
    if (app->xkb_keymap) {
        xkb_keymap_unref(app->xkb_keymap);
        app->xkb_keymap = NULL;
    }

    // Create new keymap and state
    app->xkb_keymap = xkb_keymap_new_from_string(app->xkb_context, keymap_string,
                                                XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (app->xkb_keymap) {
        app->xkb_state = xkb_state_new(app->xkb_keymap);
    }

    munmap(keymap_string, size);
    close(fd);
}

static void keyboard_enter(void *data, struct wl_keyboard *keyboard __attribute__((unused)),
                         uint32_t serial __attribute__((unused)), struct wl_surface *surface,
                         struct wl_array *keys __attribute__((unused))) {
    podi_application_wayland *app = (podi_application_wayland *)data;

    for (size_t i = 0; i < app->common.window_count; i++) {
        podi_window_wayland *window = (podi_window_wayland *)app->common.windows[i];
        if (window && window->surface == surface) {
            podi_event event = {0};
            event.type = PODI_EVENT_WINDOW_FOCUS;
            event.window = (podi_window *)window;
            add_pending_event(&event);
            break;
        }
    }
}

static void keyboard_leave(void *data, struct wl_keyboard *keyboard __attribute__((unused)),
                         uint32_t serial __attribute__((unused)), struct wl_surface *surface) {
    podi_application_wayland *app = (podi_application_wayland *)data;
    
    for (size_t i = 0; i < app->common.window_count; i++) {
        podi_window_wayland *window = (podi_window_wayland *)app->common.windows[i];
        if (window && window->surface == surface) {
            podi_event event = {0};
            event.type = PODI_EVENT_WINDOW_UNFOCUS;
            event.window = (podi_window *)window;
            add_pending_event(&event);
            break;
        }
    }
}

static void keyboard_key(void *data, struct wl_keyboard *keyboard __attribute__((unused)),
                       uint32_t serial, uint32_t time __attribute__((unused)), uint32_t key,
                       uint32_t state) {
    podi_application_wayland *app = (podi_application_wayland *)data;
    app->last_input_serial = serial;
    podi_event_type event_type = (state == WL_KEYBOARD_KEY_STATE_PRESSED)
                                ? PODI_EVENT_KEY_DOWN : PODI_EVENT_KEY_UP;

    podi_event event = {0};
    event.type = event_type;
    event.window = app->common.window_count > 0 ? app->common.windows[0] : NULL;
    event.key.key = wayland_keycode_to_podi_key(key);
    event.key.native_keycode = key;
    event.key.modifiers = app->modifier_state;
    
    // XKB-based text input with compose support
    static char text_buffer[64];
    text_buffer[0] = '\0';
    event.key.text = NULL;

    if (event_type == PODI_EVENT_KEY_DOWN && app->xkb_state) {
        // Update XKB state with this key press
        xkb_keycode_t keycode = key + 8; // Wayland keycodes are offset by 8
        xkb_state_update_key(app->xkb_state, keycode, XKB_KEY_DOWN);

        // Try compose first (for dead key sequences)
        if (app->compose_state) {
            xkb_keysym_t keysym = xkb_state_key_get_one_sym(app->xkb_state, keycode);
            xkb_compose_state_feed(app->compose_state, keysym);

            enum xkb_compose_status status = xkb_compose_state_get_status(app->compose_state);
            switch (status) {
                case XKB_COMPOSE_COMPOSING:
                    // Sequence in progress, don't output text yet
                    break;
                case XKB_COMPOSE_COMPOSED: {
                    // Sequence complete, get composed text
                    int len = xkb_compose_state_get_utf8(app->compose_state, text_buffer, sizeof(text_buffer));
                    if (len > 0) {
                        text_buffer[len] = '\0';
                        event.key.text = text_buffer;
                    }
                    xkb_compose_state_reset(app->compose_state);
                    break;
                }
                case XKB_COMPOSE_CANCELLED:
                    // Sequence cancelled, reset and fall through to normal processing
                    xkb_compose_state_reset(app->compose_state);
                    // fallthrough
                case XKB_COMPOSE_NOTHING:
                default:
                    // No compose sequence, get text from keymap directly
                    int len = xkb_state_key_get_utf8(app->xkb_state, keycode, text_buffer, sizeof(text_buffer));
                    if (len > 0) {
                        text_buffer[len] = '\0';
                        event.key.text = text_buffer;
                    }
                    break;
            }
        } else {
            // No compose support, use direct keymap text
            int len = xkb_state_key_get_utf8(app->xkb_state, keycode, text_buffer, sizeof(text_buffer));
            if (len > 0) {
                text_buffer[len] = '\0';
                event.key.text = text_buffer;
            }
        }
    } else if (event_type == PODI_EVENT_KEY_UP && app->xkb_state) {
        // Update XKB state for key release
        xkb_keycode_t keycode = key + 8;
        xkb_state_update_key(app->xkb_state, keycode, XKB_KEY_UP);
    }
    
    add_pending_event(&event);
}

static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard __attribute__((unused)),
                             uint32_t serial __attribute__((unused)), uint32_t mods_depressed,
                             uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
    podi_application_wayland *app = (podi_application_wayland *)data;
    app->modifier_state = wayland_mods_to_podi_modifiers(mods_depressed);

    // Update XKB state with modifier information
    if (app->xkb_state) {
        xkb_state_update_mask(app->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
    }
}

static void keyboard_repeat_info(void *data __attribute__((unused)), struct wl_keyboard *keyboard __attribute__((unused)),
                               int32_t rate __attribute__((unused)), int32_t delay __attribute__((unused))) {
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_keymap,
    keyboard_enter,
    keyboard_leave,
    keyboard_key,
    keyboard_modifiers,
    keyboard_repeat_info,
};

static void pointer_enter(void *data, struct wl_pointer *pointer __attribute__((unused)),
                        uint32_t serial __attribute__((unused)), struct wl_surface *surface,
                        wl_fixed_t sx __attribute__((unused)), wl_fixed_t sy __attribute__((unused))) {
    podi_application_wayland *app = (podi_application_wayland *)data;

    for (size_t i = 0; i < app->common.window_count; i++) {
        podi_window_wayland *window = (podi_window_wayland *)app->common.windows[i];
        if (window && window->surface == surface) {
            podi_event event = {0};
            event.type = PODI_EVENT_MOUSE_ENTER;
            event.window = (podi_window *)window;
            add_pending_event(&event);
            break;
        }
    }
}

static void pointer_leave(void *data, struct wl_pointer *pointer __attribute__((unused)),
                        uint32_t serial __attribute__((unused)), struct wl_surface *surface) {
    podi_application_wayland *app = (podi_application_wayland *)data;

    for (size_t i = 0; i < app->common.window_count; i++) {
        podi_window_wayland *window = (podi_window_wayland *)app->common.windows[i];
        if (window && window->surface == surface) {
            podi_event event = {0};
            event.type = PODI_EVENT_MOUSE_LEAVE;
            event.window = (podi_window *)window;
            add_pending_event(&event);
            break;
        }
    }
}

static void pointer_motion(void *data, struct wl_pointer *pointer __attribute__((unused)),
                         uint32_t time __attribute__((unused)), wl_fixed_t sx, wl_fixed_t sy) {
    podi_application_wayland *app = (podi_application_wayland *)data;

    // Track mouse position in the current window for title bar detection
    if (app->common.window_count > 0) {
        podi_window_wayland *window = (podi_window_wayland *)app->common.windows[0];
        window->last_mouse_x = wl_fixed_to_double(sx);
        window->last_mouse_y = wl_fixed_to_double(sy);
    }

    podi_event event = {0};
    event.type = PODI_EVENT_MOUSE_MOVE;
    event.window = app->common.window_count > 0 ? app->common.windows[0] : NULL;
    event.mouse_move.x = wl_fixed_to_double(sx);
    event.mouse_move.y = wl_fixed_to_double(sy);
    add_pending_event(&event);
}

static void pointer_button(void *data, struct wl_pointer *pointer __attribute__((unused)),
                         uint32_t serial, uint32_t time __attribute__((unused)), uint32_t button,
                         uint32_t state) {
    podi_application_wayland *app = (podi_application_wayland *)data;
    app->last_input_serial = serial;

    // Check for Alt+Left-click to initiate window move
    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED &&
        (app->modifier_state & PODI_MOD_ALT) &&
        app->common.window_count > 0) {
        podi_window *window = app->common.windows[0];
        wayland_window_begin_move(window);
        return; // Don't send normal mouse event
    }

    // Check for title bar click (only if no server decorations and not in resize edge)
    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED &&
        !(app->modifier_state & PODI_MOD_ALT) &&
        app->common.window_count > 0) {
        podi_window_wayland *window = (podi_window_wayland *)app->common.windows[0];


        // Only activate title bar for windows without server decorations
        if (!window->has_server_decorations &&
            window->last_mouse_y >= 0 && window->last_mouse_y <= PODI_TITLE_BAR_HEIGHT) {

            // Check if click is in a resize edge area - if so, let resize take priority
            podi_resize_edge edge = podi_detect_resize_edge((podi_window *)window,
                                                          window->last_mouse_x,
                                                          window->last_mouse_y);

            if (edge == PODI_RESIZE_EDGE_NONE) {
                wayland_window_begin_move((podi_window *)window);
                return; // Don't send normal mouse event
            } else {
                // Fall through to normal event processing for resize
            }
        }
    }

    podi_event_type event_type = (state == WL_POINTER_BUTTON_STATE_PRESSED)
                                ? PODI_EVENT_MOUSE_BUTTON_DOWN : PODI_EVENT_MOUSE_BUTTON_UP;

    podi_event event = {0};
    event.type = event_type;
    event.window = app->common.window_count > 0 ? app->common.windows[0] : NULL;
    switch (button) {
        case BTN_LEFT:
            event.mouse_button.button = PODI_MOUSE_BUTTON_LEFT;
            break;
        case BTN_RIGHT:
            event.mouse_button.button = PODI_MOUSE_BUTTON_RIGHT;
            break;
        case BTN_MIDDLE:
            event.mouse_button.button = PODI_MOUSE_BUTTON_MIDDLE;
            break;
        default:
            return;
    }
    add_pending_event(&event);
}

static void pointer_axis(void *data __attribute__((unused)), struct wl_pointer *pointer __attribute__((unused)),
                       uint32_t time __attribute__((unused)), uint32_t axis, wl_fixed_t value) {
    podi_event event = {0};
    event.type = PODI_EVENT_MOUSE_SCROLL;
    
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        event.mouse_scroll.x = 0.0;
        event.mouse_scroll.y = -wl_fixed_to_double(value) / 10.0;
    } else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
        event.mouse_scroll.x = wl_fixed_to_double(value) / 10.0;
        event.mouse_scroll.y = 0.0;
    }
    add_pending_event(&event);
}

static void pointer_frame(void *data __attribute__((unused)), struct wl_pointer *pointer __attribute__((unused))) {
}

static void pointer_axis_source(void *data __attribute__((unused)), struct wl_pointer *pointer __attribute__((unused)),
                               uint32_t axis_source __attribute__((unused))) {
}

static void pointer_axis_stop(void *data __attribute__((unused)), struct wl_pointer *pointer __attribute__((unused)),
                             uint32_t time __attribute__((unused)), uint32_t axis __attribute__((unused))) {
}

static void pointer_axis_discrete(void *data __attribute__((unused)), struct wl_pointer *pointer __attribute__((unused)),
                                 uint32_t axis __attribute__((unused)), int32_t discrete __attribute__((unused))) {
}

static void pointer_axis_value120(void *data __attribute__((unused)), struct wl_pointer *pointer __attribute__((unused)),
                                 uint32_t axis __attribute__((unused)), int32_t value120 __attribute__((unused))) {
}

static void pointer_axis_relative_direction(void *data __attribute__((unused)), struct wl_pointer *pointer __attribute__((unused)),
                                           uint32_t axis __attribute__((unused)), uint32_t direction __attribute__((unused))) {
}

static const struct wl_pointer_listener pointer_listener = {
    pointer_enter,
    pointer_leave,
    pointer_motion,
    pointer_button,
    pointer_axis,
    pointer_frame,
    pointer_axis_source,
    pointer_axis_stop,
    pointer_axis_discrete,
    pointer_axis_value120,
    pointer_axis_relative_direction,
};

static void seat_capabilities(void *data, struct wl_seat *seat,
                            uint32_t capabilities) {
    podi_application_wayland *app = (podi_application_wayland *)data;
    app->seat_capabilities = capabilities;

    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
        app->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(app->keyboard, &keyboard_listener, app);
    }

    if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
        app->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(app->pointer, &pointer_listener, app);
    }
}

static void seat_name(void *data __attribute__((unused)), struct wl_seat *seat __attribute__((unused)), const char *name __attribute__((unused))) {
}

// Output listener for scale detection
static void output_geometry(void *data __attribute__((unused)), struct wl_output *wl_output __attribute__((unused)),
                           int32_t x __attribute__((unused)), int32_t y __attribute__((unused)),
                           int32_t physical_width __attribute__((unused)), int32_t physical_height __attribute__((unused)),
                           int32_t subpixel __attribute__((unused)), const char *make __attribute__((unused)),
                           const char *model __attribute__((unused)), int32_t transform __attribute__((unused))) {
    // We don't need geometry info for scale detection
}

static void output_mode(void *data __attribute__((unused)), struct wl_output *wl_output __attribute__((unused)),
                       uint32_t flags __attribute__((unused)), int32_t width __attribute__((unused)),
                       int32_t height __attribute__((unused)), int32_t refresh __attribute__((unused))) {
    // We don't need mode info for scale detection
}

static void output_done(void *data __attribute__((unused)), struct wl_output *wl_output __attribute__((unused))) {
    // Scale changes are applied immediately in output_scale
}

static void output_scale(void *data, struct wl_output *wl_output, int32_t factor) {
    podi_application_wayland *app = (podi_application_wayland *)data;

    // Find this output in our list and update its scale
    for (size_t i = 0; i < app->output_count; i++) {
        if (app->outputs[i] == wl_output) {
            app->output_scales[i] = factor;

            // Update max scale
            app->max_scale = 1;
            for (size_t j = 0; j < app->output_count; j++) {
                if (app->output_scales[j] > app->max_scale) {
                    app->max_scale = app->output_scales[j];
                }
            }
            return;
        }
    }
}

static void output_name(void *data __attribute__((unused)), struct wl_output *wl_output __attribute__((unused)),
                       const char *name __attribute__((unused))) {
    // We don't need output name for scale detection
}

static void output_description(void *data __attribute__((unused)), struct wl_output *wl_output __attribute__((unused)),
                              const char *description __attribute__((unused))) {
    // We don't need output description for scale detection
}

static const struct wl_output_listener output_listener = {
    output_geometry,
    output_mode,
    output_done,
    output_scale,
    output_name,
    output_description,
};

static const struct wl_seat_listener seat_listener = {
    seat_capabilities,
    seat_name,
};

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                                uint32_t serial) {
    podi_window_wayland *window = (podi_window_wayland *)data;
    xdg_surface_ack_configure(xdg_surface, serial);
    window->configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    xdg_surface_configure,
};

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel __attribute__((unused)),
                                 int32_t width, int32_t height,
                                 struct wl_array *states __attribute__((unused))) {
    podi_window_wayland *window = (podi_window_wayland *)data;

    // Convert logical size from Wayland to physical size for consistency with X11
    if (width > 0 && height > 0) {
        int physical_width = (int)(width * window->common.scale_factor);
        int physical_height = (int)(height * window->common.scale_factor);

        // Ensure dimensions are divisible by scale factor to satisfy Wayland protocol
        int scale = (int)window->common.scale_factor;
        if (scale > 1) {
            physical_width = (physical_width / scale) * scale;
            physical_height = (physical_height / scale) * scale;
        }

        if (physical_width != window->common.width || physical_height != window->common.height) {
            window->common.width = physical_width;
            window->common.height = physical_height;

            podi_event event = {0};
            event.type = PODI_EVENT_WINDOW_RESIZE;
            event.window = (podi_window *)window;
            event.window_resize.width = physical_width;
            event.window_resize.height = physical_height;
            add_pending_event(&event);
        }
    }
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel __attribute__((unused))) {
    podi_window_wayland *window = (podi_window_wayland *)data;
    
    podi_event event = {0};
    event.type = PODI_EVENT_WINDOW_CLOSE;
    event.window = (podi_window *)window;
    add_pending_event(&event);
}

static void xdg_toplevel_configure_bounds(void *data __attribute__((unused)), 
                                        struct xdg_toplevel *toplevel __attribute__((unused)),
                                        int32_t width __attribute__((unused)), 
                                        int32_t height __attribute__((unused))) {
}

static void xdg_toplevel_wm_capabilities(void *data __attribute__((unused)), 
                                       struct xdg_toplevel *toplevel __attribute__((unused)),
                                       struct wl_array *capabilities __attribute__((unused))) {
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    xdg_toplevel_configure,
    xdg_toplevel_close,
    xdg_toplevel_configure_bounds,
    xdg_toplevel_wm_capabilities,
};

static void xdg_wm_base_ping(void *data __attribute__((unused)), struct xdg_wm_base *xdg_wm_base,
                           uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    xdg_wm_base_ping,
};

static void decoration_configure(void *data,
                               struct zxdg_toplevel_decoration_v1 *decoration __attribute__((unused)),
                               uint32_t mode) {
    podi_window_wayland *window = (podi_window_wayland *)data;

    // The compositor has set the decoration mode
    const char *mode_str;
    switch (mode) {
        case ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE:
            mode_str = "client-side";
            window->has_server_decorations = false;
            break;
        case ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE:
            mode_str = "server-side";
            window->has_server_decorations = true;
            break;
        default:
            mode_str = "unknown";
            window->has_server_decorations = false;
            break;
    }
    printf("Podi: Decoration mode set to %s (%u)\n", mode_str, mode);
    fflush(stdout);
}

static const struct zxdg_toplevel_decoration_v1_listener decoration_listener = {
    decoration_configure,
};

static void registry_global(void *data, struct wl_registry *registry,
                          uint32_t name, const char *interface, uint32_t version __attribute__((unused))) {
    podi_application_wayland *app = (podi_application_wayland *)data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        app->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        app->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
        wl_seat_add_listener(app->seat, &seat_listener, app);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        app->xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(app->xdg_wm_base, &xdg_wm_base_listener, app);
    } else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
        app->decoration_manager = wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1);
        printf("Podi: Decoration manager found - server-side decorations available\n");
        fflush(stdout);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        app->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        // Add output to our tracking list
        if (app->output_count >= app->output_capacity) {
            size_t new_capacity = app->output_capacity ? app->output_capacity * 2 : 4;
            struct wl_output **new_outputs = realloc(app->outputs, new_capacity * sizeof(struct wl_output *));
            int32_t *new_scales = realloc(app->output_scales, new_capacity * sizeof(int32_t));
            if (new_outputs && new_scales) {
                app->outputs = new_outputs;
                app->output_scales = new_scales;
                app->output_capacity = new_capacity;
            }
        }

        if (app->output_count < app->output_capacity) {
            app->outputs[app->output_count] = wl_registry_bind(registry, name, &wl_output_interface, 2);
            app->output_scales[app->output_count] = 1; // Default scale
            wl_output_add_listener(app->outputs[app->output_count], &output_listener, app);
            app->output_count++;
        }
    }
}

static void registry_global_remove(void *data __attribute__((unused)), struct wl_registry *registry __attribute__((unused)),
                                 uint32_t name __attribute__((unused))) {
}

static const struct wl_registry_listener registry_listener = {
    registry_global,
    registry_global_remove,
};

static float wayland_get_display_scale_factor(podi_application *app_generic) {
    podi_application_wayland *app = (podi_application_wayland *)app_generic;
    if (!app) return 1.0f;
    return (float)app->max_scale;
}

static podi_application *wayland_application_create(void) {
    printf("Podi: Creating Wayland application\n");
    fflush(stdout);
    podi_application_wayland *app = calloc(1, sizeof(podi_application_wayland));
    if (!app) return NULL;

    // Initialize output tracking
    app->max_scale = 1;
    
    app->display = wl_display_connect(NULL);
    if (!app->display) {
        free(app);
        return NULL;
    }
    
    app->registry = wl_display_get_registry(app->display);
    wl_registry_add_listener(app->registry, &registry_listener, app);
    
    wl_display_dispatch(app->display);
    wl_display_roundtrip(app->display);

    if (!app->compositor || !app->xdg_wm_base) {
        wl_display_disconnect(app->display);
        free(app);
        return NULL;
    }

    // Initialize XKB context
    app->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!app->xkb_context) {
        wl_display_disconnect(app->display);
        free(app);
        return NULL;
    }

    // Initialize compose table from locale
    setlocale(LC_CTYPE, "");
    const char *locale = setlocale(LC_CTYPE, NULL);
    app->compose_table = xkb_compose_table_new_from_locale(app->xkb_context, locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (app->compose_table) {
        app->compose_state = xkb_compose_state_new(app->compose_table, XKB_COMPOSE_STATE_NO_FLAGS);
    }

    // Initialize cursor theme
    if (app->shm) {
        app->cursor_theme = wl_cursor_theme_load(NULL, 24, app->shm);
        if (app->cursor_theme) {
            app->cursor_surface = wl_compositor_create_surface(app->compositor);
        }
    }

    return (podi_application *)app;
}

static void wayland_application_destroy(podi_application *app_generic) {
    podi_application_wayland *app = (podi_application_wayland *)app_generic;
    if (!app) return;
    
    for (size_t i = 0; i < app->common.window_count; i++) {
        if (app->common.windows[i]) {
            podi_window_destroy(app->common.windows[i]);
        }
    }
    free(app->common.windows);
    
    // Cleanup cursor resources
    if (app->cursor_surface) wl_surface_destroy(app->cursor_surface);
    if (app->cursor_theme) wl_cursor_theme_destroy(app->cursor_theme);

    // Cleanup XKB resources
    if (app->compose_state) xkb_compose_state_unref(app->compose_state);
    if (app->compose_table) xkb_compose_table_unref(app->compose_table);
    if (app->xkb_state) xkb_state_unref(app->xkb_state);
    if (app->xkb_keymap) xkb_keymap_unref(app->xkb_keymap);
    if (app->xkb_context) xkb_context_unref(app->xkb_context);

    // Cleanup outputs
    for (size_t i = 0; i < app->output_count; i++) {
        if (app->outputs[i]) wl_output_destroy(app->outputs[i]);
    }
    free(app->outputs);
    free(app->output_scales);

    if (app->keyboard) wl_keyboard_destroy(app->keyboard);
    if (app->pointer) wl_pointer_destroy(app->pointer);
    if (app->seat) wl_seat_destroy(app->seat);
    if (app->decoration_manager) zxdg_decoration_manager_v1_destroy(app->decoration_manager);
    if (app->xdg_wm_base) xdg_wm_base_destroy(app->xdg_wm_base);
    if (app->compositor) wl_compositor_destroy(app->compositor);
    if (app->shm) wl_shm_destroy(app->shm);
    if (app->registry) wl_registry_destroy(app->registry);
    if (app->display) wl_display_disconnect(app->display);
    
    free(app);
}

static bool wayland_application_should_close(podi_application *app_generic) {
    podi_application_wayland *app = (podi_application_wayland *)app_generic;
    return app ? app->common.should_close : true;
}

static void wayland_application_close(podi_application *app_generic) {
    podi_application_wayland *app = (podi_application_wayland *)app_generic;
    if (app) app->common.should_close = true;
}

static bool wayland_application_poll_event(podi_application *app_generic, podi_event *event) {
    podi_application_wayland *app = (podi_application_wayland *)app_generic;
    if (!app || !event) return false;

    while (true) {
        // Process pending events first
        wl_display_dispatch_pending(app->display);

        if (get_pending_event(event)) {
            // Check if this event should be handled by resize system
            if (event->window && podi_handle_resize_event(event->window, event)) {
                continue; // Event was consumed by resize handler, get next event
            }
            return true;
        }

        // If no pending events, try to read from the socket without blocking
        if (wl_display_prepare_read(app->display) == 0) {
            wl_display_read_events(app->display);
            wl_display_dispatch_pending(app->display);

            if (get_pending_event(event)) {
                // Check if this event should be handled by resize system
                if (event->window && podi_handle_resize_event(event->window, event)) {
                    continue; // Event was consumed by resize handler, get next event
                }
                return true;
            }
        }

        return false; // No more events
    }
}

static podi_window *wayland_window_create(podi_application *app_generic, const char *title, int width, int height) {
    podi_application_wayland *app = (podi_application_wayland *)app_generic;
    if (!app) return NULL;
    
    podi_window_wayland *window = calloc(1, sizeof(podi_window_wayland));
    if (!window) return NULL;
    
    window->app = app;
    window->common.scale_factor = (float)app->max_scale;
    window->common.title = strdup(title ? title : "Podi Window");

    // Initialize resize state
    window->common.is_resizing = false;
    window->common.resize_edge = PODI_RESIZE_EDGE_NONE;
    window->common.resize_border_width = 8;

    // Store requested content size (what user wants for rendering)
    window->common.content_width = width;
    window->common.content_height = height;

    // Surface size starts the same, will be adjusted if client decorations are needed
    window->common.width = width;
    window->common.height = height;

    // Initialize client-side decoration state
    window->has_server_decorations = false;
    window->last_mouse_x = 0.0;
    window->last_mouse_y = 0.0;


    window->surface = wl_compositor_create_surface(app->compositor);
    window->xdg_surface = xdg_wm_base_get_xdg_surface(app->xdg_wm_base, window->surface);
    window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);

    xdg_surface_add_listener(window->xdg_surface, &xdg_surface_listener, window);
    xdg_toplevel_add_listener(window->xdg_toplevel, &xdg_toplevel_listener, window);

    xdg_toplevel_set_title(window->xdg_toplevel, window->common.title);

    // Create decoration object if decoration manager is available
    if (app->decoration_manager) {
        printf("Podi: Requesting server-side decorations for window\n");
        fflush(stdout);
        window->decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
            app->decoration_manager, window->xdg_toplevel);
        zxdg_toplevel_decoration_v1_add_listener(window->decoration, &decoration_listener, window);
        // Request server-side decorations
        zxdg_toplevel_decoration_v1_set_mode(window->decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
        window->has_server_decorations = true;  // Assume we'll get server decorations
    } else {
        printf("Podi: No decoration manager available - using client-side decorations\n");
        fflush(stdout);
        window->has_server_decorations = false;
        // Adjust surface size to include title bar (scale logical pixels to physical)
        window->common.height += (int)(PODI_TITLE_BAR_HEIGHT * window->common.scale_factor);
    }

    // Calculate logical size for Wayland (physical size / scale factor)
    int logical_width = (int)(window->common.width / window->common.scale_factor);
    int logical_height = (int)(window->common.height / window->common.scale_factor);

    // Set buffer scale to inform Wayland that our buffer is at physical resolution
    if (window->common.scale_factor > 1.0f) {
        wl_surface_set_buffer_scale(window->surface, (int32_t)window->common.scale_factor);
    }

    // Set the logical window size (this is what Wayland uses for window management)
    if (logical_width > 0 && logical_height > 0) {
        // Note: We'll get a configure event that might change these, but this gives a hint
        // The actual size will be set in the configure event
    }

    wl_surface_commit(window->surface);
    
    while (!window->configured) {
        wl_display_dispatch(app->display);
    }
    
    if (app->common.window_count >= app->common.window_capacity) {
        size_t new_capacity = app->common.window_capacity ? app->common.window_capacity * 2 : 4;
        podi_window **new_windows = realloc(app->common.windows, new_capacity * sizeof(podi_window *));
        if (!new_windows) {
            xdg_toplevel_destroy(window->xdg_toplevel);
            xdg_surface_destroy(window->xdg_surface);
            wl_surface_destroy(window->surface);
            free(window->common.title);
            free(window);
            return NULL;
        }
        app->common.windows = new_windows;
        app->common.window_capacity = new_capacity;
    }
    
    app->common.windows[app->common.window_count++] = (podi_window *)window;
    
    return (podi_window *)window;
}

static void wayland_window_destroy(podi_window *window_generic) {
    podi_window_wayland *window = (podi_window_wayland *)window_generic;
    if (!window) return;
    
    podi_application_wayland *app = window->app;
    
    for (size_t i = 0; i < app->common.window_count; i++) {
        if (app->common.windows[i] == window_generic) {
            memmove(&app->common.windows[i], &app->common.windows[i + 1],
                   (app->common.window_count - i - 1) * sizeof(podi_window *));
            app->common.window_count--;
            break;
        }
    }

    if (window->decoration) {
        zxdg_toplevel_decoration_v1_destroy(window->decoration);
    }
    xdg_toplevel_destroy(window->xdg_toplevel);
    xdg_surface_destroy(window->xdg_surface);
    wl_surface_destroy(window->surface);
    free(window->common.title);
    free(window);
}

static void wayland_window_close(podi_window *window_generic) {
    podi_window_wayland *window = (podi_window_wayland *)window_generic;
    if (window) window->common.should_close = true;
}

static void wayland_window_set_title(podi_window *window_generic, const char *title) {
    podi_window_wayland *window = (podi_window_wayland *)window_generic;
    if (!window || !title) return;
    
    free(window->common.title);
    window->common.title = strdup(title);
    xdg_toplevel_set_title(window->xdg_toplevel, title);
}

static void wayland_window_set_size(podi_window *window_generic, int width, int height) {
    podi_window_wayland *window = (podi_window_wayland *)window_generic;
    if (!window) return;

    // Update content dimensions (what user requested)
    window->common.content_width = width;
    window->common.content_height = height;

    // Update surface dimensions (including decorations if needed)
    window->common.width = width;
    window->common.height = height;
    if (!window->has_server_decorations) {
        window->common.height += (int)(PODI_TITLE_BAR_HEIGHT * window->common.scale_factor);
    }
}

static void wayland_window_set_position_and_size(podi_window *window_generic, int x, int y, int width, int height) {
    podi_window_wayland *window = (podi_window_wayland *)window_generic;
    if (!window) return;

    // In Wayland, clients can't control window position - compositor handles it
    // So we just update size and track position for consistency
    window->common.x = x;
    window->common.y = y;

    // Update content dimensions (what user requested)
    window->common.content_width = width;
    window->common.content_height = height;

    // Update surface dimensions (including decorations if needed)
    window->common.width = width;
    window->common.height = height;
    if (!window->has_server_decorations) {
        window->common.height += (int)(PODI_TITLE_BAR_HEIGHT * window->common.scale_factor);
    }
}


static void wayland_window_get_size(podi_window *window_generic, int *width, int *height) {
    podi_window_wayland *window = (podi_window_wayland *)window_generic;
    if (!window) return;

    // Return content dimensions (what user requested)
    if (width) *width = window->common.content_width;
    if (height) *height = window->common.content_height;
}

static void wayland_window_get_framebuffer_size(podi_window *window_generic, int *width, int *height) {
    podi_window_wayland *window = (podi_window_wayland *)window_generic;
    if (!window) return;

    // Get the content dimensions (for rendering)
    int w = window->common.content_width;
    int h = window->common.content_height;

    // Ensure framebuffer dimensions are divisible by scale factor to satisfy Wayland protocol
    int scale = (int)window->common.scale_factor;
    if (scale > 1) {
        w = (w / scale) * scale;
        h = (h / scale) * scale;
    }

    if (width) *width = w;
    if (height) *height = h;
}

static void wayland_window_get_surface_size(podi_window *window_generic, int *width, int *height) {
    podi_window_wayland *window = (podi_window_wayland *)window_generic;
    if (!window) return;

    // Get the full surface dimensions (including decorations)
    int w = window->common.width;
    int h = window->common.height;

    // Ensure surface dimensions are divisible by scale factor to satisfy Wayland protocol
    int scale = (int)window->common.scale_factor;
    if (scale > 1) {
        w = (w / scale) * scale;
        h = (h / scale) * scale;
    }

    if (width) *width = w;
    if (height) *height = h;
}

static float wayland_window_get_scale_factor(podi_window *window_generic) {
    podi_window_wayland *window = (podi_window_wayland *)window_generic;
    return window ? window->common.scale_factor : 1.0f;
}

static bool wayland_window_should_close(podi_window *window_generic) {
    podi_window_wayland *window = (podi_window_wayland *)window_generic;
    return window ? window->common.should_close : true;
}

static bool wayland_window_get_x11_handles(podi_window *window_generic, podi_x11_handles *handles) {
    (void)window_generic;
    (void)handles;
    return false;
}

static bool wayland_window_get_wayland_handles(podi_window *window_generic, podi_wayland_handles *handles) {
    podi_window_wayland *window = (podi_window_wayland *)window_generic;
    if (!window || !handles) return false;

    handles->display = window->app->display;
    handles->surface = window->surface;
    return true;
}

static void wayland_window_set_cursor(podi_window *window_generic, podi_cursor_shape cursor) {
    podi_window_wayland *window = (podi_window_wayland *)window_generic;
    if (!window || !window->app) return;

    podi_application_wayland *app = window->app;
    if (!app->cursor_theme || !app->cursor_surface || !app->pointer) return;

    const char *cursor_name;
    switch (cursor) {
        case PODI_CURSOR_RESIZE_N:
            cursor_name = "n-resize";
            break;
        case PODI_CURSOR_RESIZE_S:
            cursor_name = "s-resize";
            break;
        case PODI_CURSOR_RESIZE_E:
            cursor_name = "e-resize";
            break;
        case PODI_CURSOR_RESIZE_W:
            cursor_name = "w-resize";
            break;
        case PODI_CURSOR_RESIZE_NE:
            cursor_name = "ne-resize";
            break;
        case PODI_CURSOR_RESIZE_SW:
            cursor_name = "sw-resize";
            break;
        case PODI_CURSOR_RESIZE_NW:
            cursor_name = "nw-resize";
            break;
        case PODI_CURSOR_RESIZE_SE:
            cursor_name = "se-resize";
            break;
        case PODI_CURSOR_DEFAULT:
        default:
            cursor_name = "left_ptr";
            break;
    }

    struct wl_cursor *wl_cursor = wl_cursor_theme_get_cursor(app->cursor_theme, cursor_name);
    if (!wl_cursor || wl_cursor->image_count == 0) return;

    struct wl_cursor_image *image = wl_cursor->images[0];
    struct wl_buffer *buffer = wl_cursor_image_get_buffer(image);
    if (!buffer) return;

    wl_surface_attach(app->cursor_surface, buffer, 0, 0);
    wl_surface_damage(app->cursor_surface, 0, 0, image->width, image->height);
    wl_surface_commit(app->cursor_surface);

    wl_pointer_set_cursor(app->pointer, app->last_input_serial,
                         app->cursor_surface, image->hotspot_x, image->hotspot_y);
}

static void wayland_window_begin_interactive_resize(podi_window *window_generic, int edge) {
    podi_window_wayland *window = (podi_window_wayland *)window_generic;
    if (!window || !window->app->seat) return;

    // Convert podi resize edge to XDG resize edge (they match, but let's be explicit)
    uint32_t xdg_edge;
    switch (edge) {
        case 0: xdg_edge = XDG_TOPLEVEL_RESIZE_EDGE_NONE; break;
        case 1: xdg_edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP; break;
        case 2: xdg_edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM; break;
        case 4: xdg_edge = XDG_TOPLEVEL_RESIZE_EDGE_LEFT; break;
        case 5: xdg_edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT; break;
        case 6: xdg_edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT; break;
        case 8: xdg_edge = XDG_TOPLEVEL_RESIZE_EDGE_RIGHT; break;
        case 9: xdg_edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT; break;
        case 10: xdg_edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT; break;
        default: return; // Invalid edge
    }

    // Start interactive resize using the last input serial
    xdg_toplevel_resize(window->xdg_toplevel, window->app->seat,
                       window->app->last_input_serial, xdg_edge);
}

static void wayland_window_begin_move(podi_window *window_generic) {
    podi_window_wayland *window = (podi_window_wayland *)window_generic;
    if (!window || !window->app->seat) return;

    // Start interactive move using the last input serial
    xdg_toplevel_move(window->xdg_toplevel, window->app->seat,
                     window->app->last_input_serial);
}

const podi_platform_vtable wayland_vtable = {
    .application_create = wayland_application_create,
    .application_destroy = wayland_application_destroy,
    .application_should_close = wayland_application_should_close,
    .application_close = wayland_application_close,
    .application_poll_event = wayland_application_poll_event,
    .get_display_scale_factor = wayland_get_display_scale_factor,
    .window_create = wayland_window_create,
    .window_destroy = wayland_window_destroy,
    .window_close = wayland_window_close,
    .window_set_title = wayland_window_set_title,
    .window_set_size = wayland_window_set_size,
    .window_set_position_and_size = wayland_window_set_position_and_size,
    .window_get_size = wayland_window_get_size,
    .window_get_framebuffer_size = wayland_window_get_framebuffer_size,
    .window_get_surface_size = wayland_window_get_surface_size,
    .window_get_scale_factor = wayland_window_get_scale_factor,
    .window_should_close = wayland_window_should_close,
    .window_begin_interactive_resize = wayland_window_begin_interactive_resize,
    .window_begin_move = wayland_window_begin_move,
    .window_set_cursor = wayland_window_set_cursor,
#ifdef PODI_PLATFORM_LINUX
    .window_get_x11_handles = wayland_window_get_x11_handles,
    .window_get_wayland_handles = wayland_window_get_wayland_handles,
#endif
};
