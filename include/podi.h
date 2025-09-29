#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct podi_application podi_application;
typedef struct podi_window podi_window;

typedef enum {
    PODI_EVENT_NONE = 0,
    PODI_EVENT_WINDOW_CLOSE,
    PODI_EVENT_WINDOW_RESIZE,
    PODI_EVENT_WINDOW_FOCUS,
    PODI_EVENT_WINDOW_UNFOCUS,
    PODI_EVENT_KEY_DOWN,
    PODI_EVENT_KEY_UP,
    PODI_EVENT_MOUSE_BUTTON_DOWN,
    PODI_EVENT_MOUSE_BUTTON_UP,
    PODI_EVENT_MOUSE_MOVE,
    PODI_EVENT_MOUSE_SCROLL,
    PODI_EVENT_MOUSE_ENTER,
    PODI_EVENT_MOUSE_LEAVE
} podi_event_type;

typedef enum {
    PODI_KEY_UNKNOWN = 0,
    PODI_KEY_A, PODI_KEY_B, PODI_KEY_C, PODI_KEY_D, PODI_KEY_E, PODI_KEY_F,
    PODI_KEY_G, PODI_KEY_H, PODI_KEY_I, PODI_KEY_J, PODI_KEY_K, PODI_KEY_L,
    PODI_KEY_M, PODI_KEY_N, PODI_KEY_O, PODI_KEY_P, PODI_KEY_Q, PODI_KEY_R,
    PODI_KEY_S, PODI_KEY_T, PODI_KEY_U, PODI_KEY_V, PODI_KEY_W, PODI_KEY_X,
    PODI_KEY_Y, PODI_KEY_Z,
    PODI_KEY_0, PODI_KEY_1, PODI_KEY_2, PODI_KEY_3, PODI_KEY_4,
    PODI_KEY_5, PODI_KEY_6, PODI_KEY_7, PODI_KEY_8, PODI_KEY_9,
    PODI_KEY_SPACE, PODI_KEY_ENTER, PODI_KEY_ESCAPE, PODI_KEY_BACKSPACE,
    PODI_KEY_TAB, PODI_KEY_SHIFT, PODI_KEY_CTRL, PODI_KEY_ALT,
    PODI_KEY_UP, PODI_KEY_DOWN, PODI_KEY_LEFT, PODI_KEY_RIGHT
} podi_key;

typedef enum {
    PODI_MOUSE_BUTTON_LEFT = 0,
    PODI_MOUSE_BUTTON_RIGHT,
    PODI_MOUSE_BUTTON_MIDDLE,
    PODI_MOUSE_BUTTON_X1,
    PODI_MOUSE_BUTTON_X2
} podi_mouse_button;

typedef enum {
    PODI_MOD_SHIFT = 1 << 0,
    PODI_MOD_CTRL  = 1 << 1,
    PODI_MOD_ALT   = 1 << 2,
    PODI_MOD_SUPER = 1 << 3
} podi_mod_flags;

typedef enum {
    PODI_RESIZE_EDGE_NONE = 0,
    PODI_RESIZE_EDGE_TOP = 1,
    PODI_RESIZE_EDGE_BOTTOM = 2,
    PODI_RESIZE_EDGE_LEFT = 4,
    PODI_RESIZE_EDGE_TOP_LEFT = 5,
    PODI_RESIZE_EDGE_BOTTOM_LEFT = 6,
    PODI_RESIZE_EDGE_RIGHT = 8,
    PODI_RESIZE_EDGE_TOP_RIGHT = 9,
    PODI_RESIZE_EDGE_BOTTOM_RIGHT = 10
} podi_resize_edge;

typedef enum {
    PODI_CURSOR_DEFAULT = 0,
    PODI_CURSOR_RESIZE_N,
    PODI_CURSOR_RESIZE_S,
    PODI_CURSOR_RESIZE_E,
    PODI_CURSOR_RESIZE_W,
    PODI_CURSOR_RESIZE_NE,
    PODI_CURSOR_RESIZE_NW,
    PODI_CURSOR_RESIZE_SE,
    PODI_CURSOR_RESIZE_SW
} podi_cursor_shape;

typedef struct {
    podi_event_type type;
    podi_window *window;
    union {
        struct {
            int width, height;
        } window_resize;
        struct {
            podi_key key;
            uint32_t native_keycode;
            const char *text;
            uint32_t modifiers;
        } key;
        struct {
            podi_mouse_button button;
        } mouse_button;
        struct {
            double x, y;         // Absolute position
            double delta_x, delta_y;  // Delta movement since last frame
        } mouse_move;
        struct {
            double x, y;
        } mouse_scroll;
    };
} podi_event;

typedef enum {
    PODI_BACKEND_AUTO = 0,
    PODI_BACKEND_X11,
    PODI_BACKEND_WAYLAND
} podi_backend_type;

typedef int (*podi_main_func)(podi_application *app);

void podi_set_backend(podi_backend_type backend);
podi_backend_type podi_get_backend(void);
const char *podi_get_backend_name(void);

podi_key podi_translate_native_keycode(uint32_t native_keycode);
const char *podi_get_key_name(podi_key key);
const char *podi_get_mouse_button_name(podi_mouse_button button);
const char *podi_get_modifiers_string(uint32_t modifiers);

podi_application *podi_application_create(void);
void podi_application_destroy(podi_application *app);
bool podi_application_should_close(podi_application *app);
void podi_application_close(podi_application *app);
bool podi_application_poll_event(podi_application *app, podi_event *event);
float podi_get_display_scale_factor(podi_application *app);

podi_window *podi_window_create(podi_application *app, const char *title, int width, int height);
void podi_window_destroy(podi_window *window);
void podi_window_close(podi_window *window);
void podi_window_set_title(podi_window *window, const char *title);
void podi_window_set_size(podi_window *window, int width, int height);
void podi_window_set_position_and_size(podi_window *window, int x, int y, int width, int height);
void podi_window_get_size(podi_window *window, int *width, int *height);
void podi_window_get_framebuffer_size(podi_window *window, int *width, int *height);
void podi_window_get_surface_size(podi_window *window, int *width, int *height);
float podi_window_get_scale_factor(podi_window *window);
bool podi_window_should_close(podi_window *window);
void podi_window_begin_interactive_resize(podi_window *window, int edge);
void podi_window_begin_move(podi_window *window);
void podi_window_set_cursor(podi_window *window, podi_cursor_shape cursor);
void podi_window_set_cursor_mode(podi_window *window, bool locked, bool visible);
void podi_window_get_cursor_position(podi_window *window, double *x, double *y);
void podi_window_set_fullscreen_exclusive(podi_window *window, bool enabled);
bool podi_window_is_fullscreen_exclusive(podi_window *window);

#ifdef PODI_PLATFORM_LINUX
typedef struct podi_x11_handles {
    void *display;
    unsigned long window;
} podi_x11_handles;

typedef struct podi_wayland_handles {
    void *display;
    void *surface;
} podi_wayland_handles;

bool podi_window_get_x11_handles(podi_window *window, podi_x11_handles *handles);
bool podi_window_get_wayland_handles(podi_window *window, podi_wayland_handles *handles);
#endif

int podi_main(podi_main_func main_func);

#ifdef __cplusplus
}
#endif
