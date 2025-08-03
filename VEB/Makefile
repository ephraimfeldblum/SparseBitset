# VEB Tree Makefile
# This Makefile provides a simple interface to the CMake build system

# Configuration
BUILD_DIR := build
BUILD_TYPE ?= Debug
NPROC := $(shell nproc 2>/dev/null || echo 4)

# Colors for output
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[1;33m
BLUE := \033[0;34m
NC := \033[0m

# Default target
.PHONY: all
all: build

# Help target
.PHONY: help
help:
	@echo "VEB Tree Makefile"
	@echo "================="
	@echo ""
	@echo "Usage: make [target] [BUILD_TYPE=type]"
	@echo ""
	@echo "Targets:"
	@echo "  build [BUILD_TYPE=type] - Build the project (default: Debug)"
	@echo "  test                    - Build and run tests"
	@echo "  test-valgrind          - Build and run tests under valgrind"
	@echo "  clean                  - Clean build directory"
	@echo "  info                   - Show build information"
	@echo "  check-deps             - Check dependencies"
	@echo "  help                   - Show this help"
	@echo ""
	@echo "Build types:"
	@echo "  Debug                  - Debug build (default)"
	@echo "  Release                - Optimized release build"
	@echo "  Valgrind               - Debug build optimized for valgrind"
	@echo ""
	@echo "Examples:"
	@echo "  make build             # Debug build"
	@echo "  make build BUILD_TYPE=Release  # Release build"
	@echo "  make test              # Build and run tests"
	@echo "  make clean             # Clean build directory"

# Ensure build directory exists
.PHONY: ensure-build-dir
ensure-build-dir:
	@if [ ! -d "$(BUILD_DIR)" ]; then \
		printf "$(BLUE)Creating build directory...$(NC)\n"; \
		mkdir -p $(BUILD_DIR); \
	fi

# Configure CMake
.PHONY: configure
configure: ensure-build-dir
	@printf "$(BLUE)Configuring CMake with build type: $(BUILD_TYPE)$(NC)\n"
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) ..

# Build project
.PHONY: build
build: configure
	@printf "$(BLUE)Building project...$(NC)\n"
	@cd $(BUILD_DIR) && make --no-print-directory -j$(NPROC)
	@printf "$(GREEN)Build completed successfully!$(NC)\n"

# Build with specific types
.PHONY: debug release valgrind
debug:
	@$(MAKE) build BUILD_TYPE=Debug

release:
	@$(MAKE) build BUILD_TYPE=Release

valgrind:
	@$(MAKE) build BUILD_TYPE=Valgrind

# Test targets
.PHONY: test
test: ensure-build-dir
	@if [ ! -f "$(BUILD_DIR)/Makefile" ]; then \
		printf "$(BLUE)Configuring CMake for tests...$(NC)\n"; \
		cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug ..; \
	fi
	@printf "$(BLUE)Building and running tests...$(NC)\n"
	@cd $(BUILD_DIR) && make --no-print-directory test
	@printf "$(GREEN)Tests completed!$(NC)\n"

.PHONY: test-valgrind
test-valgrind: ensure-build-dir
	@if [ ! -f "$(BUILD_DIR)/Makefile" ]; then \
		printf "$(BLUE)Configuring CMake for valgrind tests...$(NC)\n"; \
		cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Valgrind ..; \
	fi
	@printf "$(BLUE)Building and running tests under valgrind...$(NC)\n"
	@cd $(BUILD_DIR) && make --no-print-directory test-valgrind
	@printf "$(GREEN)Valgrind tests completed!$(NC)\n"

# Clean target
.PHONY: clean
clean:
	@if [ -d "$(BUILD_DIR)" ]; then \
		printf "$(YELLOW)Cleaning build directory...$(NC)\n"; \
		rm -rf $(BUILD_DIR); \
		printf "$(GREEN)Build directory cleaned!$(NC)\n"; \
	else \
		printf "$(YELLOW)Build directory doesn't exist, nothing to clean.$(NC)\n"; \
	fi

# Info target
.PHONY: info
info: ensure-build-dir
	@if [ ! -f "$(BUILD_DIR)/Makefile" ]; then \
		printf "$(BLUE)Configuring CMake for info...$(NC)\n"; \
		cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug ..; \
	fi
	@cd $(BUILD_DIR) && make --no-print-directory info

# Check dependencies target
.PHONY: check-deps
check-deps: ensure-build-dir
	@if [ ! -f "$(BUILD_DIR)/Makefile" ]; then \
		printf "$(BLUE)Configuring CMake for dependency check...$(NC)\n"; \
		cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug ..; \
	fi
	@cd $(BUILD_DIR) && make --no-print-directory check-deps
