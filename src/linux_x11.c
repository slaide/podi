#include "internal.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    podi_application_common common;
    Display *display;
    int screen;
    Atom wm_delete_window;
    XIM input_method;
} podi_application_x11;

typedef struct {
    podi_window_common common;
    podi_application_x11 *app;
    Window window;
    XIC input_context;
} podi_window_x11;

static uint32_t x11_state_to_podi_modifiers(unsigned int state) {
    uint32_t modifiers = 0;
    if (state & ShiftMask) modifiers |= PODI_MOD_SHIFT;
    if (state & ControlMask) modifiers |= PODI_MOD_CTRL;
    if (state & Mod1Mask) modifiers |= PODI_MOD_ALT;     // Left Alt
    if (state & Mod5Mask) modifiers |= PODI_MOD_ALT;     // Right Alt/AltGr (common on intl keyboards)
    if (state & Mod4Mask) modifiers |= PODI_MOD_SUPER;
    return modifiers;
}

static podi_key x11_keycode_to_podi_key(KeySym keysym) {
    switch (keysym) {
        case XK_a: case XK_A: return PODI_KEY_A;
        case XK_b: case XK_B: return PODI_KEY_B;
        case XK_c: case XK_C: return PODI_KEY_C;
        case XK_d: case XK_D: return PODI_KEY_D;
        case XK_e: case XK_E: return PODI_KEY_E;
        case XK_f: case XK_F: return PODI_KEY_F;
        case XK_g: case XK_G: return PODI_KEY_G;
        case XK_h: case XK_H: return PODI_KEY_H;
        case XK_i: case XK_I: return PODI_KEY_I;
        case XK_j: case XK_J: return PODI_KEY_J;
        case XK_k: case XK_K: return PODI_KEY_K;
        case XK_l: case XK_L: return PODI_KEY_L;
        case XK_m: case XK_M: return PODI_KEY_M;
        case XK_n: case XK_N: return PODI_KEY_N;
        case XK_o: case XK_O: return PODI_KEY_O;
        case XK_p: case XK_P: return PODI_KEY_P;
        case XK_q: case XK_Q: return PODI_KEY_Q;
        case XK_r: case XK_R: return PODI_KEY_R;
        case XK_s: case XK_S: return PODI_KEY_S;
        case XK_t: case XK_T: return PODI_KEY_T;
        case XK_u: case XK_U: return PODI_KEY_U;
        case XK_v: case XK_V: return PODI_KEY_V;
        case XK_w: case XK_W: return PODI_KEY_W;
        case XK_x: case XK_X: return PODI_KEY_X;
        case XK_y: case XK_Y: return PODI_KEY_Y;
        case XK_z: case XK_Z: return PODI_KEY_Z;
        case XK_0: return PODI_KEY_0;
        case XK_1: return PODI_KEY_1;
        case XK_2: return PODI_KEY_2;
        case XK_3: return PODI_KEY_3;
        case XK_4: return PODI_KEY_4;
        case XK_5: return PODI_KEY_5;
        case XK_6: return PODI_KEY_6;
        case XK_7: return PODI_KEY_7;
        case XK_8: return PODI_KEY_8;
        case XK_9: return PODI_KEY_9;
        case XK_space: return PODI_KEY_SPACE;
        case XK_Return: return PODI_KEY_ENTER;
        case XK_Escape: return PODI_KEY_ESCAPE;
        case XK_BackSpace: return PODI_KEY_BACKSPACE;
        case XK_Tab: return PODI_KEY_TAB;
        case XK_Shift_L: case XK_Shift_R: return PODI_KEY_SHIFT;
        case XK_Control_L: case XK_Control_R: return PODI_KEY_CTRL;
        case XK_Alt_L: case XK_Alt_R:
        case XK_Meta_L: case XK_Meta_R:
        case XK_ISO_Level3_Shift: return PODI_KEY_ALT;
        case XK_Up: return PODI_KEY_UP;
        case XK_Down: return PODI_KEY_DOWN;
        case XK_Left: return PODI_KEY_LEFT;
        case XK_Right: return PODI_KEY_RIGHT;
        default: return PODI_KEY_UNKNOWN;
    }
}

