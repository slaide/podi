#define _POSIX_C_SOURCE 200809L
#include "internal.h"
#include "podi.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xresource.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#if defined(__has_include)
#  if __has_include(<X11/extensions/Xrandr.h>)
#    define PODI_HAS_XRANDR 1
#  else
#    define PODI_HAS_XRANDR 0
#  endif
#else
#  define PODI_HAS_XRANDR 1
#endif

#if PODI_HAS_XRANDR
#include <X11/extensions/Xrandr.h>
#endif
#if PODI_HAS_XRANDR
#include <dlfcn.h>
#endif
// Conditional XInput2 support - only include if available
#ifdef X11_XI2_AVAILABLE
#include <X11/extensions/XInput2.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <time.h>


#define NET_WM_MOVERESIZE_SIZE_TOPLEFT     0
#define NET_WM_MOVERESIZE_SIZE_TOP         1
#define NET_WM_MOVERESIZE_SIZE_TOPRIGHT    2
#define NET_WM_MOVERESIZE_SIZE_RIGHT       3
#define NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT 4
#define NET_WM_MOVERESIZE_SIZE_BOTTOM      5
#define NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT  6
#define NET_WM_MOVERESIZE_SIZE_LEFT        7
#define NET_WM_MOVERESIZE_MOVE             8
#define NET_WM_MOVERESIZE_SOURCE_APPLICATION 1

typedef struct {
    podi_application_common common;
    Display *display;
    int screen;
    Atom wm_delete_window;
    XIM input_method;
    Atom net_wm_moveresize;
    Atom net_active_window;
    Atom net_wm_state;
    Atom net_wm_state_fullscreen;
    Atom net_wm_bypass_compositor;
    int xi2_opcode;
    bool xi2_available;
    bool randr_available;
    int randr_event_base;
    int randr_error_base;
} podi_application_x11;

typedef struct {
    podi_window_common common;
    podi_application_x11 *app;
    Window window;
    XIC input_context;
    Cursor invisible_cursor;  // Store invisible cursor for cleanup
    bool has_focus;
    bool is_viewable;
    bool want_cursor_lock;
    bool pending_cursor_lock;
    bool xi2_raw_motion_selected;
    bool restore_crtc_valid;
#if PODI_HAS_XRANDR
    RROutput fullscreen_output;
    RRCrtc fullscreen_crtc;
    RRMode restore_mode;
    Rotation restore_rotation;
    int restore_crtc_x;
    int restore_crtc_y;
    int restore_crtc_width;
    int restore_crtc_height;
#endif
} podi_window_x11;

#if PODI_HAS_XRANDR
typedef struct {
    void *library;
    Bool (*query_extension)(Display *, int *, int *);
    Status (*query_version)(Display *, int *, int *);
    XRRScreenResources *(*get_screen_resources_current)(Display *, Window);
    XRROutputInfo *(*get_output_info)(Display *, XRRScreenResources *, RROutput);
    XRRCrtcInfo *(*get_crtc_info)(Display *, XRRScreenResources *, RRCrtc);
    void (*free_screen_resources)(XRRScreenResources *);
    void (*free_output_info)(XRROutputInfo *);
    void (*free_crtc_info)(XRRCrtcInfo *);
    Status (*set_crtc_config)(Display *, XRRScreenResources *, RRCrtc, Time,
                              int, int, RRMode, Rotation, RROutput *, int);
    RROutput (*get_output_primary)(Display *, Window);
} x11_randr_api;

static x11_randr_api g_xrandr = {0};
#endif

// Forward declarations

static void x11_window_lock_cursor_if_ready(podi_window_x11 *window);
static void x11_window_release_cursor(podi_window_x11 *window);
static void x11_request_window_focus(podi_window_x11 *window);
static void x11_enforce_cursor_bounds(podi_window_x11 *window);
static void x11_warp_pointer_to_center(podi_window_x11 *window);
#ifdef X11_XI2_AVAILABLE
static bool x11_enable_raw_motion(podi_window_x11 *window);
static void x11_disable_raw_motion(podi_window_x11 *window);
#endif
static void x11_window_set_fullscreen_exclusive(podi_window *window_generic, bool enabled);
static bool x11_window_is_fullscreen_exclusive(podi_window *window_generic);

static void x11_request_window_focus(podi_window_x11 *window) {
    if (!window || !window->app) return;

    Display *display = window->app->display;
    if (!display) return;

    Window root = RootWindow(display, window->app->screen);

    if (window->app->net_active_window != None) {
        XEvent event = {0};
        event.xclient.type = ClientMessage;
        event.xclient.message_type = window->app->net_active_window;
        event.xclient.display = display;
        event.xclient.window = window->window;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 1;
        event.xclient.data.l[1] = CurrentTime;
        event.xclient.data.l[2] = 0;
        event.xclient.data.l[3] = 0;
        event.xclient.data.l[4] = 0;

        XSendEvent(display, root, False,
                   SubstructureRedirectMask | SubstructureNotifyMask,
                   &event);
    }

    XRaiseWindow(display, window->window);
    XFlush(display);
}

static void x11_warp_pointer_to_center(podi_window_x11 *window) {
    if (!window || !window->app) return;

    Display *display = window->app->display;
    const int center_x = (int)window->common.cursor_center_x;
    const int center_y = (int)window->common.cursor_center_y;

    window->common.cursor_warping = true;
    XWarpPointer(display, None, window->window, 0, 0, 0, 0,
                 center_x, center_y);
    XFlush(display);

    window->common.last_cursor_x = center_x;
    window->common.last_cursor_y = center_y;
}

