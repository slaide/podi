/**
 * @file internal.h
 * @brief Internal Podi API and data structures
 *
 * This header contains internal implementation details for the Podi windowing library.
 * It defines platform abstraction layers, internal data structures, and utility functions
 * used by platform-specific backends.
 *
 * @warning This is an internal header and should NOT be used by applications.
 *          Applications should only include podi.h and use the public API.
 */

#pragma once

#include "podi.h"
#include <stddef.h>

/* =============================================================================
 * Constants and Configuration
 * ============================================================================= */

/**
 * @brief Height of client-side title bar in logical pixels
 *
 * This defines the standard height for title bars when using client-side
 * decorations (primarily on Wayland). The actual pixel height is this value
 * multiplied by the window's scale factor.
 */
#define PODI_TITLE_BAR_HEIGHT 30

/* =============================================================================
 * Platform Abstraction Layer
 * ============================================================================= */

/**
 * @brief Platform-specific function table (vtable)
 *
 * This structure contains function pointers to all platform-specific operations.
 * Different backends (X11, Wayland) provide their own implementations of these
 * functions, allowing the common Podi API to work across platforms.
 *
 * Each backend must implement all of these functions and set up this vtable
 * during initialization.
 */
typedef struct podi_platform_vtable {
    /* Application management functions */

    /**
     * @brief Create a new application instance for this platform
     *
     * Platform-specific application initialization. This function should set up
     * all necessary platform resources and return a new application instance.
     * Called once during podi_application_create().
     *
     * @return New platform-specific application instance, or NULL on failure
     */
    podi_application *(*application_create)(void);

    /**
     * @brief Destroy application and clean up platform resources
     *
     * Platform-specific cleanup function that releases all resources associated
     * with the application instance. Should close any remaining windows and
     * clean up platform-specific state. Called during podi_application_destroy().
     *
     * @param app Application instance to destroy (may be NULL)
     */
    void (*application_destroy)(podi_application *app);

    /**
     * @brief Check if application should close
     *
     * Platform-specific check for quit signals such as system shutdown,
     * user logout, or platform-specific close requests. This is separate
     * from window-specific close events.
     *
     * @param app Application instance to check
     * @return true if application received quit signal, false otherwise
     */
    bool (*application_should_close)(podi_application *app);

    /**
     * @brief Request application closure
     *
     * Platform-specific request to close the application. May post quit
     * events or set internal flags that will cause application_should_close
     * to return true on subsequent calls.
     *
     * @param app Application instance to close
     */
    void (*application_close)(podi_application *app);

    /**
     * @brief Poll for the next event from platform event system
     *
     * Platform-specific event polling that retrieves the next available
     * event from the windowing system. Should translate platform events
     * into normalized podi_event structures.
     *
     * @param app Application instance to poll events for
     * @param event Output: Event structure to fill with event data
     * @return true if an event was retrieved, false if no events pending
     */
    bool (*application_poll_event)(podi_application *app, podi_event *event);

    /**
     * @brief Get the platform's display scale factor
     *
     * Returns the system-wide display scaling factor used for HiDPI support.
     * This is the default scale factor that will be applied to new windows
     * unless overridden by per-window settings.
     *
     * @param app Application instance to query
     * @return Display scale factor (1.0 for normal DPI, 2.0 for 2x scaling, etc.)
     */
    float (*get_display_scale_factor)(podi_application *app);

    /* Window management functions */

    /**
     * @brief Create a new window using platform-specific APIs
     *
     * Platform-specific window creation that sets up a new window with the
     * specified properties. The window should be ready for rendering and
     * event processing after creation. The created window will always have
     * the requested physical size, with internal logical-to-physical conversion
     * handled automatically for platforms that use logical scaling.
     *
     * @param app Application instance that will own this window
     * @param title UTF-8 encoded window title text
     * @param width Initial window width in physical pixels
     * @param height Initial window height in physical pixels
     * @return New window instance on success, NULL on failure
     */
    podi_window *(*window_create)(podi_application *app, const char *title, int width, int height);

    /**
     * @brief Destroy window and free platform resources
     *
     * Platform-specific window cleanup that releases all resources associated
     * with the window instance. Should close the window handle, free memory,
     * and remove from any platform window lists. Safe to call with NULL.
     *
     * @param window Window instance to destroy (may be NULL)
     */
    void (*window_destroy)(podi_window *window);

    /**
     * @brief Request window closure (generates close event)
     *
     * Platform-specific request to close the window. Should generate a
     * close event that can be handled by the application rather than
     * immediately destroying the window. Allows applications to intercept
     * and potentially cancel close requests.
     *
     * @param window Window instance to request closure for
     */
    void (*window_close)(podi_window *window);

    /**
     * @brief Set window title using platform APIs
     *
     * Platform-specific title setting that updates the window's title bar
     * text using the native windowing system. The title should be UTF-8
     * encoded and will be displayed in the platform's standard location.
     *
     * @param window Window instance to update title for
     * @param title New UTF-8 encoded title text
     */
    void (*window_set_title)(podi_window *window, const char *title);

    /**
     * @brief Resize window using platform APIs
     *
     * Platform-specific window resizing that changes the window dimensions
     * to the specified physical size. May trigger resize events and should
     * update internal window state accordingly.
     *
     * @param window Window instance to resize
     * @param width New window width in physical pixels
     * @param height New window height in physical pixels
     */
    void (*window_set_size)(podi_window *window, int width, int height);

    /**
     * @brief Set window position and size atomically
     *
     * Platform-specific function to update both window position and size
     * in a single operation. This is more efficient than separate calls
     * and prevents intermediate resize events on some platforms.
     *
     * @param window Window instance to update
     * @param x New window X position in screen coordinates
     * @param y New window Y position in screen coordinates
     * @param width New window width in physical pixels
     * @param height New window height in physical pixels
     */
    void (*window_set_position_and_size)(podi_window *window, int x, int y, int width, int height);

    /**
     * @brief Get physical window size
     *
     * Platform-specific function to retrieve the current physical window size.
     * Applications should use this for all size tracking and calculations.
     *
     * @param window Window instance to query
     * @param width Output: Current physical window width in pixels
     * @param height Output: Current physical window height in pixels
     */
    void (*window_get_size)(podi_window *window, int *width, int *height);

    /**
     * @brief Get physical framebuffer size (important for HiDPI)
     *
     * Platform-specific function to retrieve the actual framebuffer size
     * in physical pixels.
     *
     * @param window Window instance to query
     * @param width Output: Physical framebuffer width in pixels
     * @param height Output: Physical framebuffer height in pixels
     */
    void (*window_get_framebuffer_size)(podi_window *window, int *width, int *height);

    /**
     * @brief Get rendering surface size (platform-specific)
     *
     * Platform-specific function to retrieve the rendering surface dimensions.
     * May differ from framebuffer size depending on platform rendering model
     * and compositor behavior.
     *
     * @param window Window instance to query
     * @param width Output: Rendering surface width in pixels
     * @param height Output: Rendering surface height in pixels
     */
    void (*window_get_surface_size)(podi_window *window, int *width, int *height);

    /**
     * @brief Get per-window scale factor
     *
     * Platform-specific function to retrieve the current scale factor for
     * this specific window. Used for HiDPI support and can change when
     * window moves between displays with different DPI settings.
     *
     * @param window Window instance to query
     * @return Scale factor (1.0 for normal DPI, 2.0 for 2x scaling, etc.)
     */
    float (*window_get_scale_factor)(podi_window *window);

    /**
     * @brief Check if window is marked for closure
     *
     * Platform-specific check for whether the window has received a close
     * request and should be closed. Applications should check this in their
     * main loop and handle closure appropriately.
     *
     * @param window Window instance to check
     * @return true if window should close, false otherwise
     */
    bool (*window_should_close)(podi_window *window);

    /**
     * @brief Start interactive resize from specified edge/corner
     *
     * Platform-specific function to begin user-driven window resizing from
     * the specified edge or corner. The platform will take control of resize
     * operations until the user releases the mouse button.
     *
     * @param window Window instance to resize
     * @param edge Resize edge identifier (PODI_RESIZE_EDGE_* constants)
     */
    void (*window_begin_interactive_resize)(podi_window *window, int edge);

    /**
     * @brief Start interactive window move (drag operation)
     *
     * Platform-specific function to begin user-driven window movement.
     * The platform will handle the drag operation until the user releases
     * the mouse button. Typically called when title bar is clicked.
     *
     * @param window Window instance to move
     */
    void (*window_begin_move)(podi_window *window);

    /**
     * @brief Set cursor shape for this window
     *
     * Platform-specific function to change the cursor shape when it's over
     * this window. The cursor will revert when leaving the window or when
     * changed by another window or application.
     *
     * @param window Window instance to set cursor for
     * @param cursor Cursor shape identifier (PODI_CURSOR_* constants)
     */
    void (*window_set_cursor)(podi_window *window, podi_cursor_shape cursor);

    /**
     * @brief Control cursor lock and visibility
     *
     * Platform-specific function to control cursor behavior within the window.
     * Can lock cursor to window center for FPS-style input and control
     * cursor visibility for immersive applications.
     *
     * @param window Window instance to control cursor for
     * @param locked true to lock cursor to window center, false for normal behavior
     * @param visible true to show cursor, false to hide cursor
     */
    void (*window_set_cursor_mode)(podi_window *window, bool locked, bool visible);

    /**
     * @brief Get current cursor position relative to window
     *
     * Platform-specific function to retrieve the current cursor position
     * in window-relative coordinates. Position is relative to window's
     * top-left corner and includes decorations if present.
     *
     * @param window Window instance to query cursor position for
     * @param x Output: Cursor X position relative to window
     * @param y Output: Cursor Y position relative to window
     */
    void (*window_get_cursor_position)(podi_window *window, double *x, double *y);

    /**
     * @brief Enable/disable fullscreen exclusive mode
     *
     * Platform-specific function to toggle fullscreen exclusive mode.
     * In exclusive mode, the window takes over the entire display and
     * may change display settings for optimal performance.
     *
     * @param window Window instance to set fullscreen mode for
     * @param enabled true to enable exclusive fullscreen, false to return to windowed
     */
    void (*window_set_fullscreen_exclusive)(podi_window *window, bool enabled);

    /**
     * @brief Check if window is currently fullscreen
     *
     * Platform-specific function to determine if the window is currently
     * in fullscreen exclusive mode. Used for state queries and UI updates.
     *
     * @param window Window instance to check
     * @return true if window is in exclusive fullscreen mode, false otherwise
     */
    bool (*window_is_fullscreen_exclusive)(podi_window *window);

    /**
     * @brief Get the physical title bar height for client-side decorations
     *
     * Platform-specific function to return the height of the title bar area
     * when client-side decorations are active. Returns 0 when server-side
     * decorations are being used by the window manager.
     *
     * @param window Window instance to query title bar height for
     * @return Title bar height in physical pixels, or 0 if server-side decorations are used
     */
    int (*window_get_title_bar_height)(podi_window *window);

#ifdef PODI_PLATFORM_LINUX
    /* Platform-specific handle retrieval */

    /**
     * @brief Get X11 native handles (Display*, Window) for graphics integration
     *
     * Platform-specific function to retrieve native X11 handles needed for
     * graphics API integration (Vulkan, OpenGL). Only available when running
     * on X11 backend and should return false on other backends.
     *
     * @param window Window instance to get X11 handles for
     * @param handles Output: Structure to fill with X11 Display and Window handles
     * @return true if X11 handles were retrieved successfully, false if not on X11
     */
    bool (*window_get_x11_handles)(podi_window *window, podi_x11_handles *handles);

    /**
     * @brief Get Wayland native handles (wl_display*, wl_surface*) for graphics integration
     *
     * Platform-specific function to retrieve native Wayland handles needed for
     * graphics API integration (Vulkan). Only available when running on Wayland
     * backend and should return false on other backends.
     *
     * @param window Window instance to get Wayland handles for
     * @param handles Output: Structure to fill with Wayland display and surface handles
     * @return true if Wayland handles were retrieved successfully, false if not on Wayland
     */
    bool (*window_get_wayland_handles)(podi_window *window, podi_wayland_handles *handles);
#endif
} podi_platform_vtable;

