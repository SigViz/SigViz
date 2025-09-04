# Compiler
CC = gcc

# Use sdl2-config to get compiler flags (e.g., include paths)
CFLAGS = -Wall -Wextra -g `sdl2-config --cflags`

# Use sdl2-config to get linker flags and add the TTF library
LDFLAGS = `sdl2-config --libs` -lSDL2_ttf -lm

# --- Project Structure ---
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Automatically find all .c files in the src directory
SOURCES = $(wildcard $(SRC_DIR)/*.c)

# Create a list of corresponding object files in the obj directory
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))

# Define the final executable name and path
EXECUTABLE = sine_wave_modulator
TARGET = $(BIN_DIR)/$(EXECUTABLE)

# --- Rules ---
all: $(TARGET)

# Rule to link the final executable
$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: '$@'"

# Pattern rule to compile any .c file from src into a .o file in obj
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Rule to clean up all generated files
clean:
	@echo "Cleaning up..."
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean