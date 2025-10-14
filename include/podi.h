/**
 * @file podi.h
 * @brief Podi - Platform-Agnostic Window Management Library
 *
 * Podi provides a unified interface for creating and managing windows across
 * different platforms (X11, Wayland) on Linux. It handles window creation,
 * event processing, input handling, and display scaling.
 *
 * Key Features:
 * - Cross-platform window management (X11/Wayland)
 * - HiDPI/scaling support with automatic scale factor detection
 * - Comprehensive input handling (keyboard, mouse, scroll)
 * - Client-side decorations support for modern desktop environments
 * - Interactive window resizing and moving
 * - Fullscreen exclusive mode support
 * - Cursor locking and visibility control
 *
 * Usage Pattern:
 * 1. Create application with podi_application_create()
 * 2. Create windows with podi_window_create()
 * 3. Main loop: poll events with podi_application_poll_event()
 * 4. Clean up with podi_window_destroy() and podi_application_destroy()
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle to a Podi application context
 *
 * Represents the main application state and manages all windows.
 * Create with podi_application_create(), destroy with podi_application_destroy().
 */
typedef struct podi_application podi_application;

/**
 * @brief Opaque handle to a Podi window
 *
 * Represents a single window with its associated state and properties.
 * Create with podi_window_create(), destroy with podi_window_destroy().
 */
typedef struct podi_window podi_window;

/**
 * @brief Types of events that can be received from the window system
 *
 * These events are returned by podi_application_poll_event() and represent
 * different types of user input and window state changes.
 */
typedef enum {
    /** No event occurred (used internally) */
    PODI_EVENT_NONE = 0,

    /** User requested window close (e.g., clicked X button) */
    PODI_EVENT_WINDOW_CLOSE,

    /** Window was resized by user or system */
    PODI_EVENT_WINDOW_RESIZE,

    /** Window gained keyboard focus */
    PODI_EVENT_WINDOW_FOCUS,

    /** Window lost keyboard focus */
    PODI_EVENT_WINDOW_UNFOCUS,

    /** Key was pressed down */
    PODI_EVENT_KEY_DOWN,

    /** Key was released */
    PODI_EVENT_KEY_UP,

    /** Mouse button was pressed down */
    PODI_EVENT_MOUSE_BUTTON_DOWN,

    /** Mouse button was released */
    PODI_EVENT_MOUSE_BUTTON_UP,

    /** Mouse cursor moved within window */
    PODI_EVENT_MOUSE_MOVE,

    /** Mouse scroll wheel was used */
    PODI_EVENT_MOUSE_SCROLL,

    /** Mouse cursor entered window area */
    PODI_EVENT_MOUSE_ENTER,

    /** Mouse cursor left window area */
    PODI_EVENT_MOUSE_LEAVE
} podi_event_type;

/**
 * @brief Platform-agnostic keyboard key codes
 *
 * These key codes are normalized across different platforms and input methods.
 * Used in keyboard events (PODI_EVENT_KEY_DOWN, PODI_EVENT_KEY_UP).
 */
