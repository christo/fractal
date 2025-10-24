// direct framebuffer rendering version

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <gpiod.h>

#define MAXI 360 
#define COLOUR_SCALE 18

// Mandelbrot parameters (now mutable for zoom/pan)
double scaling = 0.013;
double x_offset = 2.6;
double y_offset = 1.6;

// Runtime dimensions (determined from framebuffer)
int width = 0;
int height = 0;

// Touch device coordinate ranges (determined at runtime)
int touch_max_x = 4096;  // Default, will be queried
int touch_max_y = 4096;  // Default, will be queried

// Button GPIO definitions (mapped to actual hardware)
#define BUTTON_GPIO_1 23  // Physical button 1
#define BUTTON_GPIO_2 22  // Physical button 2
#define BUTTON_GPIO_3 27  // Physical button 3
#define BUTTON_GPIO_4 17  // Physical button 4 (may not be connected)
#define GPIO_CHIP "gpiochip0"

// Global variables for cleanup and shared state
int fb_fd = -1;
char* fbp = NULL;
long screensize = 0;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
volatile sig_atomic_t quit_flag = 0;
volatile sig_atomic_t redraw_flag = 0;
const char* fb_device = "/dev/fb1";  // Default to TFT display
const char* touch_device = "/dev/input/by-path/platform-3f204000.spi-cs-1-platform-stmpe-ts-event";  // Stable path to stmpe-ts touchscreen
pthread_mutex_t param_mutex = PTHREAD_MUTEX_INITIALIZER;

// Signal handler for Ctrl+C
void signal_handler(int sig __attribute__((unused))) {
    quit_flag = 1;
}

// HSB to RGB conversion function
void hsb_to_rgb(float h, float s, float b, uint8_t* r, uint8_t* g, uint8_t* bl) {
    float c = b * s;
    float x = c * (1 - fabs(fmod(h / 60.0, 2) - 1));
    float m = b - c;
    
    float r_prime, g_prime, b_prime;
    
    if (h >= 0 && h < 60) {
        r_prime = c; g_prime = x; b_prime = 0;
    } else if (h >= 60 && h < 120) {
        r_prime = x; g_prime = c; b_prime = 0;
    } else if (h >= 120 && h < 180) {
        r_prime = 0; g_prime = c; b_prime = x;
    } else if (h >= 180 && h < 240) {
        r_prime = 0; g_prime = x; b_prime = c;
    } else if (h >= 240 && h < 300) {
        r_prime = x; g_prime = 0; b_prime = c;
    } else {
        r_prime = c; g_prime = 0; b_prime = x;
    }
    
    *r = (uint8_t)((r_prime + m) * 255);
    *g = (uint8_t)((g_prime + m) * 255);
    *bl = (uint8_t)((b_prime + m) * 255);
}

// Calculate Mandelbrot iteration count for a given point
int mandelbrot_iterations(double u, double v) {
    double x = u;
    double y = v;
    int n = 0;
    double x_sq = 0;
    double y_sq = 0;
    
    while (x_sq + y_sq < 4.0 && n < MAXI) {
        x_sq = x * x;
        y_sq = y * y;
        y = 2 * x * y + v;
        x = x_sq - y_sq + u;
        n++;
    }
    
    return n;
}

// Set pixel directly in framebuffer
void set_pixel_fb(char* fbp, struct fb_var_screeninfo* vinfo, 
                  struct fb_fix_screeninfo* finfo, int x, int y, 
                  uint8_t r, uint8_t g, uint8_t b) {
    if (x >= 0 && x < (int)vinfo->xres && y >= 0 && y < (int)vinfo->yres) {
        long location = (x + vinfo->xoffset) * (vinfo->bits_per_pixel/8) + 
                       (y + vinfo->yoffset) * finfo->line_length;
        
        if (vinfo->bits_per_pixel == 32) {
            // 32-bit RGBA/BGRA
            *(fbp + location) = b;      // Blue
            *(fbp + location + 1) = g;  // Green  
            *(fbp + location + 2) = r;  // Red
            *(fbp + location + 3) = 255; // Alpha
        } else if (vinfo->bits_per_pixel == 16) {
            // 16-bit RGB565
            uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            *((uint16_t*)(fbp + location)) = color;
        } else if (vinfo->bits_per_pixel == 24) {
            // 24-bit RGB
            *(fbp + location) = b;      // Blue
            *(fbp + location + 1) = g;  // Green
            *(fbp + location + 2) = r;  // Red
        }
    }
}

