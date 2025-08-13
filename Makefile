# Makefile for Redis Bitset Module
# This builds a Redis module that provides sparse bitset operations using VEB trees

# Module name and version
MODULE_NAME = bitset
MODULE_VERSION = 1

# Build configuration
BUILD_TYPE ?= Release

# Directories
VEB_DIR = VEB
VEB_BUILD_DIR = $(VEB_DIR)/build
SRC_DIR = .
BUILD_DIR = build

# Compiler and flags
CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -O2 -fPIC -std=c11
CXXFLAGS = -Wall -Wextra -O2 -fPIC -std=c++23
LDFLAGS = -shared

# Include directories
INCLUDES = -I$(VEB_DIR) -I.

# Libraries
LIBS = -L$(VEB_BUILD_DIR) -Wl,-rpath,$(shell pwd)/$(VEB_BUILD_DIR) -lvebtree -lstdc++

# Source files
MODULE_SOURCES = bitset_module.c
MODULE_OBJECTS = $(MODULE_SOURCES:.c=.o)

# Target shared library
MODULE_SO = $(MODULE_NAME).so

# Default target
all: $(MODULE_SO)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Ensure VEB build directory exists and is configured
$(VEB_BUILD_DIR):
	@echo "Setting up VEB build directory..."
	mkdir -p $(VEB_BUILD_DIR)
	cd $(VEB_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) ..

# Build VEB library if needed
$(VEB_BUILD_DIR)/libvebtree.so: $(VEB_BUILD_DIR)
	@echo "Building VEB library with $(BUILD_TYPE) configuration..."
	cd $(VEB_BUILD_DIR) && $(MAKE) vebtree

# Compile module source files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Link the module
$(MODULE_SO): $(MODULE_OBJECTS) $(VEB_BUILD_DIR)/libvebtree.so
	$(CC) $(LDFLAGS) -o $@ $(MODULE_OBJECTS) $(LIBS)

# Clean build artifacts
clean:
	rm -f $(MODULE_OBJECTS) $(MODULE_SO)
	rm -rf $(BUILD_DIR)
	@if [ -d "$(VEB_DIR)" ] && [ -f "$(VEB_DIR)/Makefile" ]; then \
		cd $(VEB_DIR) && $(MAKE) clean; \
	fi

# Clean everything including VEB library
distclean: clean
	@if [ -d "$(VEB_DIR)" ]; then \
		cd $(VEB_DIR) && $(MAKE) clean; \
	fi

# Install the module (copy to a standard location)
install: $(MODULE_SO)
	@echo "Installing $(MODULE_SO)..."
	@echo "Copy $(MODULE_SO) to your Redis modules directory"
	@echo "Then load it in Redis with: MODULE LOAD /path/to/$(MODULE_SO)"



# Build targets for different configurations
debug:
	@$(MAKE) all BUILD_TYPE=Debug

release:
	@$(MAKE) all BUILD_TYPE=Release

# Show help
help:
	@echo "Available targets:"
	@echo "  all       - Build the Redis module (default: Release)"
	@echo "  debug     - Build with Debug configuration"
	@echo "  release   - Build with Release configuration"
	@echo "  clean     - Clean module build artifacts"
	@echo "  distclean - Clean everything including VEB library"
	@echo "  install   - Show installation instructions"
	@echo "  help      - Show this help"
	@echo ""
	@echo "Build configuration:"
	@echo "  BUILD_TYPE=$(BUILD_TYPE) (can be Debug or Release)"

.PHONY: all debug release clean distclean install help