typedef enum {
    /** Unknown or unmapped key */
    PODI_KEY_UNKNOWN = 0,

    /* Alphabetic keys A-Z */
    PODI_KEY_A, PODI_KEY_B, PODI_KEY_C, PODI_KEY_D, PODI_KEY_E, PODI_KEY_F,
    PODI_KEY_G, PODI_KEY_H, PODI_KEY_I, PODI_KEY_J, PODI_KEY_K, PODI_KEY_L,
    PODI_KEY_M, PODI_KEY_N, PODI_KEY_O, PODI_KEY_P, PODI_KEY_Q, PODI_KEY_R,
    PODI_KEY_S, PODI_KEY_T, PODI_KEY_U, PODI_KEY_V, PODI_KEY_W, PODI_KEY_X,
    PODI_KEY_Y, PODI_KEY_Z,

    /* Numeric keys 0-9 */
    PODI_KEY_0, PODI_KEY_1, PODI_KEY_2, PODI_KEY_3, PODI_KEY_4,
    PODI_KEY_5, PODI_KEY_6, PODI_KEY_7, PODI_KEY_8, PODI_KEY_9,

    /* Whitespace and control keys */
    PODI_KEY_SPACE,      /** Spacebar */
    PODI_KEY_ENTER,      /** Return/Enter key */
    PODI_KEY_ESCAPE,     /** Escape key */
    PODI_KEY_BACKSPACE,  /** Backspace key */
    PODI_KEY_TAB,        /** Tab key */

    /* Modifier keys */
    PODI_KEY_SHIFT,      /** Any Shift key (left or right) */
    PODI_KEY_CTRL,       /** Any Control key (left or right) */
    PODI_KEY_ALT,        /** Any Alt key (left or right) */

    /* Arrow keys */
    PODI_KEY_UP,         /** Up arrow key */
    PODI_KEY_DOWN,       /** Down arrow key */
    PODI_KEY_LEFT,       /** Left arrow key */
    PODI_KEY_RIGHT       /** Right arrow key */
} podi_key;

/**
 * @brief Mouse button identifiers
 *
 * These identify which mouse button was pressed or released in mouse button events.
 */
typedef enum {
    PODI_MOUSE_BUTTON_LEFT = 0,    /** Left mouse button (primary) */
    PODI_MOUSE_BUTTON_RIGHT,       /** Right mouse button (secondary/context) */
    PODI_MOUSE_BUTTON_MIDDLE,      /** Middle mouse button (wheel click) */
    PODI_MOUSE_BUTTON_X1,          /** Extra mouse button 1 (back) */
    PODI_MOUSE_BUTTON_X2           /** Extra mouse button 2 (forward) */
} podi_mouse_button;

/**
 * @brief Keyboard modifier flags
 *
 * These flags can be combined to represent multiple modifier keys being held.
 * Used in keyboard events to indicate which modifier keys were active.
 */
typedef enum {
    PODI_MOD_SHIFT = 1 << 0,    /** Shift key held */
    PODI_MOD_CTRL  = 1 << 1,    /** Control key held */
    PODI_MOD_ALT   = 1 << 2,    /** Alt key held */
    PODI_MOD_SUPER = 1 << 3     /** Super/Windows/Cmd key held */
} podi_mod_flags;

/**
 * @brief Window resize edge identifiers
 *
 * Used for interactive window resizing to specify which edge or corner
 * the user is dragging. Values are flags that can be combined for corners.
 */
typedef enum {
    PODI_RESIZE_EDGE_NONE = 0,           /** No resize operation */
    PODI_RESIZE_EDGE_TOP = 1,            /** Top edge */
    PODI_RESIZE_EDGE_BOTTOM = 2,         /** Bottom edge */
    PODI_RESIZE_EDGE_LEFT = 4,           /** Left edge */
    PODI_RESIZE_EDGE_TOP_LEFT = 5,       /** Top-left corner */
    PODI_RESIZE_EDGE_BOTTOM_LEFT = 6,    /** Bottom-left corner */
    PODI_RESIZE_EDGE_RIGHT = 8,          /** Right edge */
    PODI_RESIZE_EDGE_TOP_RIGHT = 9,      /** Top-right corner */
    PODI_RESIZE_EDGE_BOTTOM_RIGHT = 10   /** Bottom-right corner */
} podi_resize_edge;

/**
 * @brief Cursor shape identifiers
 *
 * These define the visual appearance of the mouse cursor. Used with
 * podi_window_set_cursor() to provide visual feedback during operations.
 */