/**
 * @brief Global pointer to the active platform vtable
 *
 * This points to the vtable for the currently active backend (X11, Wayland, etc.).
 * Set during initialization and used by the public API functions to dispatch
 * to the correct platform-specific implementation.
 */
extern const podi_platform_vtable *podi_platform;

/* =============================================================================
 * Internal Data Structures
 * ============================================================================= */

/**
 * @brief Common application state shared across platforms
 *
 * This structure contains platform-independent application state that is
 * embedded in platform-specific application structures. It manages the
 * list of windows and application-wide state.
 */
typedef struct {
    /** True if application should exit (quit requested) */
    bool should_close;

    /** Number of currently open windows */
    size_t window_count;

    /** Array of pointers to all open windows */
    podi_window **windows;

    /** Allocated capacity of windows array */
    size_t window_capacity;
} podi_application_common;

/**
 * @brief Common window state shared across platforms
 *
 * This structure contains platform-independent window state that is embedded
 * in platform-specific window structures. It manages window properties,
 * resize operations, cursor state, and fullscreen mode.
 */
typedef struct {
    /** Common application state (must be first member) */
    podi_application_common common;

    /** True if window should close (close button clicked, etc.) */
    bool should_close;

    /** Window title (UTF-8 encoded, dynamically allocated) */
    char *title;

    /* Window dimensions and positioning */
    /** Total window width including decorations (pixels) */
    int width, height;

    /** Content area dimensions excluding decorations (pixels) */
    int content_width, content_height;

    /** Window position on screen (pixels from top-left) */
    int x, y;

    /** Minimum window size constraints (pixels) */
    int min_width, min_height;

    /** HiDPI scale factor for this window */
    float scale_factor;

    /* Interactive resize state */
    /** True if window is currently being resized by user */
    bool is_resizing;

    /** Which edge/corner is being dragged for resize */
    podi_resize_edge resize_edge;

    /** Mouse position when resize started (screen coordinates) */
    double resize_start_x, resize_start_y;

    /** Window size when resize started (for calculating deltas) */
    int resize_start_width, resize_start_height;

    /** Window position when resize started (for edge resize calculations) */
    int resize_start_window_x, resize_start_window_y;

    /** Last recorded mouse position during resize */
    double last_mouse_x, last_mouse_y;

    /** Width of resize border area in pixels */
    int resize_border_width;

    /* Cursor management state */
    /** True if cursor is locked to window center */
    bool cursor_locked;

    /** True if cursor should be visible */
    bool cursor_visible;

    /** Window center coordinates for cursor locking */
    double cursor_center_x, cursor_center_y;

    /** Previous cursor position for delta calculations */
    double last_cursor_x, last_cursor_y;

    /** True when cursor is being programmatically moved (X11 only) */
    bool cursor_warping;

    /* Fullscreen mode state */
    /** True if window is in fullscreen exclusive mode */
    bool fullscreen_exclusive;

    /** True if we have valid geometry to restore from fullscreen */
    bool restore_geometry_valid;

    /** Windowed mode geometry to restore when exiting fullscreen */
    int restore_x, restore_y;
    int restore_width, restore_height;
} podi_window_common;

