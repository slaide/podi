#include "internal.h"
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include "xdg-shell-client-protocol.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/input-event-codes.h>

typedef struct {
    podi_application_common common;
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;
    struct xdg_wm_base *xdg_wm_base;
    struct wl_shm *shm;
    uint32_t seat_capabilities;
    uint32_t modifier_state;
} podi_application_wayland;

typedef struct {
    podi_window_common common;
    podi_application_wayland *app;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    bool configured;
} podi_window_wayland;

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

static void keyboard_keymap(void *data __attribute__((unused)), 
                           struct wl_keyboard *keyboard __attribute__((unused)),
                           uint32_t format __attribute__((unused)), 
                           int fd, uint32_t size __attribute__((unused))) {
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
                       uint32_t serial __attribute__((unused)), uint32_t time __attribute__((unused)), uint32_t key,
                       uint32_t state) {
    podi_application_wayland *app = (podi_application_wayland *)data;
    podi_event_type event_type = (state == WL_KEYBOARD_KEY_STATE_PRESSED) 
                                ? PODI_EVENT_KEY_DOWN : PODI_EVENT_KEY_UP;
    
    podi_event event = {0};
    event.type = event_type;
    event.window = app->common.window_count > 0 ? app->common.windows[0] : NULL;
    event.key.key = wayland_keycode_to_podi_key(key);
    event.key.native_keycode = key;
    event.key.modifiers = app->modifier_state;
    
    // Basic text input for Wayland (simplified approach)
    static char text_buffer[32];
    text_buffer[0] = '\0';
    event.key.text = NULL;
    
    if (event_type == PODI_EVENT_KEY_DOWN) {
        // Simple ASCII mapping for common keys
        podi_key mapped_key = wayland_keycode_to_podi_key(key);
        if (mapped_key >= PODI_KEY_A && mapped_key <= PODI_KEY_Z) {
            text_buffer[0] = 'a' + (mapped_key - PODI_KEY_A);
            text_buffer[1] = '\0';
            event.key.text = text_buffer;
        } else if (mapped_key >= PODI_KEY_0 && mapped_key <= PODI_KEY_9) {
            text_buffer[0] = '0' + (mapped_key - PODI_KEY_0);
            text_buffer[1] = '\0';
            event.key.text = text_buffer;
        } else if (mapped_key == PODI_KEY_SPACE) {
            text_buffer[0] = ' ';
            text_buffer[1] = '\0';
            event.key.text = text_buffer;
        }
    }
    
    add_pending_event(&event);
}

static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard __attribute__((unused)),
                             uint32_t serial __attribute__((unused)), uint32_t mods_depressed,
                             uint32_t mods_latched __attribute__((unused)), uint32_t mods_locked __attribute__((unused)),
                             uint32_t group __attribute__((unused))) {
    podi_application_wayland *app = (podi_application_wayland *)data;
    app->modifier_state = wayland_mods_to_podi_modifiers(mods_depressed);
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

static void pointer_motion(void *data __attribute__((unused)), struct wl_pointer *pointer __attribute__((unused)),
                         uint32_t time __attribute__((unused)), wl_fixed_t sx, wl_fixed_t sy) {
    podi_event event = {0};
    event.type = PODI_EVENT_MOUSE_MOVE;
    event.mouse_move.x = wl_fixed_to_double(sx);
    event.mouse_move.y = wl_fixed_to_double(sy);
    add_pending_event(&event);
}

static void pointer_button(void *data __attribute__((unused)), struct wl_pointer *pointer __attribute__((unused)),
                         uint32_t serial __attribute__((unused)), uint32_t time __attribute__((unused)), uint32_t button,
                         uint32_t state) {
    podi_event_type event_type = (state == WL_POINTER_BUTTON_STATE_PRESSED)
                                ? PODI_EVENT_MOUSE_BUTTON_DOWN : PODI_EVENT_MOUSE_BUTTON_UP;
    
    podi_event event = {0};
    event.type = event_type;
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
    
    if (width > 0 && height > 0 && 
        (width != window->common.width || height != window->common.height)) {
        window->common.width = width;
        window->common.height = height;
        
        podi_event event = {0};
        event.type = PODI_EVENT_WINDOW_RESIZE;
        event.window = (podi_window *)window;
        event.window_resize.width = width;
        event.window_resize.height = height;
        add_pending_event(&event);
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
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        app->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    }
}

static void registry_global_remove(void *data __attribute__((unused)), struct wl_registry *registry __attribute__((unused)),
                                 uint32_t name __attribute__((unused))) {
}

static const struct wl_registry_listener registry_listener = {
    registry_global,
    registry_global_remove,
};

static podi_application *wayland_application_create(void) {
    podi_application_wayland *app = calloc(1, sizeof(podi_application_wayland));
    if (!app) return NULL;
    
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
    
    if (app->keyboard) wl_keyboard_destroy(app->keyboard);
    if (app->pointer) wl_pointer_destroy(app->pointer);
    if (app->seat) wl_seat_destroy(app->seat);
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
    
    // Process pending events first
    wl_display_dispatch_pending(app->display);
    
    if (get_pending_event(event)) {
        return true;
    }
    
    // If no pending events, try to read from the socket without blocking
    if (wl_display_prepare_read(app->display) == 0) {
        wl_display_read_events(app->display);
        wl_display_dispatch_pending(app->display);
        
        if (get_pending_event(event)) {
            return true;
        }
    }
    
    return false;
}

static podi_window *wayland_window_create(podi_application *app_generic, const char *title, int width, int height) {
    podi_application_wayland *app = (podi_application_wayland *)app_generic;
    if (!app) return NULL;
    
    podi_window_wayland *window = calloc(1, sizeof(podi_window_wayland));
    if (!window) return NULL;
    
    window->app = app;
    window->common.width = width;
    window->common.height = height;
    window->common.title = strdup(title ? title : "Podi Window");
    
    window->surface = wl_compositor_create_surface(app->compositor);
    window->xdg_surface = xdg_wm_base_get_xdg_surface(app->xdg_wm_base, window->surface);
    window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);
    
    xdg_surface_add_listener(window->xdg_surface, &xdg_surface_listener, window);
    xdg_toplevel_add_listener(window->xdg_toplevel, &xdg_toplevel_listener, window);
    
    xdg_toplevel_set_title(window->xdg_toplevel, window->common.title);
    
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
    
    window->common.width = width;
    window->common.height = height;
}

static void wayland_window_get_size(podi_window *window_generic, int *width, int *height) {
    podi_window_wayland *window = (podi_window_wayland *)window_generic;
    if (!window) return;
    
    if (width) *width = window->common.width;
    if (height) *height = window->common.height;
}

static bool wayland_window_should_close(podi_window *window_generic) {
    podi_window_wayland *window = (podi_window_wayland *)window_generic;
    return window ? window->common.should_close : true;
}

const podi_platform_vtable wayland_vtable = {
    .application_create = wayland_application_create,
    .application_destroy = wayland_application_destroy,
    .application_should_close = wayland_application_should_close,
    .application_close = wayland_application_close,
    .application_poll_event = wayland_application_poll_event,
    .window_create = wayland_window_create,
    .window_destroy = wayland_window_destroy,
    .window_close = wayland_window_close,
    .window_set_title = wayland_window_set_title,
    .window_set_size = wayland_window_set_size,
    .window_get_size = wayland_window_get_size,
    .window_should_close = wayland_window_should_close,
};

