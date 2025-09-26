#pragma once

#include "podi.h"
#include <stddef.h>

// Client-side decoration constants (logical pixels)
#define PODI_TITLE_BAR_HEIGHT 40

typedef struct podi_platform_vtable {
    podi_application *(*application_create)(void);
    void (*application_destroy)(podi_application *app);
    bool (*application_should_close)(podi_application *app);
    void (*application_close)(podi_application *app);
    bool (*application_poll_event)(podi_application *app, podi_event *event);
    float (*get_display_scale_factor)(podi_application *app);

    podi_window *(*window_create)(podi_application *app, const char *title, int width, int height);
    void (*window_destroy)(podi_window *window);
    void (*window_close)(podi_window *window);
    void (*window_set_title)(podi_window *window, const char *title);
    void (*window_set_size)(podi_window *window, int width, int height);
    void (*window_set_position_and_size)(podi_window *window, int x, int y, int width, int height);
    void (*window_get_size)(podi_window *window, int *width, int *height);
    void (*window_get_framebuffer_size)(podi_window *window, int *width, int *height);
    void (*window_get_surface_size)(podi_window *window, int *width, int *height);
    float (*window_get_scale_factor)(podi_window *window);
    bool (*window_should_close)(podi_window *window);
    void (*window_begin_interactive_resize)(podi_window *window, int edge);
    void (*window_begin_move)(podi_window *window);
    void (*window_set_cursor)(podi_window *window, podi_cursor_shape cursor);
    void (*window_set_cursor_mode)(podi_window *window, bool locked, bool visible);
    void (*window_get_cursor_position)(podi_window *window, double *x, double *y);

#ifdef PODI_PLATFORM_LINUX
    bool (*window_get_x11_handles)(podi_window *window, podi_x11_handles *handles);
    bool (*window_get_wayland_handles)(podi_window *window, podi_wayland_handles *handles);
#endif
} podi_platform_vtable;

extern const podi_platform_vtable *podi_platform;

typedef struct {
    bool should_close;
    size_t window_count;
    podi_window **windows;
    size_t window_capacity;
} podi_application_common;

typedef struct {
    podi_application_common common;
    bool should_close;
    char *title;
    int width, height;  // Total surface dimensions (including decorations)
    int content_width, content_height;  // Content area dimensions (excluding decorations)
    int x, y;  // Window position
    int min_width, min_height;
    float scale_factor;

    // Resize state
    bool is_resizing;
    podi_resize_edge resize_edge;
    double resize_start_x, resize_start_y;
    int resize_start_width, resize_start_height;
    int resize_start_window_x, resize_start_window_y;  // Window position at resize start
    double last_mouse_x, last_mouse_y;
    int resize_border_width;

    // Cursor state
    bool cursor_locked;
    bool cursor_visible;
    double cursor_center_x, cursor_center_y;  // Window center coordinates for locking
    double last_cursor_x, last_cursor_y;      // Last cursor position for delta calculation
    bool cursor_warping;                       // Flag to track when we're warping cursor (X11 only)
} podi_window_common;

void podi_init_platform(void);
void podi_cleanup_platform(void);

// Resize helper functions
podi_resize_edge podi_detect_resize_edge(podi_window *window, double x, double y);
podi_cursor_shape podi_resize_edge_to_cursor(podi_resize_edge edge);
bool podi_handle_resize_event(podi_window *window, podi_event *event);
