cmake_minimum_required(VERSION 3.16)

# Force GCC for C++23 support (must be set before project())
set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_C_COMPILER "gcc")

project(VEBTree VERSION 1.0.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build" FORCE)
endif()

# Set build types
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "Valgrind")

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wconversion -fPIC")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -Wconversion -fPIC")

# Build type specific flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -DDEBUG -O0")
set(CMAKE_C_FLAGS_DEBUG "-g -DDEBUG -O0")

set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -flto -O3")
set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG -flto -O3")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto")

# Custom Valgrind build type
set(CMAKE_CXX_FLAGS_VALGRIND "-g -DDEBUG -O0")
set(CMAKE_C_FLAGS_VALGRIND "-g -DDEBUG -O0")

# Include directories
include_directories(.)

# Source files
set(CORE_SOURCES
    VebTree_c_api.cpp
)


# Create shared library
add_library(vebtree SHARED ${CORE_SOURCES})


# Custom targets for different build types
add_custom_target(valgrind
    COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Valgrind ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
    COMMENT "Building with Valgrind configuration"
)

add_custom_target(release
    COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Release ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
    COMMENT "Building with Release configuration"
)



# Info target
add_custom_target(info
    COMMAND ${CMAKE_COMMAND} -E echo "Build Information:"
    COMMAND ${CMAKE_COMMAND} -E echo "  CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}"
    COMMAND ${CMAKE_COMMAND} -E echo "  CMAKE_C_COMPILER: ${CMAKE_C_COMPILER}"
    COMMAND ${CMAKE_COMMAND} -E echo "  CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}"
    COMMAND ${CMAKE_COMMAND} -E echo "  CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}"
    COMMAND ${CMAKE_COMMAND} -E echo "  CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}"
    COMMENT "Displaying build information"
)

# Help target (using show-help to avoid reserved name)
add_custom_target(show-help
    COMMAND ${CMAKE_COMMAND} -E echo "VEB Tree Project Build System (CMake)"
    COMMAND ${CMAKE_COMMAND} -E echo "====================================="
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Main targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  all                    - Build core VEB tree implementation"
    COMMAND ${CMAKE_COMMAND} -E echo "  vebtree                - Build static library"
    COMMAND ${CMAKE_COMMAND} -E echo "  clean                  - Remove all build artifacts"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Build configurations:"
    COMMAND ${CMAKE_COMMAND} -E echo "  valgrind               - Build with debug symbols for valgrind analysis"
    COMMAND ${CMAKE_COMMAND} -E echo "  release                - Build with maximum optimization"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Utility targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  info                   - Show build configuration"
    COMMAND ${CMAKE_COMMAND} -E echo "  show-help              - Show this help message"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Usage:"
    COMMAND ${CMAKE_COMMAND} -E echo "  mkdir build && cd build"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake .."
    COMMAND ${CMAKE_COMMAND} -E echo "  make [target]"
    COMMENT "Displaying help information"
)
