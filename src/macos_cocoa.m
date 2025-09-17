#import <Cocoa/Cocoa.h>
#include "internal.h"
#include <stdlib.h>
#include <string.h>

@interface PodiApplicationDelegate : NSObject <NSApplicationDelegate>
@end

@interface PodiWindow : NSWindow <NSWindowDelegate>
@property (nonatomic, assign) void *podiWindow;
@end

@interface PodiView : NSView
@property (nonatomic, assign) void *podiWindow;
@end

typedef struct {
    podi_application_common common;
    NSApplication *app;
    PodiApplicationDelegate *delegate;
    NSAutoreleasePool *pool;
} podi_application_cocoa;

typedef struct {
    podi_window_common common;
    podi_application_cocoa *app;
    PodiWindow *window;
    PodiView *view;
    NSMutableArray *eventQueue;
} podi_window_cocoa;

static podi_key cocoa_keycode_to_podi_key(unsigned short keyCode) {
    switch (keyCode) {
        case 0x00: return PODI_KEY_A;
        case 0x0B: return PODI_KEY_B;
        case 0x08: return PODI_KEY_C;
        case 0x02: return PODI_KEY_D;
        case 0x0E: return PODI_KEY_E;
        case 0x03: return PODI_KEY_F;
        case 0x05: return PODI_KEY_G;
        case 0x04: return PODI_KEY_H;
        case 0x22: return PODI_KEY_I;
        case 0x26: return PODI_KEY_J;
        case 0x28: return PODI_KEY_K;
        case 0x25: return PODI_KEY_L;
        case 0x2E: return PODI_KEY_M;
        case 0x2D: return PODI_KEY_N;
        case 0x1F: return PODI_KEY_O;
        case 0x23: return PODI_KEY_P;
        case 0x0C: return PODI_KEY_Q;
        case 0x0F: return PODI_KEY_R;
        case 0x01: return PODI_KEY_S;
        case 0x11: return PODI_KEY_T;
        case 0x20: return PODI_KEY_U;
        case 0x09: return PODI_KEY_V;
        case 0x0D: return PODI_KEY_W;
        case 0x07: return PODI_KEY_X;
        case 0x10: return PODI_KEY_Y;
        case 0x06: return PODI_KEY_Z;
        case 0x1D: return PODI_KEY_0;
        case 0x12: return PODI_KEY_1;
        case 0x13: return PODI_KEY_2;
        case 0x14: return PODI_KEY_3;
        case 0x15: return PODI_KEY_4;
        case 0x17: return PODI_KEY_5;
        case 0x16: return PODI_KEY_6;
        case 0x1A: return PODI_KEY_7;
        case 0x1C: return PODI_KEY_8;
        case 0x19: return PODI_KEY_9;
        case 0x31: return PODI_KEY_SPACE;
        case 0x24: return PODI_KEY_ENTER;
        case 0x35: return PODI_KEY_ESCAPE;
        case 0x33: return PODI_KEY_BACKSPACE;
        case 0x30: return PODI_KEY_TAB;
        case 0x38: case 0x3C: return PODI_KEY_SHIFT;
        case 0x3B: case 0x3E: return PODI_KEY_CTRL;
        case 0x3A: case 0x3D: return PODI_KEY_ALT;
        case 0x7E: return PODI_KEY_UP;
        case 0x7D: return PODI_KEY_DOWN;
        case 0x7B: return PODI_KEY_LEFT;
        case 0x7C: return PODI_KEY_RIGHT;
        default: return PODI_KEY_UNKNOWN;
    }
}

@implementation PodiApplicationDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return NO;
}

@end

@implementation PodiWindow

- (void)windowWillClose:(NSNotification *)notification {
    podi_window_cocoa *window = (podi_window_cocoa *)self.podiWindow;
    if (window) {
        podi_event *event = malloc(sizeof(podi_event));
        event->type = PODI_EVENT_WINDOW_CLOSE;
        event->window = (podi_window *)window;
        [window->eventQueue addObject:[NSValue valueWithPointer:event]];
    }
}

- (void)windowDidResize:(NSNotification *)notification {
    podi_window_cocoa *window = (podi_window_cocoa *)self.podiWindow;
    if (window) {
        NSRect frame = [self.contentView frame];
        window->common.width = (int)frame.size.width;
        window->common.height = (int)frame.size.height;
        
        podi_event *event = malloc(sizeof(podi_event));
        event->type = PODI_EVENT_WINDOW_RESIZE;
        event->window = (podi_window *)window;
        event->window_resize.width = (int)frame.size.width;
        event->window_resize.height = (int)frame.size.height;
        [window->eventQueue addObject:[NSValue valueWithPointer:event]];
    }
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
    podi_window_cocoa *window = (podi_window_cocoa *)self.podiWindow;
    if (window) {
        podi_event *event = malloc(sizeof(podi_event));
        event->type = PODI_EVENT_WINDOW_FOCUS;
        event->window = (podi_window *)window;
        [window->eventQueue addObject:[NSValue valueWithPointer:event]];
    }
}