static void x11_enforce_cursor_bounds(podi_window_x11 *window) {
    if (!window || !(window->common.cursor_locked || window->want_cursor_lock)) return;

    Display *display = window->app->display;
    const int center_x = (int)window->common.cursor_center_x;
    const int center_y = (int)window->common.cursor_center_y;

    Window root, child;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;

    if (!XQueryPointer(display, window->window, &root, &child,
                       &root_x, &root_y, &win_x, &win_y, &mask)) {
        x11_warp_pointer_to_center(window);
        return;
    }

    const int margin = 10;
    const int width = window->common.width;
    const int height = window->common.height;

    if (width <= margin * 2 || height <= margin * 2) {
        return;
    }

    if (win_x < margin || win_x > width - margin ||
        win_y < margin || win_y > height - margin) {
        x11_warp_pointer_to_center(window);
        return;
    }

    const int center_threshold = 4;
    if (abs(win_x - center_x) > center_threshold ||
        abs(win_y - center_y) > center_threshold) {
        x11_warp_pointer_to_center(window);
    }
}

#define NET_WM_STATE_REMOVE 0
#define NET_WM_STATE_ADD 1

#if PODI_HAS_XRANDR
static const XRRModeInfo *x11_find_mode_info(const XRRScreenResources *resources, RRMode mode) {
    if (!resources) return NULL;
    for (int i = 0; i < resources->nmode; ++i) {
        if (resources->modes[i].id == mode) {
            return &resources->modes[i];
        }
    }
    return NULL;
}
#endif

static void x11_change_wm_fullscreen(podi_window_x11 *window, bool enable) {
    if (!window || !window->app) return;

    podi_application_x11 *app = window->app;
    if (app->net_wm_state == None || app->net_wm_state_fullscreen == None) {
        return;
    }

    XEvent event = {0};
    event.xclient.type = ClientMessage;
    event.xclient.serial = 0;
    event.xclient.send_event = True;
    event.xclient.display = app->display;
    event.xclient.window = window->window;
    event.xclient.message_type = app->net_wm_state;
    event.xclient.format = 32;
    event.xclient.data.l[0] = enable ? NET_WM_STATE_ADD : NET_WM_STATE_REMOVE;
    event.xclient.data.l[1] = app->net_wm_state_fullscreen;
    event.xclient.data.l[2] = 0;
    event.xclient.data.l[3] = 1;
    event.xclient.data.l[4] = 0;

    XSendEvent(app->display, RootWindow(app->display, app->screen), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);

    if (app->net_wm_bypass_compositor != None) {
        unsigned long bypass = enable ? 1UL : 0UL;
        XChangeProperty(app->display, window->window, app->net_wm_bypass_compositor,
                        XA_CARDINAL, 32, PropModeReplace,
                       (unsigned char *)&bypass, 1);
    }
}

#if PODI_HAS_XRANDR
static bool x11_load_randr_symbols(void) {
    if (g_xrandr.library) {
        return true;
    }

    const char *candidates[] = {
        "libXrandr.so.2",
        "libXrandr.so"
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        void *handle = dlopen(candidates[i], RTLD_NOW | RTLD_LOCAL);
        if (handle) {
            g_xrandr.library = handle;
            break;
        }
    }

    if (!g_xrandr.library) {
        return false;
    }

    g_xrandr.query_extension = (Bool (*)(Display *, int *, int *))dlsym(g_xrandr.library, "XRRQueryExtension");
    g_xrandr.query_version = (Status (*)(Display *, int *, int *))dlsym(g_xrandr.library, "XRRQueryVersion");
    g_xrandr.get_screen_resources_current = (XRRScreenResources *(*)(Display *, Window))dlsym(g_xrandr.library, "XRRGetScreenResourcesCurrent");
    g_xrandr.get_output_info = (XRROutputInfo *(*)(Display *, XRRScreenResources *, RROutput))dlsym(g_xrandr.library, "XRRGetOutputInfo");
    g_xrandr.get_crtc_info = (XRRCrtcInfo *(*)(Display *, XRRScreenResources *, RRCrtc))dlsym(g_xrandr.library, "XRRGetCrtcInfo");
    g_xrandr.free_screen_resources = (void (*)(XRRScreenResources *))dlsym(g_xrandr.library, "XRRFreeScreenResources");
    g_xrandr.free_output_info = (void (*)(XRROutputInfo *))dlsym(g_xrandr.library, "XRRFreeOutputInfo");
    g_xrandr.free_crtc_info = (void (*)(XRRCrtcInfo *))dlsym(g_xrandr.library, "XRRFreeCrtcInfo");
    g_xrandr.set_crtc_config = (Status (*)(Display *, XRRScreenResources *, RRCrtc, Time,
                                           int, int, RRMode, Rotation, RROutput *, int))
        dlsym(g_xrandr.library, "XRRSetCrtcConfig");
    g_xrandr.get_output_primary = (RROutput (*)(Display *, Window))dlsym(g_xrandr.library, "XRRGetOutputPrimary");

    if (!g_xrandr.query_extension || !g_xrandr.query_version ||
        !g_xrandr.get_screen_resources_current || !g_xrandr.get_output_info ||
        !g_xrandr.get_crtc_info || !g_xrandr.free_screen_resources ||
        !g_xrandr.free_output_info || !g_xrandr.free_crtc_info ||
        !g_xrandr.set_crtc_config || !g_xrandr.get_output_primary) {
        dlclose(g_xrandr.library);
        memset(&g_xrandr, 0, sizeof(g_xrandr));
        return false;
    }

    return true;
}
#else
#define x11_load_randr_symbols() false
#endif

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
    app->net_wm_moveresize = XInternAtom(app->display, "_NET_WM_MOVERESIZE", False);
    app->net_active_window = XInternAtom(app->display, "_NET_ACTIVE_WINDOW", False);
    app->net_wm_state = XInternAtom(app->display, "_NET_WM_STATE", False);
    app->net_wm_state_fullscreen = XInternAtom(app->display, "_NET_WM_STATE_FULLSCREEN", False);
    app->net_wm_bypass_compositor = XInternAtom(app->display, "_NET_WM_BYPASS_COMPOSITOR", False);

    app->randr_available = false;
