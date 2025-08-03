# Makefile for Redis Bitset Module
# This builds a Redis module that provides sparse bitset operations using VEB trees

# Module name and version
MODULE_NAME = bitset
MODULE_VERSION = 1

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

# Check for Abseil libraries
ABSL_LIBS := $(shell pkg-config --libs absl_flat_hash_map 2>/dev/null)
ifneq ($(ABSL_LIBS),)
    LIBS += $(ABSL_LIBS)
endif

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

# Build VEB library if needed
$(VEB_BUILD_DIR)/libvebtree.so:
	@echo "Building VEB library..."
	cd $(VEB_DIR) && $(MAKE) -C build

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

# Clean everything including VEB library
distclean: clean
	cd $(VEB_DIR) && $(MAKE) -C build clean

# Install the module (copy to a standard location)
install: $(MODULE_SO)
	@echo "Installing $(MODULE_SO)..."
	@echo "Copy $(MODULE_SO) to your Redis modules directory"
	@echo "Then load it in Redis with: MODULE LOAD /path/to/$(MODULE_SO)"

# Test the module (requires Redis to be running)
test: $(MODULE_SO)
	@echo "Running automated tests..."
	./test_module.sh

# Run example demonstration
example: $(MODULE_SO)
	@echo "Running example demonstration..."
	./example.sh

# Quick test (just show commands)
test-info: $(MODULE_SO)
	@echo "Testing Redis module..."
	@echo "Make sure Redis is running, then execute:"
	@echo "redis-cli MODULE LOAD ./$(MODULE_SO)"
	@echo "redis-cli BITS.CREATE testkey"
	@echo "redis-cli BITS.SET testkey 1 5 10"
	@echo "redis-cli BITS.LIST testkey"

# Show help
help:
	@echo "Available targets:"
	@echo "  all       - Build the Redis module (default)"
	@echo "  clean     - Clean module build artifacts"
	@echo "  distclean - Clean everything including VEB library"
	@echo "  install   - Show installation instructions"
	@echo "  test      - Run automated test suite"
	@echo "  example   - Run example demonstration"
	@echo "  test-info - Show manual testing instructions"
	@echo "  help      - Show this help"

.PHONY: all clean distclean install test help