static podi_application *x11_application_create(void) {
    podi_application_x11 *app = calloc(1, sizeof(podi_application_x11));
    if (!app) return NULL;
    
    app->display = XOpenDisplay(NULL);
    if (!app->display) {
        free(app);
        return NULL;
    }
    
    app->screen = DefaultScreen(app->display);
    app->wm_delete_window = XInternAtom(app->display, "WM_DELETE_WINDOW", False);
    
    // Set locale to user's preference for proper text handling
    setlocale(LC_ALL, "");

    // Initialize input method for proper composition support
    if (XSupportsLocale()) {
        XSetLocaleModifiers("");
        app->input_method = XOpenIM(app->display, NULL, NULL, NULL);
    } else {
        app->input_method = NULL;
    }
    
    return (podi_application *)app;
}

static void x11_application_destroy(podi_application *app_generic) {
    podi_application_x11 *app = (podi_application_x11 *)app_generic;
    if (!app) return;
    
    for (size_t i = 0; i < app->common.window_count; i++) {
        if (app->common.windows[i]) {
            podi_window_destroy(app->common.windows[i]);
        }
    }
    free(app->common.windows);
    
    if (app->input_method) {
        XCloseIM(app->input_method);
    }
    
    if (app->display) {
        XCloseDisplay(app->display);
    }
    free(app);
}

static bool x11_application_should_close(podi_application *app_generic) {
    podi_application_x11 *app = (podi_application_x11 *)app_generic;
    return app ? app->common.should_close : true;
}

static void x11_application_close(podi_application *app_generic) {
    podi_application_x11 *app = (podi_application_x11 *)app_generic;
    if (app) app->common.should_close = true;
}

