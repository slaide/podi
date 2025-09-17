# Podi - Cross-Platform Window Management Library

Podi is a lightweight, cross-platform C23 library for window and input management, similar to GLFW. It provides a simple interface for creating applications with windows and handling user input across Linux (X11) and macOS (Cocoa).

## Supported Platforms

- **Linux**: x64 and ARM64 with X11 and Wayland
- **macOS**: ARM64 with Cocoa

## Features

- Application and window management
- Unified event system for keyboard, mouse, and window events
- Platform-specific entry point abstraction
- Simple, GLFW-like API
- Written in C23

## Building

### Prerequisites

**Linux (x64/ARM64):**
- GCC with C23 support
- X11 development libraries (`libx11-dev` on Ubuntu/Debian)
- Wayland development libraries (`libwayland-dev wayland-protocols` on Ubuntu/Debian)

**macOS (ARM64):**
- Xcode command line tools
- Clang with C23 support

### Build Instructions

```bash
# Clone and build the library
git clone <repository-url>
cd podi

# Build with both X11 and Wayland support (default)
make

# Build X11-only version
make x11

# Build Wayland-only version  
make wayland

# Build with debug symbols
make debug

# Build optimized release
make release

# Install system-wide (optional)
sudo make install
```

## Usage

### Basic Example

```c
#include "podi.h"
#include <stdio.h>

int my_main(podi_application *app) {
    podi_window *window = podi_window_create(app, "Hello Podi!", 800, 600);
    if (!window) {
        printf("Failed to create window\n");
        return -1;
    }
    
    while (!podi_application_should_close(app) && !podi_window_should_close(window)) {
        podi_event event;
        while (podi_application_poll_event(app, &event)) {
            switch (event.type) {
                case PODI_EVENT_WINDOW_CLOSE:
                    podi_window_close(window);
                    break;
                    
                case PODI_EVENT_KEY_DOWN:
                    if (event.key.key == PODI_KEY_ESCAPE) {
                        podi_window_close(window);
                    }
                    break;
                    
                // Handle other events...
                default:
                    break;
            }
        }
    }
    
    podi_window_destroy(window);
    return 0;
}

int main(void) {
    return podi_main(my_main);
}
```

### Backend Selection (Linux)

```c
#include "podi.h"

int my_main(podi_application *app) {
    // Explicitly choose X11
    podi_set_backend(PODI_BACKEND_X11);
    
    // Explicitly choose Wayland
    podi_set_backend(PODI_BACKEND_WAYLAND);
    
    // Auto-detect (default) - prefers Wayland if available
    podi_set_backend(PODI_BACKEND_AUTO);
    
    // Check which backend is being used
    printf("Using backend: %s\n", podi_get_backend_name());
    
    // Continue with window creation...
    podi_window *window = podi_window_create(app, "My Window", 800, 600);
    // ...
}
```

### Compiling Your Application

```bash
# Linux (both backends)
gcc -std=c23 your_app.c -lpodi -lX11 -lwayland-client -o your_app

# Linux (X11 only)
gcc -std=c23 your_app.c -lpodi -lX11 -o your_app

# Linux (Wayland only)
gcc -std=c23 your_app.c -lpodi -lwayland-client -o your_app

# macOS  
clang -std=c23 your_app.c -lpodi -framework Cocoa -o your_app
```

## API Reference

### Backend Selection (Linux)

- `void podi_set_backend(podi_backend_type backend)` - Choose backend (AUTO, X11, WAYLAND)
- `podi_backend_type podi_get_backend(void)` - Get current backend type
- `const char *podi_get_backend_name(void)` - Get current backend name

### Application Management

- `podi_application *podi_application_create(void)` - Create a new application instance
- `void podi_application_destroy(podi_application *app)` - Destroy application
- `bool podi_application_should_close(podi_application *app)` - Check if app should close
- `void podi_application_close(podi_application *app)` - Request application closure
- `bool podi_application_poll_event(podi_application *app, podi_event *event)` - Poll for events

### Window Management

- `podi_window *podi_window_create(podi_application *app, const char *title, int width, int height)` - Create window
- `void podi_window_destroy(podi_window *window)` - Destroy window
- `void podi_window_close(podi_window *window)` - Request window closure
- `void podi_window_set_title(podi_window *window, const char *title)` - Set window title
- `void podi_window_set_size(podi_window *window, int width, int height)` - Resize window
- `void podi_window_get_size(podi_window *window, int *width, int *height)` - Get window size
- `bool podi_window_should_close(podi_window *window)` - Check if window should close

### Entry Point

- `int podi_main(podi_main_func main_func)` - Platform-independent entry point

### Event Types

- `PODI_EVENT_WINDOW_CLOSE` - Window close button clicked
- `PODI_EVENT_WINDOW_RESIZE` - Window resized  
- `PODI_EVENT_WINDOW_FOCUS` - Window gained focus
- `PODI_EVENT_WINDOW_UNFOCUS` - Window lost focus
- `PODI_EVENT_KEY_DOWN/UP` - Keyboard input
- `PODI_EVENT_MOUSE_BUTTON_DOWN/UP` - Mouse button input  
- `PODI_EVENT_MOUSE_MOVE` - Mouse movement
- `PODI_EVENT_MOUSE_SCROLL` - Mouse scroll wheel (includes horizontal scroll)

## Architecture

Podi uses a platform abstraction layer with function pointers to provide a unified API across different platforms:

```
┌─────────────────┐
│   Public API    │  (podi.h)
├─────────────────┤
│  Common Logic   │  (podi.c)
├─────────────────┤
│ Platform Layer  │  (internal.h)
├─────────────────┤
│ Linux X11 │ macOS │  (linux_x11.c, macos_cocoa.m)
└─────────────────┘
```

## Event Handling Philosophy

Unlike some libraries that automatically handle window close events, Podi gives you full control. A `PODI_EVENT_WINDOW_CLOSE` is just a notification - your application decides whether to actually close the window or ignore the request.

## Contributing

Contributions are welcome! Please ensure:
- Code follows C23 standards
- All platforms are tested
- Documentation is updated

## License

[Specify your license here]