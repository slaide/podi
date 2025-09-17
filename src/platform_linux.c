#include "internal.h"
#include "podi.h"
#include <stdlib.h>
#include <string.h>

#ifdef PODI_BACKEND_BOTH
#include <X11/Xlib.h>
#elif !defined(PODI_BACKEND_WAYLAND_ONLY)
#include <X11/Xlib.h>
#endif

#ifndef PODI_BACKEND_WAYLAND_ONLY
extern const podi_platform_vtable x11_vtable;
#endif
#ifndef PODI_BACKEND_X11_ONLY
extern const podi_platform_vtable wayland_vtable;
#endif

static podi_backend_type selected_backend = PODI_BACKEND_AUTO;
const podi_platform_vtable *podi_platform = NULL;

#ifndef PODI_BACKEND_X11_ONLY
static bool wayland_available(void) {
    return getenv("WAYLAND_DISPLAY") != NULL;
}
#endif

#ifndef PODI_BACKEND_WAYLAND_ONLY
static bool x11_available(void) {
    return getenv("DISPLAY") != NULL;
}
#endif

void podi_set_backend(podi_backend_type backend) {
    selected_backend = backend;
    podi_platform = NULL;
}

podi_backend_type podi_get_backend(void) {
#ifndef PODI_BACKEND_WAYLAND_ONLY
    if (podi_platform == &x11_vtable) {
        return PODI_BACKEND_X11;
    }
#endif
#ifndef PODI_BACKEND_X11_ONLY
    if (podi_platform == &wayland_vtable) {
        return PODI_BACKEND_WAYLAND;
    }
#endif
    return selected_backend;
}

const char *podi_get_backend_name(void) {
    podi_backend_type backend = podi_get_backend();
    switch (backend) {
        case PODI_BACKEND_X11: return "X11";
        case PODI_BACKEND_WAYLAND: return "Wayland";
        case PODI_BACKEND_AUTO: return "Auto";
        default: return "Unknown";
    }
}

void podi_init_platform(void) {
    if (podi_platform) return;
    
    // Check environment variable for backend override
    const char *env_backend = getenv("PODI_BACKEND");
    if (env_backend && selected_backend == PODI_BACKEND_AUTO) {
        if (strcmp(env_backend, "x11") == 0 || strcmp(env_backend, "X11") == 0) {
            selected_backend = PODI_BACKEND_X11;
        } else if (strcmp(env_backend, "wayland") == 0 || strcmp(env_backend, "WAYLAND") == 0) {
            selected_backend = PODI_BACKEND_WAYLAND;
        }
    }
    
    switch (selected_backend) {
        case PODI_BACKEND_X11:
#ifndef PODI_BACKEND_WAYLAND_ONLY
            podi_platform = &x11_vtable;
#endif
            break;
            
        case PODI_BACKEND_WAYLAND:
#ifndef PODI_BACKEND_X11_ONLY
            podi_platform = &wayland_vtable;
#endif
            break;
            
        case PODI_BACKEND_AUTO:
        default:
#ifndef PODI_BACKEND_X11_ONLY
            if (wayland_available()) {
                podi_platform = &wayland_vtable;
            } else
#endif
#ifndef PODI_BACKEND_WAYLAND_ONLY
            if (x11_available()) {
                podi_platform = &x11_vtable;
            } else {
                podi_platform = &x11_vtable;
            }
#endif
            break;
    }
    
    // Initialize X11 if we're using it
#ifndef PODI_BACKEND_WAYLAND_ONLY
    if (podi_platform == &x11_vtable) {
        XInitThreads();
    }
#endif
}

void podi_cleanup_platform(void) {
}