# Mandelbrot Set Generator for Raspberry Pi

This project includes code for generating Mandelbrot set images, primarily
C programs for Raspberry Pi, running without X11, intended for a small TFT
display. One uses SDL2 and one directly writes to the TFT framebuffer.

There is also a sketch written in JavaScript for p5js.

## Setup Instructions

### 1. Install Dependencies

``` bash
sudo make install-deps
```
Or manually:
``` bash
sudo apt-get update
sudo apt-get install libsdl2-dev build-essential
```

### 2. Compile

``` bash
make
```

### 3. Run

``` bash
make run
```
Or directly:
``` bash
./mandelbrot
```

## Raspberry Pi Specific Notes

- Works with or without X11 desktop environment
- Uses SDL2's framebuffer support when X11 is unavailable
- May require setting `SDL_VIDEODRIVER=fbcon` environment variable on some Pi configurations
- Performance varies by Pi model (Pi 4 recommended for smooth rendering)

## Controls

- Press any key or close window to exit
- Progress is printed to console during generation

## Configuration

You can modify these constants in the source code:

- `SCALING`: Zoom level (smaller = more zoomed in)
- `X_OFFSET`, `Y_OFFSET`: Center point of the view
- `WIDTH`, `HEIGHT`: Output resolution
- `MAXI`: Maximum iterations (higher = more detail, slower)
- `COLOUR_SCALE`: Color cycling speed

## Troubleshooting

If you get "No available video device" error:

``` bash
export SDL_VIDEODRIVER=fbcon
./mandelbrot
```

For permission issues with framebuffer consider the following.

``` bash
sudo usermod -a -G video $USER
```
(Then log out and back in)
