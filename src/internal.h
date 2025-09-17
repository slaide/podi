#pragma once

#include "podi.h"
#include <stddef.h>

typedef struct podi_platform_vtable {
    podi_application *(*application_create)(void);
    void (*application_destroy)(podi_application *app);
    bool (*application_should_close)(podi_application *app);
    void (*application_close)(podi_application *app);
    bool (*application_poll_event)(podi_application *app, podi_event *event);
    
    podi_window *(*window_create)(podi_application *app, const char *title, int width, int height);
    void (*window_destroy)(podi_window *window);
    void (*window_close)(podi_window *window);
    void (*window_set_title)(podi_window *window, const char *title);
    void (*window_set_size)(podi_window *window, int width, int height);
    void (*window_get_size)(podi_window *window, int *width, int *height);
    bool (*window_should_close)(podi_window *window);

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
    int width, height;
} podi_window_common;

void podi_init_platform(void);
void podi_cleanup_platform(void);