#if PODI_HAS_XRANDR
    if (x11_load_randr_symbols()) {
        if (g_xrandr.query_extension(app->display, &app->randr_event_base, &app->randr_error_base)) {
            int randr_major = 1;
            int randr_minor = 5;
            if (g_xrandr.query_version(app->display, &randr_major, &randr_minor) == Success) {
                app->randr_available = true;
            }
        }
    }
#endif

    // Set locale to user's preference for proper text handling
    setlocale(LC_ALL, "");

    // Initialize input method for proper composition support
    if (XSupportsLocale()) {
        XSetLocaleModifiers("");
        app->input_method = XOpenIM(app->display, NULL, NULL, NULL);
    } else {
        app->input_method = NULL;
    }

    // Initialize XInput2 for raw mouse input (if available)
    app->xi2_available = false;
#ifdef X11_XI2_AVAILABLE
    int xi2_major = 2, xi2_minor = 0;
    if (XIQueryVersion(app->display, &xi2_major, &xi2_minor) == Success) {
        int xi2_event_base, xi2_error_base;
        if (XQueryExtension(app->display, "XInputExtension", &app->xi2_opcode,
                           &xi2_event_base, &xi2_error_base)) {
            app->xi2_available = true;
            printf("XInput2 initialized successfully (opcode=%d, version=%d.%d)\n",
                   app->xi2_opcode, xi2_major, xi2_minor);
        }
    }
#else
    printf("XInput2 not available at compile time - using XGrabPointer fallback\n");
#endif

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
    
    for (size_t i = 0; i < app->common.window_count; ++i) {
        podi_window_x11 *pending_window = (podi_window_x11 *)app->common.windows[i];
        if (pending_window) {
            x11_window_lock_cursor_if_ready(pending_window);
            x11_enforce_cursor_bounds(pending_window);
        }
    }

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

    // Handle XInput2 events (if available)
#ifdef X11_XI2_AVAILABLE
    if (xevent.type == GenericEvent && xevent.xcookie.extension == app->xi2_opcode) {
        if (XGetEventData(app->display, &xevent.xcookie)) {
            XIDeviceEvent *xidev = (XIDeviceEvent *)xevent.xcookie.data;

            if (xevent.xcookie.evtype == XI_RawMotion && window->common.cursor_locked) {
                XIRawEvent *raw = (XIRawEvent *)xevent.xcookie.data;

                // Extract raw motion values
                double *values = raw->valuators.values;
                double delta_x = 0.0, delta_y = 0.0;

                // Get raw delta values from valuators
                if (raw->valuators.mask_len > 0) {
                    int valuator_index = 0;
                    for (int i = 0; i < 2 && i < raw->valuators.mask_len * 8; i++) {
                        if (XIMaskIsSet(raw->valuators.mask, i)) {
                            if (i == 0) delta_x = values[valuator_index];      // X axis
                            else if (i == 1) delta_y = values[valuator_index]; // Y axis
                            valuator_index++;
                        }
                    }
                }

                event->type = PODI_EVENT_MOUSE_MOVE;
                event->mouse_move.x = window->common.cursor_center_x;
                event->mouse_move.y = window->common.cursor_center_y;
                event->mouse_move.delta_x = delta_x;
                event->mouse_move.delta_y = delta_y;

                x11_enforce_cursor_bounds(window);

                XFreeEventData(app->display, &xevent.xcookie);
                return true;
            }

            XFreeEventData(app->display, &xevent.xcookie);
        }
        return false; // XI2 event but not one we care about
    }