typedef enum {
    PODI_CURSOR_DEFAULT = 0,       /** Default arrow cursor */
    PODI_CURSOR_RESIZE_N,          /** Resize cursor pointing North (up) */
    PODI_CURSOR_RESIZE_S,          /** Resize cursor pointing South (down) */
    PODI_CURSOR_RESIZE_E,          /** Resize cursor pointing East (right) */
    PODI_CURSOR_RESIZE_W,          /** Resize cursor pointing West (left) */
    PODI_CURSOR_RESIZE_NE,         /** Resize cursor pointing Northeast (diagonal) */
    PODI_CURSOR_RESIZE_NW,         /** Resize cursor pointing Northwest (diagonal) */
    PODI_CURSOR_RESIZE_SE,         /** Resize cursor pointing Southeast (diagonal) */
    PODI_CURSOR_RESIZE_SW          /** Resize cursor pointing Southwest (diagonal) */
} podi_cursor_shape;

/**
 * @brief Event data structure
 *
 * Contains all information about a window system event. The event type
 * determines which field in the union contains valid data.
 */
typedef struct {
    /** Type of event that occurred */
    podi_event_type type;

    /** Window that generated this event */
    podi_window *window;

    /** Event-specific data (check type to determine which field is valid) */
    union {
        /** Window resize event data (PODI_EVENT_WINDOW_RESIZE) */
        struct {
            int width, height;    /** New window dimensions in pixels */
        } window_resize;

        /** Keyboard event data (PODI_EVENT_KEY_DOWN, PODI_EVENT_KEY_UP) */
        struct {
            podi_key key;             /** Normalized key code */
            uint32_t native_keycode;  /** Platform-specific key code */
            const char *text;         /** UTF-8 text generated (may be NULL) */
            uint32_t modifiers;       /** Active modifier keys (podi_mod_flags) */
        } key;

        /** Mouse button event data (PODI_EVENT_MOUSE_BUTTON_DOWN/UP) */
        struct {
            podi_mouse_button button; /** Which button was pressed/released */
        } mouse_button;

        /** Mouse movement event data (PODI_EVENT_MOUSE_MOVE) */
        struct {
            double x, y;              /** Absolute position within window */
            double delta_x, delta_y;  /** Movement since last event */
        } mouse_move;

        /** Mouse scroll event data (PODI_EVENT_MOUSE_SCROLL) */
        struct {
            double x, y;              /** Scroll amounts (usually only y is used) */
        } mouse_scroll;
    };
} podi_event;

/**
 * @brief Platform backend selection
 *
 * Allows explicit selection of which windowing system backend to use.
 */
typedef enum {
    PODI_BACKEND_AUTO = 0,    /** Automatically choose best available backend */
    PODI_BACKEND_X11,         /** Force use of X11 backend */
    PODI_BACKEND_WAYLAND      /** Force use of Wayland backend */
} podi_backend_type;

/**
 * @brief Application main function signature
 *
 * This is the signature for the main application function passed to podi_main().
 * The application should create windows, handle events, and return an exit code.
 *
 * @param app The Podi application instance
 * @return Exit code (0 for success, non-zero for error)
 */
typedef int (*podi_main_func)(podi_application *app);

/* =============================================================================
 * Backend Management Functions
 * ============================================================================= */

/**
 * @brief Set the preferred windowing system backend
 *
 * Must be called before podi_application_create() to take effect.
 * If not called, PODI_BACKEND_AUTO is used by default.
 *
 * @param backend The backend to use (AUTO, X11, or Wayland)
 */
void podi_set_backend(podi_backend_type backend);

/**
 * @brief Get the currently active backend type
 *
 * @return The backend type currently in use
 */
podi_backend_type podi_get_backend(void);

/**
 * @brief Get the name of the currently active backend
 *
 * @return String name of the backend ("X11", "Wayland", etc.)
 */
const char *podi_get_backend_name(void);

/* =============================================================================
 * Input Utility Functions
 * ============================================================================= */

/**
 * @brief Convert platform-specific keycode to normalized key
 *
 * @param native_keycode Platform-specific key code from event
 * @return Normalized podi_key value, or PODI_KEY_UNKNOWN if unmapped
 */
