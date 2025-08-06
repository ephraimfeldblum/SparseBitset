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

# Auto-detect optional dependencies
message(STATUS "Checking for optional dependencies...")

# Check for Abseil
find_package(PkgConfig QUIET)
set(HAVE_ABSL FALSE)
if(PkgConfig_FOUND)
    pkg_check_modules(ABSL QUIET absl_flat_hash_map)
    if(ABSL_FOUND)
        set(HAVE_ABSL TRUE)
        message(STATUS "Found Abseil via pkg-config")
    endif()
endif()

# Fallback: Try to find Abseil manually
if(NOT HAVE_ABSL)
    find_path(ABSL_INCLUDE_DIR absl/container/flat_hash_map.h
        PATHS /usr/local/include /usr/include
        NO_DEFAULT_PATH)
    
    if(ABSL_INCLUDE_DIR)
        # Try to compile a test program
        include(CheckCXXSourceCompiles)
        set(CMAKE_REQUIRED_INCLUDES ${ABSL_INCLUDE_DIR})
        set(CMAKE_REQUIRED_LIBRARIES absl_hash absl_raw_hash_set)
        check_cxx_source_compiles("
            #include <absl/container/flat_hash_map.h>
            int main() { return 0; }
        " ABSL_COMPILES)
        
        if(ABSL_COMPILES)
            set(HAVE_ABSL TRUE)
            message(STATUS "Found Abseil manually at ${ABSL_INCLUDE_DIR}")
        endif()
    endif()
endif()

# Check for Boost
find_path(BOOST_INCLUDE_DIR boost/unordered/unordered_flat_map.hpp
    PATHS /usr/local/include /usr/include
    NO_DEFAULT_PATH)

set(HAVE_BOOST FALSE)
if(BOOST_INCLUDE_DIR)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_INCLUDES ${BOOST_INCLUDE_DIR})
    check_cxx_source_compiles("
        #include <boost/unordered/unordered_flat_map.hpp>
        int main() { boost::unordered_flat_map<int,int> m; return 0; }
    " BOOST_COMPILES)
    
    if(BOOST_COMPILES)
        set(HAVE_BOOST TRUE)
        message(STATUS "Found Boost at ${BOOST_INCLUDE_DIR}")
    endif()
endif()

# Configure dependencies
# Set status variables for display
if(HAVE_ABSL)
    set(ABSL_STATUS "Available")
    add_compile_definitions(HAVE_ABSL)

    # Add Abseil include directory
    if(ABSL_INCLUDE_DIR)
        include_directories(${ABSL_INCLUDE_DIR})
    endif()

    # Use pkg-config to get all required Abseil libraries
    if(PkgConfig_FOUND)
        pkg_check_modules(ABSL_ALL QUIET absl_flat_hash_map)
        if(ABSL_ALL_FOUND)
            set(ABSL_LIBRARIES ${ABSL_ALL_LIBRARIES})
        endif()
    endif()

    # Fallback to manual library list if pkg-config fails
    if(NOT ABSL_LIBRARIES)
        set(ABSL_LIBRARIES
            absl_raw_hash_set absl_hashtablez_sampler absl_hash absl_city
            absl_base absl_raw_logging_internal absl_throw_delegate
            absl_log_severity absl_time absl_stacktrace absl_synchronization
            absl_spinlock_wait absl_malloc_internal absl_debugging_internal)
    endif()

    # Add library search path
    link_directories(/usr/local/lib)

    message(STATUS "Abseil support: ENABLED")
else()
    set(ABSL_STATUS "Not available")
    message(STATUS "Abseil support: DISABLED")
endif()

if(HAVE_BOOST)
    set(BOOST_STATUS "Available")
    add_compile_definitions(HAVE_BOOST)
    if(BOOST_INCLUDE_DIR)
        include_directories(${BOOST_INCLUDE_DIR})
    endif()
    message(STATUS "Boost support: ENABLED")
else()
    set(BOOST_STATUS "Not available")
    message(STATUS "Boost support: DISABLED")
endif()

# Source files
set(CORE_SOURCES
    VebTree_c_api.cpp
)



# Create shared library
add_library(vebtree SHARED ${CORE_SOURCES})

# Link dependencies to library
if(HAVE_ABSL)
    if(ABSL_LIBRARIES)
        # Use whole-archive for raw_hash_set to ensure all symbols are included
        target_link_libraries(vebtree PRIVATE
            -Wl,--whole-archive absl_raw_hash_set -Wl,--no-whole-archive
            ${ABSL_LIBRARIES})
    endif()
endif()



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
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Auto-detected Dependencies:"
    COMMAND ${CMAKE_COMMAND} -E echo "  Abseil: ${ABSL_STATUS}"
    COMMAND ${CMAKE_COMMAND} -E echo "  Boost: ${BOOST_STATUS}"
    COMMENT "Displaying build information"
)

# Check dependencies target
add_custom_target(check-deps
    COMMAND ${CMAKE_COMMAND} -E echo "Checking for optional dependencies..."
    COMMAND ${CMAKE_COMMAND} -E echo "Abseil: ${ABSL_STATUS}"
    COMMAND ${CMAKE_COMMAND} -E echo "Boost: ${BOOST_STATUS}"
    COMMENT "Checking dependencies"
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
    COMMAND ${CMAKE_COMMAND} -E echo "  info                   - Show build configuration and detected dependencies"
    COMMAND ${CMAKE_COMMAND} -E echo "  check-deps             - Check for optional dependencies"
    COMMAND ${CMAKE_COMMAND} -E echo "  show-help              - Show this help message"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Usage:"
    COMMAND ${CMAKE_COMMAND} -E echo "  mkdir build && cd build"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake .."
    COMMAND ${CMAKE_COMMAND} -E echo "  make [target]"
    COMMENT "Displaying help information"
)

# Set default target
set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    "build.log" "test.log"
)
