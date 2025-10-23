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

#define MAXI 360
#define COLOUR_SCALE 18
#define WIDTH 320
#define HEIGHT 240

// Mandelbrot parameters (now mutable for zoom/pan)
double scaling = 0.013;
double x_offset = 2.6;
double y_offset = 1.6;

// Global variables for cleanup
int fb_fd = -1;
char* fbp = NULL;
long screensize = 0;
volatile sig_atomic_t quit_flag = 0;
volatile sig_atomic_t redraw_flag = 0;
const char* fb_device = "/dev/fb1";  // Default to TFT display
const char* touch_device = "/dev/input/event4";  // Default touchscreen device (stmpe-ts)
pthread_mutex_t param_mutex = PTHREAD_MUTEX_INITIALIZER;

// Signal handler for Ctrl+C
void signal_handler(int sig) {
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
    if (x >= 0 && x < vinfo->xres && y >= 0 && y < vinfo->yres) {
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

// Touch event handler thread
void* touch_handler(void* arg) {
    int touch_fd = open(touch_device, O_RDONLY | O_NONBLOCK);
    if (touch_fd < 0) {
        fprintf(stderr, "Warning: Could not open touch device %s: %s\n",
                touch_device, strerror(errno));
        fprintf(stderr, "Touch input will be disabled.\n");
        return NULL;
    }

    printf("Touch device opened: %s\n", touch_device);

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
                        // Assuming touch coordinates need mapping (adjust if needed)
                        int screen_x = touch_x * WIDTH / 4096;  // Adjust divisor based on actual touch range
                        int screen_y = touch_y * HEIGHT / 4096;

                        if (screen_x >= 0 && screen_x < WIDTH &&
                            screen_y >= 0 && screen_y < HEIGHT) {
                            printf("Touch detected at screen position (%d, %d)\n", screen_x, screen_y);
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

// Cleanup function
void cleanup() {
    if (fbp && fbp != MAP_FAILED) {
        munmap(fbp, screensize);
    }
    if (fb_fd >= 0) {
        close(fb_fd);
    }
    pthread_mutex_destroy(&param_mutex);
}

// Render Mandelbrot set to framebuffer
void render_mandelbrot(char* fbp, struct fb_var_screeninfo* vinfo,
                       struct fb_fix_screeninfo* finfo) {
    double local_scaling, local_x_offset, local_y_offset;

    // Copy parameters with mutex protection
    pthread_mutex_lock(&param_mutex);
    local_scaling = scaling;
    local_x_offset = x_offset;
    local_y_offset = y_offset;
    pthread_mutex_unlock(&param_mutex);

    printf("Rendering Mandelbrot set (scaling=%.6f, x_off=%.6f, y_off=%.6f)...\n",
           local_scaling, local_x_offset, local_y_offset);

    // Clear screen (fill with black)
    memset(fbp, 0, screensize);

    // Generate Mandelbrot set
    for (int i = 0; i < WIDTH && !quit_flag; i++) {
        for (int j = 0; j < HEIGHT && !quit_flag; j++) {
            // Convert pixel coordinates to complex plane coordinates
            double u = i * local_scaling - local_x_offset;
            double v = j * local_scaling - local_y_offset;

            // Calculate iterations for this point
            int n = mandelbrot_iterations(u, v);

            // Calculate color based on iteration count
            if (n == MAXI) {
                // Point is in the set - color it black
                set_pixel_fb(fbp, vinfo, finfo, i, j, 0, 0, 0);
            } else {
                // Point escaped - color based on iteration count
                float hue = fmod((n * 360.0 * COLOUR_SCALE) / MAXI, 360.0);
                uint8_t r, g, b;
                hsb_to_rgb(hue, 1.0, 1.0, &r, &g, &b);
                set_pixel_fb(fbp, vinfo, finfo, i, j, r, g, b);
            }
        }
    }

    printf("Render complete.\n");
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
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

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
    
    printf("Framebuffer device: %s\n", fb_device);
    printf("  Display: %s\n", finfo.id);
    printf("  Resolution: %dx%d\n", vinfo.xres, vinfo.yres);
    printf("  Bits per pixel: %d\n", vinfo.bits_per_pixel);
    printf("  Line length: %d bytes\n", finfo.line_length);
    
    // Check if our target resolution fits
    if (WIDTH > vinfo.xres || HEIGHT > vinfo.yres) {
        printf("Warning: Target resolution %dx%d is larger than framebuffer %dx%d\n", 
               WIDTH, HEIGHT, vinfo.xres, vinfo.yres);
        printf("The image will be clipped.\n");
    }
    
    // Calculate screen size in bytes
    screensize = vinfo.yres * finfo.line_length;
    
    // Map framebuffer to memory
    fbp = (char*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fbp == MAP_FAILED) {
        perror("Error mapping framebuffer device to memory");
        close(fb_fd);
        return 1;
    }
    
    printf("\nGenerating Mandelbrot set (%dx%d)...\n", WIDTH, HEIGHT);
    printf("Press Ctrl+C to exit. Touch screen to zoom in by 10%%.\n");

    // Start touch handler thread
    pthread_t touch_thread;
    if (pthread_create(&touch_thread, NULL, touch_handler, NULL) != 0) {
        fprintf(stderr, "Warning: Failed to create touch handler thread\n");
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

    // Wait for touch thread to finish
    pthread_join(touch_thread, NULL);
    
    // Cleanup
    cleanup();
    printf("Cleanup complete.\n");
    
    return 0;
}
