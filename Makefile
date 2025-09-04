# Compiler and flags
CC = gcc
EMCC = emcc
CFLAGS = -Wall -Wextra -std=c99 -I/usr/local/include
LDFLAGS = -L/usr/local/lib -lraylib -lm

# Emscripten specific flags
EMCC_FLAGS = -s USE_RAYLIB=1 -s ASYNCIFY -s ALLOW_MEMORY_GROWTH --shell-file src/template.html
# Export the C functions we need to call from JavaScript
EMCC_FLAGS += -s EXPORTED_FUNCTIONS="['_main', '_open_file_data', '_get_waveform_data_for_saving']"
EMCC_LDFLAGS =

# Source files for native build
SRC_NATIVE = src/main.c src/text_renderer.c src/tinyfiledialogs.c
OBJ = $(SRC_NATIVE:.c=.o)

# Source files for web build (exclude tinyfiledialogs.c)
SRC_WEB = src/main.c src/text_renderer.c

# Target executable
TARGET_NATIVE = sigviz_native
TARGET_WEB = index.html

# Default target
all: native

# Native build
native: $(OBJ)
	$(CC) $(OBJ) -o $(TARGET_NATIVE) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Web build
web:
	$(EMCC) $(SRC_WEB) -o $(TARGET_WEB) $(EMCC_FLAGS) $(EMCC_LDFLAGS)

# Clean up
clean:
	rm -f $(OBJ) $(TARGET_NATIVE) index.js index.wasm index.html

.PHONY: all native web clean