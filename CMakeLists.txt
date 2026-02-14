cmake_minimum_required(VERSION 3.16)

project(vEBitset VERSION 1.0.0 LANGUAGES C CXX)

# Set C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wconversion -Wsign-compare -Wpointer-arith -march=native -fPIC")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -Wconversion -Wsign-compare -Wpointer-arith -march=native -fPIC")

add_compile_definitions(_POSIX_C_SOURCE=200809L)

# Build type specific flags
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -DDEBUG -Og")
set(CMAKE_C_FLAGS_DEBUG "-g3 -DDEBUG -Og")

set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -flto -O3")
set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG -flto -O3")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto")

# Public include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add VEB subdirectory
add_subdirectory(src/VEB)

# Core library target (C API)
add_library(vebitset src/vebitset.cpp)
target_link_libraries(vebitset PUBLIC vebtree)
target_include_directories(vebitset 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Fetch dependencies for benchmarks
include(FetchContent)
FetchContent_Declare(roaring
    GIT_REPOSITORY https://github.com/RoaringBitmap/CRoaring.git
    GIT_TAG v4.5.1
)
FetchContent_Declare(nanobench
    GIT_REPOSITORY https://github.com/martinus/nanobench.git
    GIT_TAG v4.3.11
)

# Build tests and benchmarks
option(VEBITSET_BUILD_TESTS "Build C++ tests" ON)
option(VEBITSET_BUILD_BENCHMARKS "Build benchmarks" ON)

# Prevent building cmocka example binaries (these examples sometimes
# cause linker issues on certain toolchains). This is a build-time
# configuration only and does NOT modify dependency source.
set(WITH_EXAMPLES OFF CACHE BOOL "Disable cmocka examples" FORCE)

if(VEBITSET_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(VEBITSET_BUILD_BENCHMARKS)
    FetchContent_MakeAvailable(roaring nanobench)
    add_subdirectory(tests/benchmarks)
endif()

# Export targets for downstream consumption
export(TARGETS vebitset vebtree FILE ${CMAKE_CURRENT_BINARY_DIR}/vEBitsetTargets.cmake)
