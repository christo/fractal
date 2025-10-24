CC=gcc
CFLAGS=-Wall -Wextra -O3 -std=c99
LIBS=-lm -lpthread -lgpiod
TARGET=mandelbrot
SOURCE=mandelbrot.c

# Remote development configuration
PI_HOST ?= fractal.local
PI_DIR ?= src/fractal

# Default target - build framebuffer version
all: $(TARGET)

# Direct framebuffer version
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

# Install build dependencies
install-deps:
	apt-get update
	apt-get install -y build-essential libgpiod-dev

# Clean build artifacts
clean:
	rm -f $(TARGET)

# Run framebuffer version
run: $(TARGET)
	./$(TARGET)

# Remote development targets
remote-sync:
	rsync -avz --exclude '$(TARGET)' --exclude '.git/' --exclude '.claude/' . $(PI_HOST):$(PI_DIR)

remote-build: remote-sync
	ssh $(PI_HOST) "cd $(PI_DIR) && make"

remote-run: remote-build
	ssh $(PI_HOST) "cd $(PI_DIR) && ./$(TARGET)"

remote-clean:
	ssh $(PI_HOST) "cd $(PI_DIR) && make clean"

.PHONY: clean install-deps run all remote-sync remote-build remote-run remote-clean
