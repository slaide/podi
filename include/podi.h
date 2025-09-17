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
            double x, y;
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

podi_window *podi_window_create(podi_application *app, const char *title, int width, int height);
void podi_window_destroy(podi_window *window);
void podi_window_close(podi_window *window);
void podi_window_set_title(podi_window *window, const char *title);
void podi_window_set_size(podi_window *window, int width, int height);
void podi_window_get_size(podi_window *window, int *width, int *height);
bool podi_window_should_close(podi_window *window);

int podi_main(podi_main_func main_func);

#ifdef __cplusplus
}
#endif