- (void)windowDidResignKey:(NSNotification *)notification {
    podi_window_cocoa *window = (podi_window_cocoa *)self.podiWindow;
    if (window) {
        podi_event *event = malloc(sizeof(podi_event));
        event->type = PODI_EVENT_WINDOW_UNFOCUS;
        event->window = (podi_window *)window;
        [window->eventQueue addObject:[NSValue valueWithPointer:event]];
    }
}

@end

@implementation PodiView

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)keyDown:(NSEvent *)event {
    podi_window_cocoa *window = (podi_window_cocoa *)self.podiWindow;
    if (window) {
        podi_event *podiEvent = malloc(sizeof(podi_event));
        podiEvent->type = PODI_EVENT_KEY_DOWN;
        podiEvent->window = (podi_window *)window;
        podiEvent->key.key = cocoa_keycode_to_podi_key([event keyCode]);
        podiEvent->key.native_keycode = [event keyCode];
        
        // Get text input from the event
        NSString *characters = [event characters];
        if (characters && [characters length] > 0) {
            // Store the text in a static buffer (similar to X11 approach)
            static char text_buffer[32];
            const char *utf8String = [characters UTF8String];
            if (utf8String && strlen(utf8String) < sizeof(text_buffer)) {
                strcpy(text_buffer, utf8String);
                podiEvent->key.text = text_buffer;
            } else {
                podiEvent->key.text = NULL;
            }
        } else {
            podiEvent->key.text = NULL;
        }
        
        [window->eventQueue addObject:[NSValue valueWithPointer:podiEvent]];
    }
}

- (void)keyUp:(NSEvent *)event {
    podi_window_cocoa *window = (podi_window_cocoa *)self.podiWindow;
    if (window) {
        podi_event *podiEvent = malloc(sizeof(podi_event));
        podiEvent->type = PODI_EVENT_KEY_UP;
        podiEvent->window = (podi_window *)window;
        podiEvent->key.key = cocoa_keycode_to_podi_key([event keyCode]);
        podiEvent->key.native_keycode = [event keyCode];
        podiEvent->key.text = NULL; // No text on key release
        [window->eventQueue addObject:[NSValue valueWithPointer:podiEvent]];
    }
}

- (void)mouseDown:(NSEvent *)event {
    podi_window_cocoa *window = (podi_window_cocoa *)self.podiWindow;
    if (window) {
        podi_event *podiEvent = malloc(sizeof(podi_event));
        podiEvent->type = PODI_EVENT_MOUSE_BUTTON_DOWN;
        podiEvent->window = (podi_window *)window;
        podiEvent->mouse_button.button = PODI_MOUSE_BUTTON_LEFT;
        [window->eventQueue addObject:[NSValue valueWithPointer:podiEvent]];
    }
}

- (void)mouseUp:(NSEvent *)event {
    podi_window_cocoa *window = (podi_window_cocoa *)self.podiWindow;
    if (window) {
        podi_event *podiEvent = malloc(sizeof(podi_event));
        podiEvent->type = PODI_EVENT_MOUSE_BUTTON_UP;
        podiEvent->window = (podi_window *)window;
        podiEvent->mouse_button.button = PODI_MOUSE_BUTTON_LEFT;
        [window->eventQueue addObject:[NSValue valueWithPointer:podiEvent]];
    }
}

- (void)rightMouseDown:(NSEvent *)event {
    podi_window_cocoa *window = (podi_window_cocoa *)self.podiWindow;
    if (window) {
        podi_event *podiEvent = malloc(sizeof(podi_event));
        podiEvent->type = PODI_EVENT_MOUSE_BUTTON_DOWN;
        podiEvent->window = (podi_window *)window;
        podiEvent->mouse_button.button = PODI_MOUSE_BUTTON_RIGHT;
        [window->eventQueue addObject:[NSValue valueWithPointer:podiEvent]];
    }
}

- (void)rightMouseUp:(NSEvent *)event {
    podi_window_cocoa *window = (podi_window_cocoa *)self.podiWindow;
    if (window) {
        podi_event *podiEvent = malloc(sizeof(podi_event));
        podiEvent->type = PODI_EVENT_MOUSE_BUTTON_UP;
        podiEvent->window = (podi_window *)window;
        podiEvent->mouse_button.button = PODI_MOUSE_BUTTON_RIGHT;
        [window->eventQueue addObject:[NSValue valueWithPointer:podiEvent]];
    }
}

