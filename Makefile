CC=gcc
CFLAGS=-Wall -Wextra -O3 -std=c99
LIBS=-lm
TARGET=mandelbrot_fb
SOURCE=mandelbrot_fb.c

# Default target - build framebuffer version
all: $(TARGET)

# Direct framebuffer version
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

# Install build dependencies
install-deps:
	apt-get update
	apt-get install -y build-essential

# Clean build artifacts
clean:
	rm -f $(TARGET)

# Run framebuffer version
run: $(TARGET)
	./$(TARGET)

.PHONY: clean install-deps run all
