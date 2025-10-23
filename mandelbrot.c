// SDL version

#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#define MAXI 100
#define COLOUR_SCALE 11
#define WIDTH 320
#define HEIGHT 240

// Mandelbrot parameters
const double SCALING = 0.011;
const double X_OFFSET = 2.6;
const double Y_OFFSET = 1.6;

// HSB to RGB conversion function
void hsb_to_rgb(float h, float s, float b, Uint8* r, Uint8* g, Uint8* bl) {
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
    
    *r = (Uint8)((r_prime + m) * 255);
    *g = (Uint8)((g_prime + m) * 255);
    *bl = (Uint8)((b_prime + m) * 255);
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

// Set pixel color in the surface
void set_pixel(SDL_Surface* surface, int x, int y, Uint32 color) {
    if (x >= 0 && x < surface->w && y >= 0 && y < surface->h) {
        Uint32* pixels = (Uint32*)surface->pixels;
        pixels[y * surface->w + x] = color;
    }
}

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    
    // Create window (this works even without X11 on Pi with proper drivers)
    SDL_Window* window = SDL_CreateWindow("Mandelbrot Set",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    
    if (window == NULL) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    // Get window surface
    SDL_Surface* screen_surface = SDL_GetWindowSurface(window);
    
    // Create a surface for our Mandelbrot rendering
    SDL_Surface* mandelbrot_surface = SDL_CreateRGBSurface(0, WIDTH, HEIGHT, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    
    if (mandelbrot_surface == NULL) {
        fprintf(stderr, "Surface could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    printf("Generating Mandelbrot set...\n");
    
    // Lock surface for direct pixel access
    if (SDL_LockSurface(mandelbrot_surface) < 0) {
        fprintf(stderr, "Could not lock surface! SDL_Error: %s\n", SDL_GetError());
        SDL_FreeSurface(mandelbrot_surface);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Generate Mandelbrot set
    for (int i = 0; i < WIDTH; i++) {
        for (int j = 0; j < HEIGHT; j++) {
            // Convert pixel coordinates to complex plane coordinates
            double u = i * SCALING - X_OFFSET;
            double v = j * SCALING - Y_OFFSET;
            
            // Calculate iterations for this point
            int n = mandelbrot_iterations(u, v);
            
            // Calculate color based on iteration count
            if (n == MAXI) {
                // Point is in the set - color it black
                Uint32 color = SDL_MapRGB(mandelbrot_surface->format, 0, 0, 0);
                set_pixel(mandelbrot_surface, i, j, color);
            } else {
                // Point escaped - color based on iteration count
                float hue = fmod((n * 360.0 * COLOUR_SCALE) / MAXI, 360.0);
                Uint8 r, g, b;
                hsb_to_rgb(hue, 1.0, 1.0, &r, &g, &b);
                Uint32 color = SDL_MapRGB(mandelbrot_surface->format, r, g, b);
                set_pixel(mandelbrot_surface, i, j, color);
            }
        }
        
        // Print progress every 100 columns
        if (i % 100 == 0) {
            printf("Progress: %d/%d columns\n", i, WIDTH);
        }
    }
    
    // Unlock surface
    SDL_UnlockSurface(mandelbrot_surface);
    
    printf("Done! Ctrl+C to exit (or any key on local keyboard).\n");
    
    // Main event loop
    bool quit = false;
    SDL_Event e;
    
    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT || e.type == SDL_KEYDOWN) {
                quit = true;
            }
        }
        
        // Blit the Mandelbrot surface to the screen
        SDL_BlitSurface(mandelbrot_surface, NULL, screen_surface, NULL);
        SDL_UpdateWindowSurface(window);
        
        // Small delay to prevent excessive CPU usage
        SDL_Delay(16);
    }
    
    // Cleanup
    SDL_FreeSurface(mandelbrot_surface);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