// Draw crosshair at touched point (for visual feedback)
void draw_crosshair(char* fbp, struct fb_var_screeninfo* vinfo,
                    struct fb_fix_screeninfo* finfo, int x, int y) {
    // Draw vertical line through x (3 pixels wide)
    for (int j = 0; j < height; j++) {
        set_pixel_fb(fbp, vinfo, finfo, x, j, 0, 0, 0);
        if (x > 0) set_pixel_fb(fbp, vinfo, finfo, x - 1, j, 0, 0, 0);
        if (x < width - 1) set_pixel_fb(fbp, vinfo, finfo, x + 1, j, 0, 0, 0);
    }

    // Draw horizontal line through y (3 pixels wide)
    for (int i = 0; i < width; i++) {
        set_pixel_fb(fbp, vinfo, finfo, i, y, 0, 0, 0);
        if (y > 0) set_pixel_fb(fbp, vinfo, finfo, i, y - 1, 0, 0, 0);
        if (y < height - 1) set_pixel_fb(fbp, vinfo, finfo, i, y + 1, 0, 0, 0);
    }
}

// Zoom to a specific point
void zoom_to_point(int screen_x, int screen_y, double zoom_factor) {
    pthread_mutex_lock(&param_mutex);

    // Convert screen coordinates to complex plane coordinates using current transform
    // Formula from render: u = i * scaling - x_offset
    double u = screen_x * scaling - x_offset;
    double v = screen_y * scaling - y_offset;

    // Apply zoom (smaller scaling = more zoomed in)
    double new_scaling = scaling * zoom_factor;

    // Calculate new offsets to keep the complex point (u,v) at the same screen position
    // We want: u = screen_x * new_scaling - new_x_offset
    // Therefore: new_x_offset = screen_x * new_scaling - u
    double new_x_offset = screen_x * new_scaling - u;
    double new_y_offset = screen_y * new_scaling - v;

    // Update the global parameters
    scaling = new_scaling;
    x_offset = new_x_offset;
    y_offset = new_y_offset;

    pthread_mutex_unlock(&param_mutex);

    printf("Zoomed to point (%d, %d) -> complex (%.6f, %.6f), new scaling: %.6f\n",
           screen_x, screen_y, u, v, scaling);
    redraw_flag = 1;
}

// Query touch device capabilities to get coordinate ranges
void query_touch_capabilities(int touch_fd) {
    struct input_absinfo abs_x, abs_y;

    if (ioctl(touch_fd, EVIOCGABS(ABS_X), &abs_x) == 0) {
        touch_max_x = abs_x.maximum;
        printf("Touch X range: 0-%d\n", touch_max_x);
    } else {
        fprintf(stderr, "Warning: Could not query touch X range, using default %d\n", touch_max_x);
    }

    if (ioctl(touch_fd, EVIOCGABS(ABS_Y), &abs_y) == 0) {
        touch_max_y = abs_y.maximum;
        printf("Touch Y range: 0-%d\n", touch_max_y);
    } else {
        fprintf(stderr, "Warning: Could not query touch Y range, using default %d\n", touch_max_y);
    }
}

// Touch event handler thread
void* touch_handler(void* arg __attribute__((unused))) {
    int touch_fd = open(touch_device, O_RDONLY | O_NONBLOCK);
    if (touch_fd < 0) {
        fprintf(stderr, "Warning: Could not open touch device %s: %s\n",
                touch_device, strerror(errno));
        fprintf(stderr, "Touch input will be disabled.\n");
        return NULL;
    }

    printf("Touch device opened: %s\n", touch_device);

    // Query touch device capabilities
    query_touch_capabilities(touch_fd);

    struct input_event ev;
    int touch_x = -1, touch_y = -1;
    bool touch_active = false;
    while (!quit_flag) {
        ssize_t n = read(touch_fd, &ev, sizeof(ev));

        if (n == sizeof(ev)) {
            if (ev.type == EV_ABS) {
                if (ev.code == ABS_X) {
                    touch_x = ev.value;
                } else if (ev.code == ABS_Y) {
                    touch_y = ev.value;
                }
            } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
                if (ev.value == 1) {
                    // Touch pressed
                    touch_active = true;
                } else if (ev.value == 0 && touch_active) {
                    // Touch released - trigger zoom
                    if (touch_x >= 0 && touch_y >= 0) {
                        // Scale touch coordinates to screen coordinates
                        // Display is rotated 90 degrees, so transform coordinates
                        // For 90-degree counter-clockwise rotation: screen_x = max_y - touch_y, screen_y = touch_x
                        int screen_x = (touch_max_y - touch_y) * width / touch_max_y;
                        int screen_y = touch_x * height / touch_max_x;

                        if (screen_x >= 0 && screen_x < width &&
                            screen_y >= 0 && screen_y < height) {
                            printf("Touch detected at screen position (%d, %d)\n", screen_x, screen_y);
                            draw_crosshair(fbp, &vinfo, &finfo, screen_x, screen_y);
                            zoom_to_point(screen_x, screen_y, 0.9);  // Zoom in by 10%
                        }
                    }
                    touch_active = false;
                }
            }
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // Error reading (not just no data available)
            break;
        }

        usleep(10000);  // Sleep 10ms between reads
    }

    close(touch_fd);
    return NULL;
}