/* =============================================================================
 * Platform Initialization Functions
 * ============================================================================= */

/**
 * @brief Initialize the platform-specific backend
 *
 * Sets up the platform vtable and initializes any global platform state.
 * Called once during Podi initialization to select and initialize the
 * appropriate backend (X11, Wayland, etc.).
 */
void podi_init_platform(void);

/**
 * @brief Clean up platform-specific backend
 *
 * Releases any global platform resources and resets the platform vtable.
 * Called during Podi shutdown to ensure clean exit.
 */
void podi_cleanup_platform(void);

/* =============================================================================
 * Window Resize Helper Functions
 * ============================================================================= */

/**
 * @brief Detect which resize edge is under cursor position
 *
 * Used for interactive window resizing to determine which edge or corner
 * the user is trying to drag based on cursor position relative to window borders.
 *
 * @param window Window to check against
 * @param x Cursor X position relative to window
 * @param y Cursor Y position relative to window
 * @return Resize edge identifier, or PODI_RESIZE_EDGE_NONE if not on border
 */
podi_resize_edge podi_detect_resize_edge(podi_window *window, double x, double y);

/**
 * @brief Get appropriate cursor shape for resize edge
 *
 * Returns the cursor shape that should be displayed when hovering over
 * a particular resize edge or corner to provide visual feedback.
 *
 * @param edge Resize edge identifier
 * @return Cursor shape for this resize operation
 */
podi_cursor_shape podi_resize_edge_to_cursor(podi_resize_edge edge);

/**
 * @brief Handle platform-independent resize event processing
 *
 * Common resize event handling logic that can be shared between platforms.
 * Updates window state and generates appropriate events.
 *
 * @param window Window being resized
 * @param event Resize-related event to process
 * @return true if event was handled, false otherwise
 */
bool podi_handle_resize_event(podi_window *window, podi_event *event);