#endif

    switch (xevent.type) {
        case ClientMessage:
            if (xevent.xclient.data.l[0] == (long)app->wm_delete_window) {
                event->type = PODI_EVENT_WINDOW_CLOSE;
                return true;
            }
            break;

        case MapNotify:
            window->is_viewable = true;
            if (window->want_cursor_lock && !window->common.cursor_locked) {
                window->pending_cursor_lock = true;
                x11_window_lock_cursor_if_ready(window);
            }
            return false;

        case UnmapNotify:
            window->is_viewable = false;
            if (window->common.cursor_locked) {
                x11_window_release_cursor(window);
            }
            if (window->want_cursor_lock) {
                window->pending_cursor_lock = true;
            }
            return false;

        case DestroyNotify:
            window->is_viewable = false;
            window->want_cursor_lock = false;
            window->pending_cursor_lock = false;
            x11_window_release_cursor(window);
            return false;
            
        case ConfigureNotify: {
            int old_width = window->common.width;
            int old_height = window->common.height;

            window->common.width = xevent.xconfigure.width;
            window->common.height = xevent.xconfigure.height;
            window->common.x = xevent.xconfigure.x;
            window->common.y = xevent.xconfigure.y;

            if (xevent.xconfigure.width != old_width ||
                xevent.xconfigure.height != old_height) {
                // Update cursor center if cursor is locked
                if (window->common.cursor_locked) {
                    window->common.cursor_center_x = window->common.width / 2.0;
                    window->common.cursor_center_y = window->common.height / 2.0;
                }

                event->type = PODI_EVENT_WINDOW_RESIZE;
                event->window_resize.width = xevent.xconfigure.width;
                event->window_resize.height = xevent.xconfigure.height;
                return true;
            }
            break;
        }
            
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
                    // Let X11 window manager handle all resize operations natively
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
            // Let X11 window manager handle all resize operations natively
            return true;
            
        case MotionNotify: {
            event->type = PODI_EVENT_MOUSE_MOVE;

            // Find the window for cursor locking check
            podi_window_x11 *podi_window = NULL;
            for (size_t i = 0; i < app->common.window_count; i++) {
                podi_window_x11 *win = (podi_window_x11*)app->common.windows[i];
                if (win->window == xevent.xmotion.window) {
                    podi_window = win;
                    break;
                }
            }

            int motion_x = xevent.xmotion.x;
            int motion_y = xevent.xmotion.y;

            if (podi_window && podi_window->common.cursor_warping) {
                podi_window->common.cursor_warping = false;
                podi_window->common.last_cursor_x = motion_x;
                podi_window->common.last_cursor_y = motion_y;
                if (!app->xi2_available) {
                    return false;
                }
            }

            if (podi_window && podi_window->common.cursor_locked && !app->xi2_available) {
                // Only use old XWarpPointer method when XInput2 is not available
                // Calculate deltas from actual mouse position to center
                double center_x = podi_window->common.cursor_center_x;
                double center_y = podi_window->common.cursor_center_y;
                double delta_x = motion_x - center_x;
                double delta_y = motion_y - center_y;

                // Report actual mouse position
                event->mouse_move.x = motion_x;
                event->mouse_move.y = motion_y;
                event->mouse_move.delta_x = delta_x;
                event->mouse_move.delta_y = delta_y;

                // Always warp back to center when locked (aggressive warping)
                x11_warp_pointer_to_center(podi_window);
            } else if (podi_window && podi_window->common.cursor_locked && app->xi2_available) {
                // When XInput2 is available and cursor is locked, ignore regular motion events
                // XI_RawMotion events will handle relative motion instead
                podi_window->common.last_cursor_x = motion_x;
                podi_window->common.last_cursor_y = motion_y;
                return false;
            } else {
                // Normal unlocked mode - report actual position and calculate deltas from last position
                event->mouse_move.x = motion_x;
                event->mouse_move.y = motion_y;

                if (podi_window) {
                    event->mouse_move.delta_x = motion_x - podi_window->common.last_cursor_x;
                    event->mouse_move.delta_y = motion_y - podi_window->common.last_cursor_y;

                    // Update last position
                    podi_window->common.last_cursor_x = motion_x;
                    podi_window->common.last_cursor_y = motion_y;
                } else {
                    // Fallback if window not found
                    event->mouse_move.delta_x = 0.0;
                    event->mouse_move.delta_y = 0.0;
                }
            }

            return true;
        }
            
        case FocusIn:
            window->has_focus = true;
            if (window->want_cursor_lock && !window->common.cursor_locked) {
                window->pending_cursor_lock = true;
                x11_window_lock_cursor_if_ready(window);
            }
            event->type = PODI_EVENT_WINDOW_FOCUS;
            return true;
            
        case FocusOut: {
            bool focus_lost_to_other_window =
                (xevent.xfocus.mode == NotifyNormal || xevent.xfocus.mode == NotifyUngrab) &&
                (xevent.xfocus.detail == NotifyAncestor ||
                 xevent.xfocus.detail == NotifyNonlinear ||
                 xevent.xfocus.detail == NotifyNonlinearVirtual);

            if (focus_lost_to_other_window) {
                window->has_focus = false;
            }

            if (window->common.cursor_locked && focus_lost_to_other_window) {
                x11_window_release_cursor(window);
            }

            if (window->want_cursor_lock) {
                window->pending_cursor_lock = true;
            }

            event->type = PODI_EVENT_WINDOW_UNFOCUS;
            return true;
        }

        case EnterNotify: {
            // Find window and check if cursor is locked
            podi_window_x11 *window = NULL;
            for (size_t i = 0; i < app->common.window_count; i++) {
                podi_window_x11 *win = (podi_window_x11*)app->common.windows[i];
                if (win->window == xevent.xany.window) {
                    window = win;
                    break;
                }
            }

            // Skip enter events when cursor is locked
            if (window && window->common.cursor_locked) {
                return false;
            }

            event->type = PODI_EVENT_MOUSE_ENTER;
            return true;
        }

        case LeaveNotify: {
            // Find window and check if cursor is locked
            podi_window_x11 *window = NULL;
            for (size_t i = 0; i < app->common.window_count; i++) {
                podi_window_x11 *win = (podi_window_x11*)app->common.windows[i];
                if (win->window == xevent.xany.window) {
                    window = win;
                    break;
                }
            }

            // Skip leave events when cursor is locked
            if (window && window->common.cursor_locked) {
                return false;
            }

            event->type = PODI_EVENT_MOUSE_LEAVE;
            return true;
        }
    }

    return false;
}