// Button handler thread using libgpiod v2 API
void* button_handler(void* arg __attribute__((unused))) {
    struct gpiod_chip *chip;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;
    struct gpiod_request_config *req_cfg;
    struct gpiod_line_request *request;
    unsigned int offsets[4] = {BUTTON_GPIO_1, BUTTON_GPIO_2, BUTTON_GPIO_3, BUTTON_GPIO_4};
    enum gpiod_line_value values[4];
    int prev_values[4] = {1, 1, 1, 1};  // Buttons are active-low (pressed = 0)

    // Open GPIO chip
    chip = gpiod_chip_open("/dev/" GPIO_CHIP);
    if (!chip) {
        fprintf(stderr, "Warning: Could not open GPIO chip %s: %s\n",
                GPIO_CHIP, strerror(errno));
        fprintf(stderr, "Button input will be disabled.\n");
        return NULL;
    }

    // Create line settings for input with pull-up
    settings = gpiod_line_settings_new();
    if (!settings) {
        fprintf(stderr, "Warning: Could not create line settings\n");
        gpiod_chip_close(chip);
        return NULL;
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_UP);

    // Create line config and add settings for all button lines
    line_cfg = gpiod_line_config_new();
    if (!line_cfg) {
        fprintf(stderr, "Warning: Could not create line config\n");
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return NULL;
    }

    for (int i = 0; i < 4; i++) {
        if (gpiod_line_config_add_line_settings(line_cfg, &offsets[i], 1, settings) < 0) {
            fprintf(stderr, "Warning: Could not add line %d to config\n", offsets[i]);
            gpiod_line_config_free(line_cfg);
            gpiod_line_settings_free(settings);
            gpiod_chip_close(chip);
            return NULL;
        }
    }

    // Create request config
    req_cfg = gpiod_request_config_new();
    if (!req_cfg) {
        fprintf(stderr, "Warning: Could not create request config\n");
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return NULL;
    }
    gpiod_request_config_set_consumer(req_cfg, "mandelbrot");

    // Request the lines
    request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
    if (!request) {
        fprintf(stderr, "Warning: Could not request GPIO lines: %s\n", strerror(errno));
        gpiod_request_config_free(req_cfg);
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return NULL;
    }

    printf("Button monitoring enabled on GPIOs %d, %d, %d, %d\n",
           BUTTON_GPIO_1, BUTTON_GPIO_2, BUTTON_GPIO_3, BUTTON_GPIO_4);

    // Monitor buttons
    while (!quit_flag) {
        // Read current button states
        if (gpiod_line_request_get_values(request, values) < 0) {
            fprintf(stderr, "Warning: Error reading GPIO values\n");
            break;
        }

        // Check for button press events (transition from 1 to 0, active-low)
        for (int i = 0; i < 4; i++) {
            if (prev_values[i] == 1 && values[i] == GPIOD_LINE_VALUE_INACTIVE) {
                // Button pressed
                printf("Button %d (GPIO %d) pressed\n", i + 1, offsets[i]);

                // Button actions
                switch(i) {
                    case 0:  // Button 1 (GPIO 17) - Zoom in to center
                        printf("  -> Zoom in to center\n");
                        zoom_to_point(width / 2, height / 2, 0.5);
                        break;
                    case 1:  // Button 2 (GPIO 22) - Zoom out from center
                        printf("  -> Zoom out from center\n");
                        zoom_to_point(width / 2, height / 2, 2.0);
                        break;
                    case 2:  // Button 3 (GPIO 23) - Reset to initial view
                        printf("  -> Reset view\n");
                        pthread_mutex_lock(&param_mutex);
                        scaling = 0.013;
                        x_offset = 2.6;
                        y_offset = 1.6;
                        pthread_mutex_unlock(&param_mutex);
                        redraw_flag = 1;
                        break;
                    case 3:  // Button 4 (GPIO 27) - Reserved for future use
                        printf("  -> Button 4 (reserved)\n");
                        break;
                }
            }
            prev_values[i] = (values[i] == GPIOD_LINE_VALUE_ACTIVE) ? 1 : 0;
        }

        usleep(50000);  // Sleep 50ms between reads (debouncing)
    }

    // Cleanup
    gpiod_line_request_release(request);
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    gpiod_chip_close(chip);

    return NULL;
}