podi_key podi_translate_native_keycode(uint32_t native_keycode);

/**
 * @brief Get human-readable name for a key
 *
 * @param key The key to get the name for
 * @return String name of the key (e.g., "A", "Space", "Enter")
 */
const char *podi_get_key_name(podi_key key);

/**
 * @brief Get human-readable name for a mouse button
 *
 * @param button The mouse button to get the name for
 * @return String name of the button (e.g., "Left", "Right", "Middle")
 */
const char *podi_get_mouse_button_name(podi_mouse_button button);

/**
 * @brief Get human-readable string for modifier key flags
 *
 * @param modifiers Combined modifier flags (podi_mod_flags)
 * @return String describing active modifiers (e.g., "Ctrl+Shift")
 */
const char *podi_get_modifiers_string(uint32_t modifiers);

/* =============================================================================
 * Application Management Functions
 * ============================================================================= */

/**
 * @brief Create a new Podi application instance
 *
 * This initializes the windowing system and creates the main application context.
 * Must be called before creating any windows.
 *
 * @return New application instance, or NULL on failure
 */
podi_application *podi_application_create(void);

/**
 * @brief Destroy a Podi application instance
 *
 * Cleans up all resources and closes all windows associated with the application.
 * Should be called when the application is shutting down.
 *
 * @param app Application instance to destroy (may be NULL)
 */
void podi_application_destroy(podi_application *app);

/**
 * @brief Check if the application should close
 *
 * Returns true if the application has received a quit signal or all windows
 * have been closed.
 *
 * @param app Application instance
 * @return true if application should exit, false otherwise
 */
bool podi_application_should_close(podi_application *app);

/**
 * @brief Request application closure
 *
 * Signals that the application should close. This doesn't immediately exit,
 * but causes podi_application_should_close() to return true.
 *
 * @param app Application instance
 */
void podi_application_close(podi_application *app);

/**
 * @brief Poll for the next window system event
 *
 * This is the main event loop function. Call repeatedly to process user input
 * and window system events. Returns true if an event was available.
 *
 * @param app Application instance
 * @param event Pointer to event structure to fill (output parameter)
 * @return true if an event was retrieved, false if no events are pending
 */
bool podi_application_poll_event(podi_application *app, podi_event *event);

/**
 * @brief Get the display scale factor
 *
 * Returns the system's display scaling factor (1.0 for normal DPI,
 * 2.0 for 2x/HiDPI displays, etc.). Used for proper UI scaling.
 *
 * @param app Application instance
 * @return Display scale factor (typically 1.0, 1.25, 1.5, 2.0, etc.)
 */
float podi_get_display_scale_factor(podi_application *app);

/* =============================================================================
 * Window Management Functions
 * ============================================================================= */

/**
 * @brief Create a new window
 *
 * Creates a window with the specified title and initial size. The window will
 * be visible and ready to receive events immediately.
 *
 * @param app Application instance that will own this window
 * @param title Window title (UTF-8 encoded, may be NULL)
 * @param width Initial window width in pixels
 * @param height Initial window height in pixels
 * @return New window instance, or NULL on failure
 */
podi_window *podi_window_create(podi_application *app, const char *title, int width, int height);

/**
 * @brief Destroy a window
 *
 * Closes the window and frees all associated resources. The window handle
 * becomes invalid after this call.
 *
 * @param window Window to destroy (may be NULL)
 */
void podi_window_destroy(podi_window *window);

/**
 * @brief Request window closure
 *
 * Requests that the window be closed. This will generate a PODI_EVENT_WINDOW_CLOSE
 * event, allowing the application to handle the close request gracefully.
 *
 * @param window Window to close
 */
void podi_window_close(podi_window *window);

/**
 * @brief Set the window title
 *
 * @param window Window to modify
 * @param title New title (UTF-8 encoded, may be NULL for no title)
 */
void podi_window_set_title(podi_window *window, const char *title);

