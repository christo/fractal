// direct framebuffer rendering version

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MAXI 360
#define COLOUR_SCALE 18
#define WIDTH 320
#define HEIGHT 240

// Mandelbrot parameters
const double SCALING = 0.013;
const double X_OFFSET = 2.6;
const double Y_OFFSET = 1.6;

// Global variables for cleanup
int fb_fd = -1;
char* fbp = NULL;
long screensize = 0;
volatile sig_atomic_t quit_flag = 0;

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

// Cleanup function
void cleanup() {
    if (fbp && fbp != MAP_FAILED) {
        munmap(fbp, screensize);
    }
    if (fb_fd >= 0) {
        close(fb_fd);
    }
}

int main(int argc, char* argv[]) {
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    
    // Set up signal handler for Ctrl+C
    signal(SIGINT, signal_handler);
    
    // Open framebuffer device
    fb_fd = open("/dev/fb1", O_RDWR);
    if (fb_fd < 0) {
        perror("Error opening /dev/fb1");
        printf("Make sure your TFT display is properly configured.\n");
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
    
  /*
    printf("Framebuffer info:\n");
    printf("  Resolution: %dx%d\n", vinfo.xres, vinfo.yres);
    printf("  Bits per pixel: %d\n", vinfo.bits_per_pixel);
    printf("  Line length: %d bytes\n", finfo.line_length);
    */
    
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
    
    printf("Framebuffer mapped successfully. Generating Mandelbrot set...\n");
    printf("Press Ctrl+C to exit.\n");
    
    // Clear screen (fill with black)
    memset(fbp, 0, screensize);
    
    // Generate Mandelbrot set
    for (int i = 0; i < WIDTH && !quit_flag; i++) {
        for (int j = 0; j < HEIGHT && !quit_flag; j++) {
            // Convert pixel coordinates to complex plane coordinates
            double u = i * SCALING - X_OFFSET;
            double v = j * SCALING - Y_OFFSET;
            
            // Calculate iterations for this point
            int n = mandelbrot_iterations(u, v);
            
            // Calculate color based on iteration count
            if (n == MAXI) {
                // Point is in the set - color it black
                set_pixel_fb(fbp, &vinfo, &finfo, i, j, 0, 0, 0);
            } else {
                // Point escaped - color based on iteration count
                float hue = fmod((n * 360.0 * COLOUR_SCALE) / MAXI, 360.0);
                uint8_t r, g, b;
                hsb_to_rgb(hue, 1.0, 1.0, &r, &g, &b);
                set_pixel_fb(fbp, &vinfo, &finfo, i, j, r, g, b);
            }
        }
        
        // Print progress every 50 columns and check for early exit
	/*
        if (i % 50 == 0) {
            printf("Progress: %d/%d columns\n", i, WIDTH);
        }
	*/
    }
    
    if (quit_flag) {
        printf("\nGeneration interrupted by user.\n");
    } else {
        //printf("Mandelbrot set generated! Press Ctrl+C to exit.\n");
        
        // Keep the image displayed until user exits
        while (!quit_flag) {
            usleep(100000); // Sleep 100ms
        }
    }
    
    // Cleanup
    cleanup();
    printf("Cleanup complete.\n");
    
    return 0;
}
