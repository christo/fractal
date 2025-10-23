# Mandelbrot Set Generator for Raspberry Pi

This project generates Mandelbrot set images on Raspberry Pi with direct framebuffer access,
designed for small TFT displays running without X11.

There is also a sketch written in JavaScript for p5js.

## Setup Instructions

### 1. Install Dependencies

``` bash
sudo make install-deps
```

Or manually:

``` bash
sudo apt-get update
sudo apt-get install build-essential
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
./mandelbrot_fb
```

## Raspberry Pi Specific Notes

- Works without X11 desktop environment
- Uses direct framebuffer access for TFT displays
- Defaults to `/dev/fb1` (TFT display), can target `/dev/fb0` (HDMI) with `-d` flag
- Supports 16-bit (RGB565), 24-bit (RGB), and 32-bit (RGBA/BGRA) pixel formats
- Performance varies by Pi model (Pi 3b was plenty quick for a TFT of 320x240)

## Usage

```bash
./mandelbrot_fb              # Run on TFT display (/dev/fb1)
./mandelbrot_fb -d /dev/fb0  # Run on HDMI display
./mandelbrot_fb --help       # Show usage information
```

## Controls

- Press Ctrl+C to exit
- TODO: zoom and pan controls using touchscreen
- TODO: use GPIO buttons

## Configuration

You can modify these constants in the source code:

- `SCALING`: Zoom level (smaller = more zoomed in)
- `X_OFFSET`, `Y_OFFSET`: Center point of the view
- `WIDTH`, `HEIGHT`: Output resolution
- `MAXI`: Maximum iterations (higher = more detail, slower) numbers over 100
probably make no visible difference except on very high density displays
- `COLOUR_SCALE`: Color cycling speed

## Troubleshooting

For permission issues with framebuffer consider the following.

``` bash
sudo usermod -a -G video $USER
```
(Then log out and back in)