// Cleanup function
void cleanup() {
    if (fbp && fbp != MAP_FAILED) {
        // Blank the framebuffer before exit
        memset(fbp, 0, screensize);
        munmap(fbp, screensize);
    }
    if (fb_fd >= 0) {
        close(fb_fd);
    }
    pthread_mutex_destroy(&param_mutex);
}

// Structure to pass parameters to render worker threads
typedef struct {
    char* fbp;
    struct fb_var_screeninfo* vinfo;
    struct fb_fix_screeninfo* finfo;
    int start_row;
    int end_row;
    double scaling;
    double x_offset;
    double y_offset;
} render_worker_args_t;

// Worker thread function for parallel Mandelbrot rendering
void* render_worker_thread(void* arg) {
    render_worker_args_t* args = (render_worker_args_t*)arg;

    // Render assigned rows
    for (int j = args->start_row; j < args->end_row && !quit_flag; j++) {
        for (int i = 0; i < width && !quit_flag; i++) {
            // Convert pixel coordinates to complex plane coordinates
            double u = i * args->scaling - args->x_offset;
            double v = j * args->scaling - args->y_offset;

            // Calculate iterations for this point
            int n = mandelbrot_iterations(u, v);

            // Calculate color based on iteration count
            if (n == MAXI) {
                // Point is in the set - color it black
                set_pixel_fb(args->fbp, args->vinfo, args->finfo, i, j, 0, 0, 0);
            } else {
                // Point escaped - color based on iteration count
                float hue = fmod((n * 360.0 * COLOUR_SCALE) / MAXI, 360.0);
                uint8_t r, g, b;
                hsb_to_rgb(hue, 1.0, 1.0, &r, &g, &b);
                set_pixel_fb(args->fbp, args->vinfo, args->finfo, i, j, r, g, b);
            }
        }
    }

    return NULL;
}

// Render Mandelbrot set to framebuffer (multi-threaded)
void render_mandelbrot(char* fbp, struct fb_var_screeninfo* vinfo,
                       struct fb_fix_screeninfo* finfo) {
    double local_scaling, local_x_offset, local_y_offset;
    struct timespec start_time, end_time;

    // Copy parameters with mutex protection
    pthread_mutex_lock(&param_mutex);
    local_scaling = scaling;
    local_x_offset = x_offset;
    local_y_offset = y_offset;
    pthread_mutex_unlock(&param_mutex);

    printf("Rendering Mandelbrot set (scaling=%.6f, x_off=%.6f, y_off=%.6f)...\n",
           local_scaling, local_x_offset, local_y_offset);

    // Start timing
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // Multi-threaded rendering: divide screen into horizontal bands
    #define NUM_RENDER_THREADS 4
    pthread_t render_threads[NUM_RENDER_THREADS];
    render_worker_args_t worker_args[NUM_RENDER_THREADS];

    // Calculate rows per thread
    int rows_per_thread = height / NUM_RENDER_THREADS;

    // Create worker threads
    for (int t = 0; t < NUM_RENDER_THREADS; t++) {
        worker_args[t].fbp = fbp;
        worker_args[t].vinfo = vinfo;
        worker_args[t].finfo = finfo;
        worker_args[t].start_row = t * rows_per_thread;
        worker_args[t].end_row = (t == NUM_RENDER_THREADS - 1) ? height : (t + 1) * rows_per_thread;
        worker_args[t].scaling = local_scaling;
        worker_args[t].x_offset = local_x_offset;
        worker_args[t].y_offset = local_y_offset;

        if (pthread_create(&render_threads[t], NULL, render_worker_thread, &worker_args[t]) != 0) {
            fprintf(stderr, "Error: Failed to create render thread %d\n", t);
        }
    }

    // Wait for all worker threads to complete
    for (int t = 0; t < NUM_RENDER_THREADS; t++) {
        pthread_join(render_threads[t], NULL);
    }

    // End timing and calculate elapsed time in milliseconds
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                      (end_time.tv_nsec - start_time.tv_nsec) / 1000000;

    printf("Render complete in %ld ms (4 threads).\n", elapsed_ms);
}

