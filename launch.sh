#!/bin/bash
# Launch script for Mandelbrot fractal generator
# Ensures only one instance runs at a time

# Check if mandelbrot is already running
if pgrep -x mandelbrot > /dev/null; then
    echo "Mandelbrot is already running, exiting..."
    exit 0
fi

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Launch mandelbrot from script directory
cd "$SCRIPT_DIR"
echo "Starting mandelbrot..."
./mandelbrot "$@"
