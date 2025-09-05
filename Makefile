# Compiler for native build
CC = gcc

# --- Native Build Configuration ---
# Use sdl2-config to get compiler and linker flags for native build
NATIVE_CFLAGS = -Wall -Wextra -g `sdl2-config --cflags`
NATIVE_LDFLAGS = `sdl2-config --libs` -lSDL2_ttf -lm

# --- Project Structure ---
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
WEB_DIR = web

# Automatically find all .c files in the src directory
SOURCES = $(wildcard $(SRC_DIR)/*.c)

# Create a list of corresponding object files for the native build
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))

# Define the final executable name and path for the native build
EXECUTABLE = sine_wave_modulator
TARGET = $(BIN_DIR)/$(EXECUTABLE)


# --- Build Rules ---

# Default rule: build the native version
all: native

# Rule to build the native executable
native: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CC) -o $@ $^ $(NATIVE_LDFLAGS)
	@echo "Native build complete: '$@'"
	@cp -r assets $(BIN_DIR)/

# Pattern rule to compile .c files into .o files for the native build
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(NATIVE_CFLAGS) -c -o $@ $<

# Rule to build the web version using Emscripten in Docker
web:
	@echo "Building for web..."
	@mkdir -p $(WEB_DIR)
	docker run --rm -v $(CURDIR):/src trzeci/emscripten:sdk-incoming-64bit emcc \
		-Wall \
		$(SOURCES) \
		-D__EMSCRIPTEN__ \
		--preload-file assets \
		--js-library src/library_save_file.js \
		-s USE_SDL=2 \
		-s USE_SDL_TTF=2 \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s ASYNCIFY \
		--shell-file src/template.html \
		-o $(WEB_DIR)/index.html
	@echo "Web build complete: '$(WEB_DIR)/index.html'"


# Rule to clean up all generated files
clean:
	@echo "Cleaning up..."
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(WEB_DIR)

# Phony targets are not actual files
.PHONY: all native web clean