static bool x11_application_poll_event(podi_application *app_generic, podi_event *event) {
    podi_application_x11 *app = (podi_application_x11 *)app_generic;
    if (!app || !event) return false;
    
    if (!XPending(app->display)) return false;
    
    XEvent xevent;
    XNextEvent(app->display, &xevent);

    // Let input method process the event first
    if (XFilterEvent(&xevent, None)) {
        return false;  // Event consumed by input method
    }

    podi_window_x11 *window = NULL;
    for (size_t i = 0; i < app->common.window_count; i++) {
        podi_window_x11 *w = (podi_window_x11 *)app->common.windows[i];
        if (w && w->window == xevent.xany.window) {
            window = w;
            break;
        }
    }
    
    if (!window) return false;
    
    event->window = (podi_window *)window;
    
    switch (xevent.type) {
        case ClientMessage:
            if (xevent.xclient.data.l[0] == (long)app->wm_delete_window) {
                event->type = PODI_EVENT_WINDOW_CLOSE;
                return true;
            }
            break;
            
        case ConfigureNotify:
            if (xevent.xconfigure.width != window->common.width ||
                xevent.xconfigure.height != window->common.height) {
                window->common.width = xevent.xconfigure.width;
                window->common.height = xevent.xconfigure.height;
                event->type = PODI_EVENT_WINDOW_RESIZE;
                event->window_resize.width = xevent.xconfigure.width;
                event->window_resize.height = xevent.xconfigure.height;
                return true;
            }
            break;
            
        case KeyPress: {
            KeySym keysym = XLookupKeysym(&xevent.xkey, 0);
            event->type = PODI_EVENT_KEY_DOWN;
            event->key.key = x11_keycode_to_podi_key(keysym);
            event->key.native_keycode = xevent.xkey.keycode;
            event->key.modifiers = x11_state_to_podi_modifiers(xevent.xkey.state);

            // Use proper Unicode input with composition support
            static char text_buffer[32];
            text_buffer[0] = '\0';
            event->key.text = NULL;

            podi_window_x11 *window = (podi_window_x11 *)event->window;
            int len = 0;
            Status status;

            if (window && window->input_context) {
                // Use Xutf8LookupString for proper Unicode and composition
                len = Xutf8LookupString(window->input_context, &xevent.xkey,
                                        text_buffer, sizeof(text_buffer) - 1,
                                        NULL, &status);
                if (status == XBufferOverflow) {
                    // Buffer too small, truncate
                    len = sizeof(text_buffer) - 1;
                }
            } else {
                // Fallback to XLookupString if no input context
                static XComposeStatus compose_status = {NULL, 0};
                KeySym lookup_sym;
                len = XLookupString(&xevent.xkey, text_buffer, sizeof(text_buffer) - 1,
                                    &lookup_sym, &compose_status);
            }

            if (len > 0) {
                text_buffer[len] = '\0';
                event->key.text = text_buffer;
            }

            return true;
        }
        
        case KeyRelease: {
            KeySym keysym = XLookupKeysym(&xevent.xkey, 0);
            event->type = PODI_EVENT_KEY_UP;
            event->key.key = x11_keycode_to_podi_key(keysym);
            event->key.native_keycode = xevent.xkey.keycode;
            event->key.modifiers = x11_state_to_podi_modifiers(xevent.xkey.state);
            event->key.text = NULL; // No text on key release
            return true;
        }
        
        case ButtonPress:
            switch (xevent.xbutton.button) {
                case Button1: case Button2: case Button3:
                    event->type = PODI_EVENT_MOUSE_BUTTON_DOWN;
                    switch (xevent.xbutton.button) {
                        case Button1: event->mouse_button.button = PODI_MOUSE_BUTTON_LEFT; break;
                        case Button2: event->mouse_button.button = PODI_MOUSE_BUTTON_MIDDLE; break;
                        case Button3: event->mouse_button.button = PODI_MOUSE_BUTTON_RIGHT; break;
                    }
                    return true;
                case Button4:
                    event->type = PODI_EVENT_MOUSE_SCROLL;
                    event->mouse_scroll.x = 0.0;
                    event->mouse_scroll.y = 1.0;
                    return true;
                case Button5:
                    event->type = PODI_EVENT_MOUSE_SCROLL;
                    event->mouse_scroll.x = 0.0;
                    event->mouse_scroll.y = -1.0;
                    return true;
                case 6:
                    event->type = PODI_EVENT_MOUSE_SCROLL;
                    event->mouse_scroll.x = 1.0;
                    event->mouse_scroll.y = 0.0;
                    return true;
                case 7:
                    event->type = PODI_EVENT_MOUSE_SCROLL;
                    event->mouse_scroll.x = -1.0;
                    event->mouse_scroll.y = 0.0;
                    return true;
                default: return false;
            }
            
        case ButtonRelease:
            event->type = PODI_EVENT_MOUSE_BUTTON_UP;
            switch (xevent.xbutton.button) {
                case Button1: event->mouse_button.button = PODI_MOUSE_BUTTON_LEFT; break;
                case Button2: event->mouse_button.button = PODI_MOUSE_BUTTON_MIDDLE; break;
                case Button3: event->mouse_button.button = PODI_MOUSE_BUTTON_RIGHT; break;
                default: return false;
            }
            return true;
            
        case MotionNotify:
            event->type = PODI_EVENT_MOUSE_MOVE;
            event->mouse_move.x = xevent.xmotion.x;
            event->mouse_move.y = xevent.xmotion.y;
            return true;
            
        case FocusIn:
            event->type = PODI_EVENT_WINDOW_FOCUS;
            return true;
            
        case FocusOut:
            event->type = PODI_EVENT_WINDOW_UNFOCUS;
            return true;

        case EnterNotify:
            event->type = PODI_EVENT_MOUSE_ENTER;
            return true;

        case LeaveNotify:
            event->type = PODI_EVENT_MOUSE_LEAVE;
            return true;
    }
    
    return false;
}

