#include "../include/podi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>

#define MAX_TEXT_LENGTH 1024


void print_usage(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  --help, -h          Show this help message\n");
    printf("\nThis demo shows all PODI API functionality with comprehensive event logging.\n");
    printf("It demonstrates window creation, event handling, and backend selection.\n");
    printf("\nBackend selection (Linux only):\n");
    printf("  PODI_BACKEND=x11 %s        # Force X11\n", program_name);
    printf("  PODI_BACKEND=wayland %s    # Force Wayland\n", program_name);
}

int demo_main(podi_application *app) {
    printf("=== PODI Comprehensive Demo ===\n");
    printf("Backend: %s\n", podi_get_backend_name());
    printf("Locale: %s\n", setlocale(LC_CTYPE, NULL));
    printf("Creating window...\n\n");

    podi_window *window = podi_window_create(app, "PODI Demo - Comprehensive API Example", 800, 600);
    if (!window) {
        printf("ERROR: Failed to create window\n");
        return -1;
    }

    printf("Window created successfully!\n");
    printf("Instructions:\n");
    printf("  - Type text to see input handling\n");
    printf("  - Move mouse, click, scroll to see mouse events\n");
    printf("  - Focus/unfocus window to see focus events\n");
    printf("  - Resize window to see resize events\n");
    printf("  - Press ESC or close button to exit\n");
    printf("  - Press ENTER to clear text buffer\n");
    printf("  - Press BACKSPACE to delete characters\n\n");

    char input_buffer[MAX_TEXT_LENGTH] = {0};
    int buffer_pos = 0;
    int event_count = 0;
    int mouse_move_count = 0;

    podi_event event;
    while (!podi_application_should_close(app) && !podi_window_should_close(window)) {
        while (podi_application_poll_event(app, &event)) {
            // Don't count mouse moves in main event counter to reduce noise
            if (event.type != PODI_EVENT_MOUSE_MOVE) {
                event_count++;
                printf("[Event %d] ", event_count);
            }

            switch (event.type) {
                case PODI_EVENT_WINDOW_CLOSE:
                    printf("WINDOW_CLOSE - Closing window\n");
                    if (event.window == window) {
                        podi_window_close(window);
                    }
                    break;

                case PODI_EVENT_WINDOW_RESIZE:
                    printf("WINDOW_RESIZE - New size: %dx%d\n",
                           event.window_resize.width, event.window_resize.height);
                    break;

                case PODI_EVENT_KEY_DOWN: {
                    const char *key_name = podi_get_key_name(event.key.key);
                    const char *modifiers_str = podi_get_modifiers_string(event.key.modifiers);

                    printf("KEY_DOWN - Key: %s (code: %d, native: %u)",
                           key_name, event.key.key, event.key.native_keycode);

                    if (strlen(modifiers_str) > 0) {
                        printf(" Modifiers: %s", modifiers_str);
                    }

                    if (event.key.text && strlen(event.key.text) > 0) {
                        printf(" Text: \"%s\"", event.key.text);

                        // Add text to buffer
                        int text_len = strlen(event.key.text);
                        if (buffer_pos + text_len < MAX_TEXT_LENGTH - 1) {
                            strcpy(input_buffer + buffer_pos, event.key.text);
                            buffer_pos += text_len;
                            printf(" -> Buffer: \"%s\"", input_buffer);
                        }
                    }
                    printf("\n");

                    // Handle special keys
                    if (event.key.key == PODI_KEY_ESCAPE) {
                        printf("  -> ESC pressed, exiting...\n");
                        podi_window_close(window);
                    } else if (event.key.key == PODI_KEY_BACKSPACE && buffer_pos > 0) {
                        buffer_pos--;
                        input_buffer[buffer_pos] = '\0';
                        printf("  -> Backspace, buffer: \"%s\"\n", input_buffer);
                    } else if (event.key.key == PODI_KEY_ENTER) {
                        printf("  -> Enter pressed, clearing buffer (was: \"%s\")\n", input_buffer);
                        buffer_pos = 0;
                        input_buffer[0] = '\0';
                    }
                    break;
                }

                case PODI_EVENT_KEY_UP: {
                    const char *key_name = podi_get_key_name(event.key.key);
                    const char *modifiers_str = podi_get_modifiers_string(event.key.modifiers);

                    printf("KEY_UP - Key: %s (code: %d, native: %u)",
                           key_name, event.key.key, event.key.native_keycode);

                    if (strlen(modifiers_str) > 0) {
                        printf(" Modifiers: %s", modifiers_str);
                    }
                    printf("\n");
                    break;
                }

                case PODI_EVENT_MOUSE_BUTTON_DOWN:
                    printf("MOUSE_BUTTON_DOWN - Button: %s (%d)\n",
                           podi_get_mouse_button_name(event.mouse_button.button), event.mouse_button.button);
                    break;

                case PODI_EVENT_MOUSE_BUTTON_UP:
                    printf("MOUSE_BUTTON_UP - Button: %s (%d)\n",
                           podi_get_mouse_button_name(event.mouse_button.button), event.mouse_button.button);
                    break;

                case PODI_EVENT_MOUSE_MOVE:
                    // Only log mouse moves occasionally to avoid spam
                    mouse_move_count++;
                    if (mouse_move_count % 50 == 0) {
                        printf("[Mouse Move %d] MOUSE_MOVE - Position: (%.1f, %.1f) [logging every 50th move]\n",
                               mouse_move_count, event.mouse_move.x, event.mouse_move.y);
                    }
                    break;

                case PODI_EVENT_MOUSE_SCROLL:
                    printf("MOUSE_SCROLL - Delta: (%.2f, %.2f)\n",
                           event.mouse_scroll.x, event.mouse_scroll.y);
                    break;

                case PODI_EVENT_WINDOW_FOCUS:
                    printf("WINDOW_FOCUS - Window gained focus\n");
                    break;

                case PODI_EVENT_WINDOW_UNFOCUS:
                    printf("WINDOW_UNFOCUS - Window lost focus\n");
                    break;

                case PODI_EVENT_MOUSE_ENTER:
                    printf("MOUSE_ENTER - Mouse entered window\n");
                    break;

                case PODI_EVENT_MOUSE_LEAVE:
                    printf("MOUSE_LEAVE - Mouse left window\n");
                    break;

                default:
                    printf("UNKNOWN_EVENT - Type: %d\n", event.type);
                    break;
            }
        }
    }

    printf("\n=== Demo Summary ===\n");
    printf("Total events processed: %d\n", event_count);
    printf("Final text buffer: \"%s\"\n", input_buffer);
    printf("Backend used: %s\n", podi_get_backend_name());

    podi_window_destroy(window);
    return 0;
}

int main(int argc, char *argv[]) {
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            printf("Error: Unknown argument '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    printf("Starting PODI demo...\n");
    const char *backend_env = getenv("PODI_BACKEND");
    if (backend_env) {
        printf("PODI_BACKEND environment variable: %s\n", backend_env);
    } else {
        printf("PODI_BACKEND not set - using auto-detection\n");
    }

    return podi_main(demo_main);
}