static float x11_get_scale_factor(podi_application_x11 *app) {
    // Try multiple methods to detect HiDPI scaling

    // Method 1: Check environment variables
    const char *gdk_scale = getenv("GDK_SCALE");
    if (gdk_scale) {
        float scale = atof(gdk_scale);
        if (scale > 0.5f && scale <= 4.0f) {
            return scale;
        }
    }

    const char *qt_scale = getenv("QT_SCALE_FACTOR");
    if (qt_scale) {
        float scale = atof(qt_scale);
        if (scale > 0.5f && scale <= 4.0f) {
            return scale;
        }
    }

    // Method 2: Try Xft.dpi resource
    char *resource_string = XResourceManagerString(app->display);
    if (resource_string) {
        XrmDatabase database = XrmGetStringDatabase(resource_string);
        if (database) {
            char *type;
            XrmValue value;
            if (XrmGetResource(database, "Xft.dpi", "Xft.Dpi", &type, &value)) {
                if (value.addr) {
                    float dpi = atof(value.addr);
                    float scale = dpi / 96.0f;
                    XrmDestroyDatabase(database);
                    if (scale > 0.5f && scale <= 4.0f) {
                        return scale;
                    }
                }
            }
            XrmDestroyDatabase(database);
        }
    }

    // Method 3: Calculate scale factor based on physical DPI (fallback)
    int screen_width_mm = DisplayWidthMM(app->display, app->screen);
    int screen_width_px = DisplayWidth(app->display, app->screen);

    if (screen_width_mm > 0) {
        // Calculate DPI: pixels per inch = (pixels * 25.4) / mm
        float dpi = (float)(screen_width_px * 25.4f) / (float)screen_width_mm;
        float scale = dpi / 96.0f;

        // If DPI is exactly 96, this might be a scaled environment, try to detect actual scale
        if (dpi >= 95.0f && dpi <= 97.0f) {
            // Look for typical high-DPI display resolutions that would be scaled
            if (screen_width_px >= 2560) {
                return 2.0f;
            }
        }

        // Clamp to reasonable range and round to common scale factors
        if (scale >= 2.75f) return 3.0f;
        else if (scale >= 2.25f) return 2.5f;
        else if (scale >= 1.75f) return 2.0f;
        else if (scale >= 1.25f) return 1.5f;
        else return 1.0f;
    }

    return 1.0f;
}

static float x11_get_display_scale_factor(podi_application *app_generic) {
    podi_application_x11 *app = (podi_application_x11 *)app_generic;
    if (!app) return 1.0f;
    return x11_get_scale_factor(app);
}

static podi_window *x11_window_create(podi_application *app_generic, const char *title, int width, int height) {
    podi_application_x11 *app = (podi_application_x11 *)app_generic;
    if (!app) return NULL;
    
    podi_window_x11 *window = calloc(1, sizeof(podi_window_x11));
    if (!window) return NULL;
    
    window->app = app;
    window->common.width = width;
    window->common.height = height;
    window->common.x = 0;
    window->common.y = 0;
    window->common.min_width = width;
    window->common.min_height = height;
    window->common.scale_factor = x11_get_scale_factor(app);
    window->common.title = strdup(title ? title : "Podi Window");

    // Initialize resize state
    window->common.is_resizing = false;
    window->common.resize_edge = PODI_RESIZE_EDGE_NONE;
    window->common.resize_border_width = 8;

    // Initialize cursor state
    window->common.cursor_locked = false;
    window->common.cursor_visible = true;
    window->common.last_cursor_x = 0.0;
   window->common.last_cursor_y = 0.0;
   window->common.cursor_warping = false;
   window->invisible_cursor = None;

    window->common.fullscreen_exclusive = false;
    window->common.restore_geometry_valid = false;
    window->common.restore_x = 0;
    window->common.restore_y = 0;
    window->common.restore_width = width;
    window->common.restore_height = height;
    window->restore_crtc_valid = false;

    // Create window at the requested size (physical pixels)
    // Applications can use podi_get_display_scale_factor() to scale to logical size if desired

    Window root = RootWindow(app->display, app->screen);

    XSetWindowAttributes attrs = {0};
    attrs.background_pixmap = None;
    attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                       ButtonReleaseMask | PointerMotionMask | StructureNotifyMask |
                       FocusChangeMask | EnterWindowMask | LeaveWindowMask;
    attrs.bit_gravity = StaticGravity;
    attrs.win_gravity = StaticGravity;

    window->window = XCreateWindow(app->display, root,
                                   0, 0, width, height,
                                   0, CopyFromParent, InputOutput, CopyFromParent,
                                   CWBackPixmap | CWEventMask | CWBitGravity | CWWinGravity,
                                   &attrs);

    if (!window->window) {
        free(window->common.title);
        free(window);
        return NULL;
    }

    XSetWindowBackgroundPixmap(app->display, window->window, None);

    // Set reasonable window hints to allow resizing
    XSizeHints size_hints = {0};
    size_hints.flags = PWinGravity | PMinSize;
    size_hints.win_gravity = StaticGravity;
    size_hints.min_width = 100;  // Minimum reasonable size
    size_hints.min_height = 100;
    XSetWMNormalHints(app->display, window->window, &size_hints);

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

    if (window->common.fullscreen_exclusive) {
        x11_window_set_fullscreen_exclusive(window_generic, false);
    }

    podi_application_x11 *app = window->app;

    for (size_t i = 0; i < app->common.window_count; i++) {
        if (app->common.windows[i] == window_generic) {
            memmove(&app->common.windows[i], &app->common.windows[i + 1],
                   (app->common.window_count - i - 1) * sizeof(podi_window *));
            app->common.window_count--;
            break;
        }
    }

    window->want_cursor_lock = false;
    window->pending_cursor_lock = false;
    x11_window_release_cursor(window);

    // Clean up invisible cursor if created
    if (window->invisible_cursor != None) {
        XFreeCursor(app->display, window->invisible_cursor);
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

    window->common.min_width = width;
    window->common.min_height = height;
    window->common.width = width;
    window->common.height = height;
    XResizeWindow(window->app->display, window->window, width, height);

    // Update WM size hints with reasonable minimum size
    XSizeHints size_hints = {0};
    size_hints.flags = PMinSize;
    size_hints.min_width = 100;
    size_hints.min_height = 100;
    XSetWMNormalHints(window->app->display, window->window, &size_hints);
    XFlush(window->app->display);
}

static void x11_window_set_position_and_size(podi_window *window_generic, int x, int y, int width, int height) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    if (!window) return;

    window->common.x = x;
    window->common.y = y;
    window->common.width = width;
    window->common.height = height;
    XMoveResizeWindow(window->app->display, window->window, x, y, width, height);

    // Update WM size hints with reasonable minimum size
    XSizeHints size_hints = {0};
    size_hints.flags = PMinSize;
    size_hints.min_width = 100;
    size_hints.min_height = 100;
    XSetWMNormalHints(window->app->display, window->window, &size_hints);
    XFlush(window->app->display);
}

