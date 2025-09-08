# Compiler for native build
CC = gcc

# --- Native Build Configuration ---
NATIVE_CFLAGS = -Wall -Wextra -g `sdl2-config --cflags`
NATIVE_LDFLAGS = `sdl2-config --libs` -lSDL2_ttf -lm

# --- Project Structure ---
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
WEB_DIR = web

# Automatically find all .c files in the src directory
SOURCES = $(wildcard $(SRC_DIR)/*.c)
# Create a list of corresponding object files
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))
# Create a list of source files with the correct paths for the Docker container
WEB_SOURCES = $(addprefix /src/, $(SOURCES))
# Define the final executable name and path
EXECUTABLE = SigViz
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

# Explicit dependencies to ensure proper recompilation when headers change
$(OBJ_DIR)/main.o: $(SRC_DIR)/shared.h $(SRC_DIR)/time_domain.h $(SRC_DIR)/iq_plot.h $(SRC_DIR)/fft.h
$(OBJ_DIR)/time_domain.o: $(SRC_DIR)/shared.h
$(OBJ_DIR)/iq_plot.o: $(SRC_DIR)/shared.h
$(OBJ_DIR)/fft.o: $(SRC_DIR)/shared.h
$(OBJ_DIR)/text_renderer.o: $(SRC_DIR)/text_renderer.h

# Rule to build the web version using Emscripten
web:
    @echo "Building for web..."
    @mkdir -p $(WEB_DIR)
    docker run --rm -v $(CURDIR):/src trzeci/emscripten:sdk-incoming-64bit \
        sh -c "emcc \
            -Wall \
            $(WEB_SOURCES) \
            -D__EMSCRIPTEN__ \
            --preload-file assets \
            --js-library src/library_save_file.js \
            -s USE_SDL=2 \
            -s USE_SDL_TTF=2 \
            -s ALLOW_MEMORY_GROWTH=1 \
            -s ASYNCIFY \
            --shell-file src/template.html \
            -o $(WEB_DIR)/index.html"
    @echo "Web build complete: '$(WEB_DIR)/index.html'"

# Rule to clean up all generated files
clean:
    @echo "Cleaning up..."
    rm -rf $(OBJ_DIR) $(BIN_DIR) $(WEB_DIR)

# Phony targets are not actual files
.PHONY: all native web clean