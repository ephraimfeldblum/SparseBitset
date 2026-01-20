# Makefile for Redis Bitset Module
# This builds a Redis module that provides sparse bitset operations using VEB trees

# Module name and version
MODULE_NAME = bitset
MODULE_VERSION = 1

# Build configuration
BUILD_TYPE ?= Release

# Directories
SRC_DIR = src
VEB_DIR = $(SRC_DIR)/VEB
VEB_BUILD_DIR = $(VEB_DIR)/build
BUILD_DIR = build

# Compiler and flags
CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -O3 -fPIC -std=c11 -mavx2
CXXFLAGS = -Wall -Wextra -O3 -fPIC -std=c++23 -mavx2
LDFLAGS = -shared

# Include directories
INCLUDES = -I$(VEB_DIR) -I$(SRC_DIR)

# Libraries
LIBS = -L$(VEB_BUILD_DIR) -Wl,-rpath,$(shell pwd)/$(VEB_BUILD_DIR) -lvebtree

# Source files
MODULE_SOURCES = $(SRC_DIR)/bitset_module.c
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
	cd $(VEB_BUILD_DIR) && \
		cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_C_COMPILER=$(CC) \
		-DCMAKE_CXX_COMPILER=$(CXX) \
		-DCMAKE_C_FLAGS="$(CFLAGS)" \
		-DCMAKE_CXX_FLAGS="$(CXXFLAGS)" \
		-DCMAKE_CXX_STANDARD=23 ..

# Build VEB library if needed
$(VEB_BUILD_DIR)/libvebtree.so: $(VEB_BUILD_DIR)
	@echo "Building VEB library with $(BUILD_TYPE) configuration..."
	cd $(VEB_BUILD_DIR) && $(MAKE) -j$(shell nproc) vebtree

# Compile module source files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Link the module
$(MODULE_SO): $(MODULE_OBJECTS) $(VEB_BUILD_DIR)/libvebtree.so
	$(CXX) $(LDFLAGS) -o $@ $(MODULE_OBJECTS) $(LIBS)

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

.PHONY: test

# Run flow tests locally (uses run_flow_tests.sh)
test:
	TEST=$(TEST) QUICK=$(QUICK) ./run_flow_tests.sh

.PHONY: docker-build-image docker-test

# Build the Docker image used for tests
docker-build-image:
	docker build -t sparsebitset:test .

# Run RLTest-based flow tests inside Docker and store logs inside the repo
# Usage: make docker-test
docker-test: docker-build-image
	./run_in_docker.sh