static void x11_window_get_size(podi_window *window_generic, int *width, int *height) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    if (!window) return;

    if (width) *width = window->common.width;
    if (height) *height = window->common.height;
}

static void x11_window_get_framebuffer_size(podi_window *window_generic, int *width, int *height) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    if (!window) return;

    // Since windows are created at physical size, framebuffer size equals window size
    if (width) *width = window->common.width;
    if (height) *height = window->common.height;
}

static float x11_window_get_scale_factor(podi_window *window_generic) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    return window ? window->common.scale_factor : 1.0f;
}

static bool x11_window_should_close(podi_window *window_generic) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    return window ? window->common.should_close : true;
}

static bool x11_window_get_x11_handles(podi_window *window_generic, podi_x11_handles *handles) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    if (!window || !handles) return false;

    handles->display = window->app->display;
    handles->window = window->window;
    return true;
}

static bool x11_window_get_wayland_handles(podi_window *window_generic, podi_wayland_handles *handles) {
    (void)window_generic;
    (void)handles;
    return false;
}

static void x11_window_set_cursor(podi_window *window_generic, podi_cursor_shape cursor) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    if (!window) return;

    Display *display = window->app->display;
    Window xwindow = window->window;

    Cursor x11_cursor;
    switch (cursor) {
        case PODI_CURSOR_RESIZE_N:
        case PODI_CURSOR_RESIZE_S:
            x11_cursor = XCreateFontCursor(display, XC_sb_v_double_arrow);
            break;
        case PODI_CURSOR_RESIZE_E:
        case PODI_CURSOR_RESIZE_W:
            x11_cursor = XCreateFontCursor(display, XC_sb_h_double_arrow);
            break;
        case PODI_CURSOR_RESIZE_NE:
        case PODI_CURSOR_RESIZE_SW:
            x11_cursor = XCreateFontCursor(display, XC_top_right_corner);
            break;
        case PODI_CURSOR_RESIZE_NW:
        case PODI_CURSOR_RESIZE_SE:
            x11_cursor = XCreateFontCursor(display, XC_top_left_corner);
            break;
        case PODI_CURSOR_DEFAULT:
        default:
            x11_cursor = XCreateFontCursor(display, XC_left_ptr);
            break;
    }

    XDefineCursor(display, xwindow, x11_cursor);
    XFreeCursor(display, x11_cursor);
    XFlush(display);
}

#ifdef X11_XI2_AVAILABLE
static bool x11_enable_raw_motion(podi_window_x11 *window) {
    if (!window || !window->app->xi2_available || window->xi2_raw_motion_selected) {
        return window && window->xi2_raw_motion_selected;
    }

    XIEventMask mask;
    unsigned char data[XIMaskLen(XI_LASTEVENT)] = {0};

    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = sizeof(data);
    mask.mask = data;

    XISetMask(data, XI_RawMotion);

    if (XISelectEvents(window->app->display,
                       DefaultRootWindow(window->app->display),
                       &mask, 1) == Success) {
        window->xi2_raw_motion_selected = true;
        printf("DEBUG: XInput2 XI_RawMotion events enabled for relative mouse mode\n");
        return true;
    }

    printf("ERROR: Failed to enable XInput2 XI_RawMotion events\n");
    return false;
}

static void x11_disable_raw_motion(podi_window_x11 *window) {
    if (!window || !window->app->xi2_available || !window->xi2_raw_motion_selected) {
        return;
    }

    XIEventMask mask;
    unsigned char data[XIMaskLen(XI_LASTEVENT)] = {0};

    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = sizeof(data);
    mask.mask = data;

    XISelectEvents(window->app->display,
                   DefaultRootWindow(window->app->display),
                   &mask, 1);
    window->xi2_raw_motion_selected = false;
    printf("DEBUG: XInput2 XI_RawMotion events disabled\n");
}
#endif

static void x11_window_release_cursor(podi_window_x11 *window) {
    if (!window) return;

#ifdef X11_XI2_AVAILABLE
    bool had_raw_motion = window->xi2_raw_motion_selected;
    x11_disable_raw_motion(window);
#else
    bool had_raw_motion = false;
#endif

    bool had_grab = window->common.cursor_locked;
    if (had_grab) {
        XUngrabPointer(window->app->display, CurrentTime);
    }

    window->common.cursor_locked = false;
    window->common.cursor_warping = false;

    if (had_grab || had_raw_motion) {
        XFlush(window->app->display);
    }
}