static podi_window *x11_window_create(podi_application *app_generic, const char *title, int width, int height) {
    podi_application_x11 *app = (podi_application_x11 *)app_generic;
    if (!app) return NULL;
    
    podi_window_x11 *window = calloc(1, sizeof(podi_window_x11));
    if (!window) return NULL;
    
    window->app = app;
    window->common.width = width;
    window->common.height = height;
    window->common.title = strdup(title ? title : "Podi Window");
    
    Window root = RootWindow(app->display, app->screen);
    window->window = XCreateSimpleWindow(app->display, root, 0, 0, width, height, 1,
                                        BlackPixel(app->display, app->screen),
                                        WhitePixel(app->display, app->screen));
    
    XSetWMProtocols(app->display, window->window, &app->wm_delete_window, 1);
    XStoreName(app->display, window->window, window->common.title);
    
    XSelectInput(app->display, window->window,
                 ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                 ButtonReleaseMask | PointerMotionMask | StructureNotifyMask |
                 FocusChangeMask | EnterWindowMask | LeaveWindowMask);
    
    XMapWindow(app->display, window->window);
    XFlush(app->display);

    // Create input context for proper composition support
    if (app->input_method) {
        window->input_context = XCreateIC(app->input_method,
                                          XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                                          XNClientWindow, window->window,
                                          NULL);
    } else {
        window->input_context = NULL;
    }
    
    if (app->common.window_count >= app->common.window_capacity) {
        size_t new_capacity = app->common.window_capacity ? app->common.window_capacity * 2 : 4;
        podi_window **new_windows = realloc(app->common.windows, new_capacity * sizeof(podi_window *));
        if (!new_windows) {
            XDestroyWindow(app->display, window->window);
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

static void x11_window_destroy(podi_window *window_generic) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    if (!window) return;
    
    podi_application_x11 *app = window->app;
    
    for (size_t i = 0; i < app->common.window_count; i++) {
        if (app->common.windows[i] == window_generic) {
            memmove(&app->common.windows[i], &app->common.windows[i + 1],
                   (app->common.window_count - i - 1) * sizeof(podi_window *));
            app->common.window_count--;
            break;
        }
    }
    
    if (window->input_context) {
        XDestroyIC(window->input_context);
    }
    
    XDestroyWindow(app->display, window->window);
    free(window->common.title);
    free(window);
}

static void x11_window_close(podi_window *window_generic) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    if (window) window->common.should_close = true;
}

static void x11_window_set_title(podi_window *window_generic, const char *title) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    if (!window || !title) return;
    
    free(window->common.title);
    window->common.title = strdup(title);
    XStoreName(window->app->display, window->window, title);
    XFlush(window->app->display);
}

static void x11_window_set_size(podi_window *window_generic, int width, int height) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    if (!window) return;
    
    window->common.width = width;
    window->common.height = height;
    XResizeWindow(window->app->display, window->window, width, height);
    XFlush(window->app->display);
}

static void x11_window_get_size(podi_window *window_generic, int *width, int *height) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    if (!window) return;
    
    if (width) *width = window->common.width;
    if (height) *height = window->common.height;
}

static bool x11_window_should_close(podi_window *window_generic) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    return window ? window->common.should_close : true;
}

const podi_platform_vtable x11_vtable = {
    .application_create = x11_application_create,
    .application_destroy = x11_application_destroy,
    .application_should_close = x11_application_should_close,
    .application_close = x11_application_close,
    .application_poll_event = x11_application_poll_event,
    .window_create = x11_window_create,
    .window_destroy = x11_window_destroy,
    .window_close = x11_window_close,
    .window_set_title = x11_window_set_title,
    .window_set_size = x11_window_set_size,
    .window_get_size = x11_window_get_size,
    .window_should_close = x11_window_should_close,
};

