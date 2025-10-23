CC=gcc
CFLAGS=-Wall -Wextra -O3 -std=c99
SDL_LIBS=-lSDL2 -lm
FB_LIBS=-lm
TARGET_SDL=mandelbrot
TARGET_FB=mandelbrot_fb
SOURCE_SDL=mandelbrot.c
SOURCE_FB=mandelbrot_fb.c

# Default target - build both versions
all: $(TARGET_SDL) $(TARGET_FB)

# SDL version
$(TARGET_SDL): $(SOURCE_SDL)
	$(CC) $(CFLAGS) -o $(TARGET_SDL) $(SOURCE_SDL) $(SDL_LIBS)

# Direct framebuffer version
$(TARGET_FB): $(SOURCE_FB)
	$(CC) $(CFLAGS) -o $(TARGET_FB) $(SOURCE_FB) $(FB_LIBS)

# Install SDL2 development libraries (run with sudo make install-deps)
install-deps:
	apt-get update
	apt-get install -y libsdl2-dev build-essential

# Clean build artifacts
clean:
	rm -f $(TARGET_SDL) $(TARGET_FB)

# Run SDL version
run-sdl: $(TARGET_SDL)
	./$(TARGET_SDL)

# Run framebuffer version
run-fb: $(TARGET_FB)
	./$(TARGET_FB)

# Default run target uses framebuffer version
run: $(TARGET_FB)
	./$(TARGET_FB)

.PHONY: clean install-deps run run-sdl run-fb all