- (void)otherMouseDown:(NSEvent *)event {
    podi_window_cocoa *window = (podi_window_cocoa *)self.podiWindow;
    if (window) {
        podi_event *podiEvent = malloc(sizeof(podi_event));
        podiEvent->type = PODI_EVENT_MOUSE_BUTTON_DOWN;
        podiEvent->window = (podi_window *)window;
        podiEvent->mouse_button.button = PODI_MOUSE_BUTTON_MIDDLE;
        [window->eventQueue addObject:[NSValue valueWithPointer:podiEvent]];
    }
}

- (void)otherMouseUp:(NSEvent *)event {
    podi_window_cocoa *window = (podi_window_cocoa *)self.podiWindow;
    if (window) {
        podi_event *podiEvent = malloc(sizeof(podi_event));
        podiEvent->type = PODI_EVENT_MOUSE_BUTTON_UP;
        podiEvent->window = (podi_window *)window;
        podiEvent->mouse_button.button = PODI_MOUSE_BUTTON_MIDDLE;
        [window->eventQueue addObject:[NSValue valueWithPointer:podiEvent]];
    }
}

- (void)mouseMoved:(NSEvent *)event {
    podi_window_cocoa *window = (podi_window_cocoa *)self.podiWindow;
    if (window) {
        NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
        podi_event *podiEvent = malloc(sizeof(podi_event));
        podiEvent->type = PODI_EVENT_MOUSE_MOVE;
        podiEvent->window = (podi_window *)window;
        podiEvent->mouse_move.x = point.x;
        podiEvent->mouse_move.y = window->common.height - point.y;
        [window->eventQueue addObject:[NSValue valueWithPointer:podiEvent]];
    }
}

- (void)mouseDragged:(NSEvent *)event {
    [self mouseMoved:event];
}

- (void)rightMouseDragged:(NSEvent *)event {
    [self mouseMoved:event];
}

- (void)otherMouseDragged:(NSEvent *)event {
    [self mouseMoved:event];
}

- (void)scrollWheel:(NSEvent *)event {
    podi_window_cocoa *window = (podi_window_cocoa *)self.podiWindow;
    if (window) {
        podi_event *podiEvent = malloc(sizeof(podi_event));
        podiEvent->type = PODI_EVENT_MOUSE_SCROLL;
        podiEvent->window = (podi_window *)window;
        podiEvent->mouse_scroll.x = [event scrollingDeltaX];
        podiEvent->mouse_scroll.y = [event scrollingDeltaY];
        [window->eventQueue addObject:[NSValue valueWithPointer:podiEvent]];
    }
}

@end

static podi_application *cocoa_application_create(void) {
    podi_application_cocoa *app = calloc(1, sizeof(podi_application_cocoa));
    if (!app) return NULL;
    
    app->pool = [[NSAutoreleasePool alloc] init];
    
    app->app = [NSApplication sharedApplication];
    app->delegate = [[PodiApplicationDelegate alloc] init];
    [app->app setDelegate:app->delegate];
    
    return (podi_application *)app;
}

static void cocoa_application_destroy(podi_application *app_generic) {
    podi_application_cocoa *app = (podi_application_cocoa *)app_generic;
    if (!app) return;
    
    for (size_t i = 0; i < app->common.window_count; i++) {
        if (app->common.windows[i]) {
            podi_window_destroy(app->common.windows[i]);
        }
    }
    free(app->common.windows);
    
    [app->delegate release];
    [app->pool release];
    free(app);
}

static bool cocoa_application_should_close(podi_application *app_generic) {
    podi_application_cocoa *app = (podi_application_cocoa *)app_generic;
    return app ? app->common.should_close : true;
}

static void cocoa_application_close(podi_application *app_generic) {
    podi_application_cocoa *app = (podi_application_cocoa *)app_generic;
    if (app) app->common.should_close = true;
}

static bool cocoa_application_poll_event(podi_application *app_generic, podi_event *event) {
    podi_application_cocoa *app = (podi_application_cocoa *)app_generic;
    if (!app || !event) return false;
    
    @autoreleasepool {
        NSEvent *nsEvent = [app->app nextEventMatchingMask:NSEventMaskAny
                                                 untilDate:[NSDate distantPast]
                                                    inMode:NSDefaultRunLoopMode
                                                   dequeue:YES];
        if (nsEvent) {
            [app->app sendEvent:nsEvent];
        }
        
        for (size_t i = 0; i < app->common.window_count; i++) {
            podi_window_cocoa *window = (podi_window_cocoa *)app->common.windows[i];
            if (window && [window->eventQueue count] > 0) {
                NSValue *value = [window->eventQueue objectAtIndex:0];
                podi_event *queued_event = [value pointerValue];
                *event = *queued_event;
                free(queued_event);
                [window->eventQueue removeObjectAtIndex:0];
                return true;
            }
        }
    }
    
    return false;
}

