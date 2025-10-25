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
- [x] test button 4 for electrical continuity (GPIO 18, pin 12)
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

## Autostart on Boot

To run the Mandelbrot generator automatically when the Raspberry Pi boots:

### Install the systemd service

**Option 1: Using Makefile (easiest)**

```bash
# From development machine
make remote-install-service

# Or directly on the Pi
sudo make install-service
sudo systemctl start mandelbrot
```

**Option 2: Manual installation**

```bash
sudo cp mandelbrot.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable mandelbrot
sudo systemctl start mandelbrot
```

### Managing the service

```bash
# Check status
sudo systemctl status mandelbrot

# View logs in real-time
journalctl -u mandelbrot -f

# Stop the service
sudo systemctl stop mandelbrot

# Disable autostart
sudo systemctl disable mandelbrot

# Restart the service
sudo systemctl restart mandelbrot
```

### Uninstall the service

```bash
# From development machine
make remote-uninstall-service

# Or directly on the Pi
sudo make uninstall-service
```

**Note:** The service uses `launch.sh` which ensures only one instance runs at a time. Manual launches while the service is running will safely exit without starting a duplicate process.