/**
 * @brief Resize the window
 *
 * @param window Window to resize
 * @param width New width in pixels
 * @param height New height in pixels
 */
void podi_window_set_size(podi_window *window, int width, int height);

/**
 * @brief Set window position and size in one operation
 *
 * @param window Window to modify
 * @param x New X position on screen
 * @param y New Y position on screen
 * @param width New width in pixels
 * @param height New height in pixels
 */
void podi_window_set_position_and_size(podi_window *window, int x, int y, int width, int height);

/**
 * @brief Get the window size
 *
 * Returns the window's logical size in pixels. This may differ from the
 * framebuffer size on HiDPI displays.
 *
 * @param window Window to query
 * @param width Output: Window width (may be NULL)
 * @param height Output: Window height (may be NULL)
 */
void podi_window_get_size(podi_window *window, int *width, int *height);

/**
 * @brief Get the framebuffer size
 *
 * Returns the actual size of the window's framebuffer in pixels. This is what
 * should be used for setting up rendering contexts and viewports.
 *
 * @param window Window to query
 * @param width Output: Framebuffer width (may be NULL)
 * @param height Output: Framebuffer height (may be NULL)
 */
void podi_window_get_framebuffer_size(podi_window *window, int *width, int *height);

/**
 * @brief Get the surface size for rendering APIs
 *
 * Returns the size that should be used for creating rendering surfaces.
 * On some platforms this may differ from both window and framebuffer size.
 *
 * @param window Window to query
 * @param width Output: Surface width (may be NULL)
 * @param height Output: Surface height (may be NULL)
 */
void podi_window_get_surface_size(podi_window *window, int *width, int *height);

/**
 * @brief Get the window's scale factor
 *
 * Returns the scaling factor for this specific window. This may differ from
 * the global display scale factor if the window spans multiple monitors.
 *
 * @param window Window to query
 * @return Scale factor for this window (1.0, 1.25, 2.0, etc.)
 */
float podi_window_get_scale_factor(podi_window *window);

/**
 * @brief Check if window should close
 *
 * Returns true if the window has been marked for closure (e.g., user clicked
 * the close button, or podi_window_close() was called).
 *
 * @param window Window to check
 * @return true if window should be closed, false otherwise
 */
bool podi_window_should_close(podi_window *window);
/* =============================================================================
 * Window Interaction Functions
 * ============================================================================= */

/**
 * @brief Begin interactive window resize
 *
 * Starts an interactive resize operation, allowing the user to resize the window
 * by dragging from the specified edge. Call this in response to user interaction
 * with resize handles.
 *
 * @param window Window to resize
 * @param edge Which edge/corner to resize from (podi_resize_edge)
 */
void podi_window_begin_interactive_resize(podi_window *window, int edge);

/**
 * @brief Begin interactive window move
 *
 * Starts an interactive move operation, allowing the user to drag the window
 * around the screen. Typically called when user drags the title bar.
 *
 * @param window Window to move
 */
void podi_window_begin_move(podi_window *window);

/**
 * @brief Set the window's cursor shape
 *
 * Changes the cursor appearance when it's over this window. Used to provide
 * visual feedback about available operations (resize, clickable areas, etc.).
 *
 * @param window Window to affect
 * @param cursor New cursor shape to display
 */
void podi_window_set_cursor(podi_window *window, podi_cursor_shape cursor);

/**
 * @brief Set cursor lock and visibility mode
 *
 * Controls cursor behavior for applications like games or 3D editors that need
 * to capture mouse movement without the cursor moving on screen.
 *
 * @param window Window to affect
 * @param locked If true, cursor is locked to window center and reports delta movement only
 * @param visible If true, cursor is visible; if false, cursor is hidden
 */
void podi_window_set_cursor_mode(podi_window *window, bool locked, bool visible);

/**
 * @brief Get current cursor position within the window
 *
 * Returns the cursor position relative to the window's content area.
 * Coordinates are in pixels with (0,0) at the top-left.
 *
 * @param window Window to query
 * @param x Output: X coordinate (may be NULL)
 * @param y Output: Y coordinate (may be NULL)
 */