static podi_window *cocoa_window_create(podi_application *app_generic, const char *title, int width, int height) {
    podi_application_cocoa *app = (podi_application_cocoa *)app_generic;
    if (!app) return NULL;
    
    podi_window_cocoa *window = calloc(1, sizeof(podi_window_cocoa));
    if (!window) return NULL;
    
    window->app = app;
    window->common.width = width;
    window->common.height = height;
    window->common.title = strdup(title ? title : "Podi Window");
    window->eventQueue = [[NSMutableArray alloc] init];
    
    @autoreleasepool {
        NSRect frame = NSMakeRect(100, 100, width, height);
        NSUInteger styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | 
                              NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
        
        window->window = [[PodiWindow alloc] initWithContentRect:frame
                                                       styleMask:styleMask
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        
        window->window.podiWindow = window;
        [window->window setDelegate:window->window];
        [window->window setTitle:[NSString stringWithUTF8String:window->common.title]];
        [window->window setAcceptsMouseMovedEvents:YES];
        
        window->view = [[PodiView alloc] initWithFrame:frame];
        window->view.podiWindow = window;
        [window->window setContentView:window->view];
        
        [window->window makeKeyAndOrderFront:nil];
    }
    
    if (app->common.window_count >= app->common.window_capacity) {
        size_t new_capacity = app->common.window_capacity ? app->common.window_capacity * 2 : 4;
        podi_window **new_windows = realloc(app->common.windows, new_capacity * sizeof(podi_window *));
        if (!new_windows) {
            [window->window close];
            [window->eventQueue release];
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

static void cocoa_window_destroy(podi_window *window_generic) {
    podi_window_cocoa *window = (podi_window_cocoa *)window_generic;
    if (!window) return;
    
    podi_application_cocoa *app = window->app;
    
    for (size_t i = 0; i < app->common.window_count; i++) {
        if (app->common.windows[i] == window_generic) {
            memmove(&app->common.windows[i], &app->common.windows[i + 1],
                   (app->common.window_count - i - 1) * sizeof(podi_window *));
            app->common.window_count--;
            break;
        }
    }
    
    @autoreleasepool {
        [window->window close];
        [window->eventQueue release];
    }
    
    free(window->common.title);
    free(window);
}

static void cocoa_window_close(podi_window *window_generic) {
    podi_window_cocoa *window = (podi_window_cocoa *)window_generic;
    if (window) window->common.should_close = true;
}

static void cocoa_window_set_title(podi_window *window_generic, const char *title) {
    podi_window_cocoa *window = (podi_window_cocoa *)window_generic;
    if (!window || !title) return;
    
    free(window->common.title);
    window->common.title = strdup(title);
    
    @autoreleasepool {
        [window->window setTitle:[NSString stringWithUTF8String:title]];
    }
}

static void cocoa_window_set_size(podi_window *window_generic, int width, int height) {
    podi_window_cocoa *window = (podi_window_cocoa *)window_generic;
    if (!window) return;
    
    window->common.width = width;
    window->common.height = height;
    
    @autoreleasepool {
        NSRect frame = [window->window frame];
        frame.size.width = width;
        frame.size.height = height;
        [window->window setFrame:frame display:YES];
    }
}

static void cocoa_window_get_size(podi_window *window_generic, int *width, int *height) {
    podi_window_cocoa *window = (podi_window_cocoa *)window_generic;
    if (!window) return;
    
    if (width) *width = window->common.width;
    if (height) *height = window->common.height;
}

static bool cocoa_window_should_close(podi_window *window_generic) {
    podi_window_cocoa *window = (podi_window_cocoa *)window_generic;
    return window ? window->common.should_close : true;
}

static const podi_platform_vtable cocoa_vtable = {
    .application_create = cocoa_application_create,
    .application_destroy = cocoa_application_destroy,
    .application_should_close = cocoa_application_should_close,
    .application_close = cocoa_application_close,
    .application_poll_event = cocoa_application_poll_event,
    .window_create = cocoa_window_create,
    .window_destroy = cocoa_window_destroy,
    .window_close = cocoa_window_close,
    .window_set_title = cocoa_window_set_title,
    .window_set_size = cocoa_window_set_size,
    .window_get_size = cocoa_window_get_size,
    .window_should_close = cocoa_window_should_close,
};

const podi_platform_vtable *podi_platform = &cocoa_vtable;

void podi_init_platform(void) {
}

void podi_cleanup_platform(void) {
}