static void x11_window_lock_cursor_if_ready(podi_window_x11 *window) {
    if (!window || !window->pending_cursor_lock) {
        return;
    }

    if (!window->is_viewable) {
        return;
    }

    if (!window->has_focus) {
        x11_request_window_focus(window);
        return;
    }

    Display *display = window->app->display;
    Window xwindow = window->window;

    x11_warp_pointer_to_center(window);

    const int max_attempts = 4;
    int grab_result = GrabSuccess;
    bool grab_established = false;

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        grab_result = XGrabPointer(
            display, xwindow, True,
            PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
            GrabModeAsync, GrabModeAsync, xwindow,
            window->invisible_cursor, CurrentTime
        );

        if (grab_result == GrabSuccess) {
            grab_established = true;
            break;
        }

        if (grab_result == AlreadyGrabbed) {
            XUngrabPointer(display, CurrentTime);
        }

        printf("WARNING: XGrabPointer failed: %d (attempt %d)\n", grab_result, attempt + 1);

        struct timespec wait_time;
        wait_time.tv_sec = 0;
        wait_time.tv_nsec = 2000000L * (attempt + 1); // 2ms, 4ms, ...
        nanosleep(&wait_time, NULL);
    }

    if (!grab_established) {
        window->pending_cursor_lock = true;
        return;
    }

#ifdef X11_XI2_AVAILABLE
    x11_enable_raw_motion(window);
#endif

    window->common.cursor_locked = true;
    window->pending_cursor_lock = false;

    x11_warp_pointer_to_center(window);

    printf("DEBUG: Pointer grab established (raw motion %s)\n",
           window->xi2_raw_motion_selected ? "enabled" : "disabled");
}

static void x11_window_set_cursor_mode(podi_window *window_generic, bool locked, bool visible) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    if (!window) return;

    Display *display = window->app->display;
    Window xwindow = window->window;

    window->common.cursor_visible = visible;

    if (locked) {
        window->want_cursor_lock = true;
        window->pending_cursor_lock = !window->common.cursor_locked;

        // Calculate window center for cursor locking
        window->common.cursor_center_x = window->common.width / 2.0;
        window->common.cursor_center_y = window->common.height / 2.0;

        // Create invisible cursor first
        if (window->invisible_cursor == None) {
            // Create a truly invisible cursor (1x1 transparent bitmap)
            char data = 0;
            Pixmap blank = XCreateBitmapFromData(display, xwindow, &data, 1, 1);
            XColor dummy = {0};
            window->invisible_cursor = XCreatePixmapCursor(display, blank, blank, &dummy, &dummy, 0, 0);
            XFreePixmap(display, blank);
        }

        // Set invisible cursor on window
        XDefineCursor(display, xwindow, window->invisible_cursor);
        x11_window_lock_cursor_if_ready(window);
    } else {
        window->want_cursor_lock = false;
        window->pending_cursor_lock = false;

        x11_window_release_cursor(window);

        // Handle cursor visibility for non-locked mode
        if (!visible) {
            // Create temporary invisible cursor for non-locked mode
            char data = 0;
            Pixmap blank = XCreateBitmapFromData(display, xwindow, &data, 1, 1);
            XColor dummy = {0};
            Cursor temp_invisible = XCreatePixmapCursor(display, blank, blank, &dummy, &dummy, 0, 0);
            XDefineCursor(display, xwindow, temp_invisible);
            XFreeCursor(display, temp_invisible);
            XFreePixmap(display, blank);
        } else {
            // Restore default cursor
            x11_window_set_cursor(window_generic, PODI_CURSOR_DEFAULT);
        }
    }

    XFlush(display);
}

static void x11_window_get_cursor_position(podi_window *window_generic, double *x, double *y) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    if (!window || !x || !y) return;

    Display *display = window->app->display;
    Window xwindow = window->window;
    Window root, child;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;

    if (XQueryPointer(display, xwindow, &root, &child, &root_x, &root_y, &win_x, &win_y, &mask)) {
        *x = (double)win_x;
        *y = (double)win_y;
    } else {
        *x = 0.0;
        *y = 0.0;
    }
}

static void x11_window_set_fullscreen_exclusive(podi_window *window_generic, bool enabled) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    if (!window || !window->app) return;

    podi_application_x11 *app = window->app;
    Display *display = app->display;
#if PODI_HAS_XRANDR
    Window root = RootWindow(display, app->screen);