void podi_window_get_cursor_position(podi_window *window, double *x, double *y);

/**
 * @brief Set fullscreen exclusive mode
 *
 * Controls whether the window operates in fullscreen mode. In fullscreen mode,
 * the window typically takes over the entire screen and may change the display mode.
 *
 * @param window Window to modify
 * @param enabled true to enable fullscreen, false to return to windowed mode
 */
void podi_window_set_fullscreen_exclusive(podi_window *window, bool enabled);

/**
 * @brief Check if window is in fullscreen mode
 *
 * @param window Window to check
 * @return true if window is currently fullscreen, false otherwise
 */
bool podi_window_is_fullscreen_exclusive(podi_window *window);

/**
 * @brief Get the physical title bar height for client-side decorations
 *
 * Returns the height in physical pixels of the title bar area when client-side
 * decorations are active. This can be used for custom title bar rendering or
 * understanding the layout of the window framebuffer.
 *
 * @param window Window to query
 * @return Title bar height in physical pixels, or 0 if server-side decorations are used
 *
 * @note Returns 0 when server-side decorations are being used (title bar handled by window manager)
 * @note When non-zero, this represents the top portion of the framebuffer available for custom rendering
 * @note The returned value accounts for HiDPI scaling automatically
 */
int podi_window_get_title_bar_height(podi_window *window);

#ifdef PODI_PLATFORM_LINUX
/* =============================================================================
 * Platform-Specific Linux Functions
 * ============================================================================= */

/**
 * @brief X11 window handle structure
 *
 * Contains the native X11 handles needed for integration with X11-specific
 * libraries (OpenGL, Vulkan, etc.).
 */
typedef struct podi_x11_handles {
    void *display;        /** X11 Display pointer (cast to Display*) */
    unsigned long window; /** X11 Window ID */
} podi_x11_handles;

/**
 * @brief Wayland window handle structure
 *
 * Contains the native Wayland handles needed for integration with Wayland-specific
 * libraries (OpenGL, Vulkan, etc.).
 */
typedef struct podi_wayland_handles {
    void *display; /** Wayland display pointer (wl_display*) */
    void *surface; /** Wayland surface pointer (wl_surface*) */
} podi_wayland_handles;

/**
 * @brief Get X11 native window handles
 *
 * Retrieves the native X11 display and window ID for use with X11-specific APIs.
 * Only works when running on X11 backend.
 *
 * @param window Window to get handles for
 * @param handles Output: Structure to fill with X11 handles
 * @return true if handles were retrieved (X11 backend), false otherwise
 */
bool podi_window_get_x11_handles(podi_window *window, podi_x11_handles *handles);

/**
 * @brief Get Wayland native window handles
 *
 * Retrieves the native Wayland display and surface for use with Wayland-specific APIs.
 * Only works when running on Wayland backend.
 *
 * @param window Window to get handles for
 * @param handles Output: Structure to fill with Wayland handles
 * @return true if handles were retrieved (Wayland backend), false otherwise
 */
bool podi_window_get_wayland_handles(podi_window *window, podi_wayland_handles *handles);
#endif

/* =============================================================================
 * Application Entry Point
 * ============================================================================= */

/**
 * @brief Main entry point for Podi applications
 *
 * This function initializes Podi, calls the provided main function, and handles
 * cleanup. Use this instead of a regular main() function.
 *
 * Example usage:
 * @code
 * int my_main(podi_application *app) {
 *     // Create windows, handle events, etc.
 *     return 0;
 * }
 *
 * int main(void) {
 *     return podi_main(my_main);
 * }
 * @endcode
 *
 * @param main_func Your application's main function
 * @return Exit code from the main function, or error code if initialization failed
 */
int podi_main(podi_main_func main_func);

#ifdef __cplusplus
}
#endif
