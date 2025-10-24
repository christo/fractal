# Mandelbrot Set Generator for Raspberry Pi

This project generates Mandelbrot set images on Raspberry Pi with direct framebuffer access,
designed for small TFT displays running without X11.

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
./mandelbrot
```

## Features

- Works without X11 desktop environment
- Uses direct framebuffer access for TFT displays
- Defaults to `/dev/fb1` (TFT display), can target `/dev/fb0` (HDMI) with `-d` flag
- Supports 16-bit (RGB565), 24-bit (RGB), and 32-bit (RGBA/BGRA) pixel formats
- Multi-threaded rendering (4 worker threads) for optimal performance on multi-core Raspberry Pi
- Performance varies by Pi model and screen resolution (Pi 3b was plenty quick
for a TFT of 320x240)

## Usage

```bash
./mandelbrot                       # Run on TFT display /dev/fb1
./mandelbrot -d /dev/fb0           # Run on HDMI display
./mandelbrot -t /dev/input/event0  # specify touch device
./mandelbrot --help                # Show usage information
```

## TODO

- [x] add visual indicator of touchscreen centre
- [x] derive WIDTH and HEIGHT from framebuffer
- [x] fix origin bias of touch zoom (probably need offset bias in complex plane
for centre)
- [x] use GPIO buttons for zoom out
- [x] use GPIO buttons for reset zoom
- [x] concurrency for 4-core rpi (4 render threads + touch/button handlers)
- [ ] test button 4 for electrical continuity (what GPIO?)
- [ ] touchscreen swipe to pan

## Controls

- Press Ctrl+C to exit
- zoom controls using touchscreen

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