void print_usage(const char* prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -d, --device <device>  Framebuffer device (default: /dev/fb1)\n");
    printf("  -t, --touch <device>   Touch input device (default: /dev/input/event4)\n");
    printf("  -h, --help             Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s                     # Use TFT display (/dev/fb1)\n", prog_name);
    printf("  %s -d /dev/fb0         # Use HDMI display\n", prog_name);
    printf("  %s -t /dev/input/event0  # Use different touch device\n", prog_name);
}

int main(int argc, char* argv[]) {

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) {
            if (i + 1 < argc) {
                fb_device = argv[++i];
            } else {
                fprintf(stderr, "Error: -d/--device requires an argument\n");
                print_usage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--touch") == 0) {
            if (i + 1 < argc) {
                touch_device = argv[++i];
            } else {
                fprintf(stderr, "Error: -t/--touch requires an argument\n");
                print_usage(argv[0]);
                return 1;
            }
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    // Set up signal handler for Ctrl+C
    signal(SIGINT, signal_handler);

    // Open framebuffer device
    fb_fd = open(fb_device, O_RDWR);
    if (fb_fd < 0) {
        fprintf(stderr, "Error opening %s: ", fb_device);
        perror("");
        return 1;
    }
    
    // Get fixed screen information
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        perror("Error reading fixed information");
        close(fb_fd);
        return 1;
    }
    
    // Get variable screen information
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        perror("Error reading variable information");
        close(fb_fd);
        return 1;
    }
    
    // Set runtime dimensions from framebuffer
    width = vinfo.xres;
    height = vinfo.yres;

    printf("Framebuffer device: %s\n", fb_device);
    printf("  Display: %s\n", finfo.id);
    printf("  Resolution: %dx%d\n", width, height);
    printf("  Bits per pixel: %d\n", vinfo.bits_per_pixel);
    printf("  Line length: %d bytes\n", finfo.line_length);

    // Calculate screen size in bytes
    screensize = vinfo.yres * finfo.line_length;
    
    // Map framebuffer to memory
    fbp = (char*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fbp == MAP_FAILED) {
        perror("Error mapping framebuffer device to memory");
        close(fb_fd);
        return 1;
    }

    printf("\nGenerating Mandelbrot set (%dx%d)...\n", width, height);
    printf("Press Ctrl+C to exit.\n");
    printf("Touch screen to zoom in by 10%% at touched point.\n");
    printf("Buttons: 1=Zoom In Center, 2=Zoom Out Center, 3=Reset View, 4=Reserved\n");

    // Start touch handler thread
    pthread_t touch_thread;
    if (pthread_create(&touch_thread, NULL, touch_handler, NULL) != 0) {
        fprintf(stderr, "Warning: Failed to create touch handler thread\n");
    }

    // Start button handler thread
    pthread_t button_thread;
    if (pthread_create(&button_thread, NULL, button_handler, NULL) != 0) {
        fprintf(stderr, "Warning: Failed to create button handler thread\n");
    }

    // Initial render
    render_mandelbrot(fbp, &vinfo, &finfo);

    // Main event loop - wait for redraw requests or quit
    while (!quit_flag) {
        if (redraw_flag) {
            redraw_flag = 0;
            render_mandelbrot(fbp, &vinfo, &finfo);
        }
        usleep(50000); // Sleep 50ms
    }

    printf("\nExiting...\n");

    // Wait for threads to finish
    pthread_join(touch_thread, NULL);
    pthread_join(button_thread, NULL);

    // Cleanup
    cleanup();
    printf("Cleanup complete.\n");
    
    return 0;
}