#endif

    if (enabled) {
        if (window->common.fullscreen_exclusive) {
            return;
        }

        XWindowAttributes attrs;
        if (XGetWindowAttributes(display, window->window, &attrs)) {
            window->common.restore_geometry_valid = true;
            window->common.restore_x = attrs.x;
            window->common.restore_y = attrs.y;
            window->common.restore_width = attrs.width;
            window->common.restore_height = attrs.height;
        } else {
            window->common.restore_geometry_valid = false;
        }

#if PODI_HAS_XRANDR
        if (app->randr_available) {
            XRRScreenResources *resources = g_xrandr.get_screen_resources_current(display, root);
            if (resources) {
                RROutput output = g_xrandr.get_output_primary(display, root);
                if (!output && resources->noutput > 0) {
                    output = resources->outputs[0];
                }

                window->restore_crtc_valid = false;

                if (output) {
                    XRROutputInfo *output_info = g_xrandr.get_output_info(display, resources, output);
                    if (output_info && output_info->connection == RR_Connected && output_info->crtc) {
                        XRRCrtcInfo *crtc_info = g_xrandr.get_crtc_info(display, resources, output_info->crtc);
                        if (crtc_info) {
                            window->restore_crtc_valid = true;
                            window->fullscreen_output = output;
                            window->fullscreen_crtc = output_info->crtc;
                            window->restore_mode = crtc_info->mode;
                            window->restore_rotation = crtc_info->rotation;
                            window->restore_crtc_x = crtc_info->x;
                            window->restore_crtc_y = crtc_info->y;
                            window->restore_crtc_width = crtc_info->width;
                            window->restore_crtc_height = crtc_info->height;

                            int screen_width = DisplayWidth(display, app->screen);
                            int screen_height = DisplayHeight(display, app->screen);

                            RRMode desired_mode = crtc_info->mode;
                            for (int i = 0; i < output_info->nmode; ++i) {
                                const XRRModeInfo *mode_info = x11_find_mode_info(resources, output_info->modes[i]);
                                if (!mode_info) continue;
                                if ((int)mode_info->width == screen_width &&
                                    (int)mode_info->height == screen_height) {
                                    desired_mode = mode_info->id;
                                    break;
                                }
                            }

                            if (desired_mode != crtc_info->mode) {
                                RROutput outputs[] = { output };
                                g_xrandr.set_crtc_config(display, resources, output_info->crtc, CurrentTime,
                                                         crtc_info->x, crtc_info->y, desired_mode,
                                                         crtc_info->rotation, outputs, 1);
                            }

                            g_xrandr.free_crtc_info(crtc_info);
                        }
                    }

                    if (output_info) {
                        g_xrandr.free_output_info(output_info);
                    }
                }

                g_xrandr.free_screen_resources(resources);
            }
        } else {
            window->restore_crtc_valid = false;
        }
#else
        window->restore_crtc_valid = false;
#endif

        int target_width = DisplayWidth(display, app->screen);
        int target_height = DisplayHeight(display, app->screen);

        x11_change_wm_fullscreen(window, true);

        XMoveResizeWindow(display, window->window, 0, 0,
                          (unsigned int)target_width, (unsigned int)target_height);
        XRaiseWindow(display, window->window);

        window->common.cursor_center_x = target_width / 2.0;
        window->common.cursor_center_y = target_height / 2.0;
        window->common.fullscreen_exclusive = true;
    } else {
        if (!window->common.fullscreen_exclusive) {
            return;
        }

        x11_change_wm_fullscreen(window, false);

#if PODI_HAS_XRANDR
        if (app->randr_available && window->restore_crtc_valid) {
            XRRScreenResources *resources = g_xrandr.get_screen_resources_current(display, root);
            if (resources) {
                RROutput outputs[] = { window->fullscreen_output };
                g_xrandr.set_crtc_config(display, resources, window->fullscreen_crtc, CurrentTime,
                                         window->restore_crtc_x, window->restore_crtc_y,
                                         window->restore_mode, window->restore_rotation,
                                         outputs, 1);
                g_xrandr.free_screen_resources(resources);
            }
            window->restore_crtc_valid = false;
        }
#else
        window->restore_crtc_valid = false;
#endif

        if (window->common.restore_geometry_valid) {
            XMoveResizeWindow(display, window->window,
                              window->common.restore_x,
                              window->common.restore_y,
                              (unsigned int)window->common.restore_width,
                              (unsigned int)window->common.restore_height);
        }

        window->common.fullscreen_exclusive = false;
    }

    XFlush(display);
}

static bool x11_window_is_fullscreen_exclusive(podi_window *window_generic) {
    podi_window_x11 *window = (podi_window_x11 *)window_generic;
    if (!window) {
        return false;
    }
    return window->common.fullscreen_exclusive;
}

static void x11_window_begin_interactive_resize(podi_window *window_generic, int edge) {
    (void)window_generic;
    (void)edge;
    // Let the window manager handle interactive resizes through native decorations
}

static void x11_window_begin_move(podi_window *window_generic) {
    (void)window_generic;
    // Let the window manager handle window moves through native decorations
}

const podi_platform_vtable x11_vtable = {
    .application_create = x11_application_create,
    .application_destroy = x11_application_destroy,
    .application_should_close = x11_application_should_close,
    .application_close = x11_application_close,
    .application_poll_event = x11_application_poll_event,
    .get_display_scale_factor = x11_get_display_scale_factor,
    .window_create = x11_window_create,
    .window_destroy = x11_window_destroy,
    .window_close = x11_window_close,
    .window_set_title = x11_window_set_title,
    .window_set_size = x11_window_set_size,
    .window_set_position_and_size = x11_window_set_position_and_size,
    .window_get_size = x11_window_get_size,
    .window_get_framebuffer_size = x11_window_get_framebuffer_size,
    .window_get_surface_size = x11_window_get_framebuffer_size,  // For X11, surface size = framebuffer size
    .window_get_scale_factor = x11_window_get_scale_factor,
    .window_should_close = x11_window_should_close,
    .window_begin_interactive_resize = x11_window_begin_interactive_resize,
    .window_begin_move = x11_window_begin_move,
    .window_set_cursor = x11_window_set_cursor,
    .window_set_cursor_mode = x11_window_set_cursor_mode,
    .window_get_cursor_position = x11_window_get_cursor_position,
    .window_set_fullscreen_exclusive = x11_window_set_fullscreen_exclusive,
    .window_is_fullscreen_exclusive = x11_window_is_fullscreen_exclusive,
#ifdef PODI_PLATFORM_LINUX
    .window_get_x11_handles = x11_window_get_x11_handles,
    .window_get_wayland_handles = x11_window_get_wayland_handles,
#endif
};
// Pointer barrier direction constants are available in newer XFixes headers.
