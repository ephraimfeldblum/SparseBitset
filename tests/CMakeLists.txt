# Test configuration
enable_testing()

# Fetch doctest
include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.12
)
FetchContent_MakeAvailable(doctest)

# Add unit tests
set(UNIT_TESTS
    unit/test_basics.cpp
    unit/test_queries.cpp
    unit/test_set_operations.cpp
    unit/test_edge_cases.cpp
    unit/test_serialization.cpp
    unit/test_node_transitions.cpp
    unit/test_memory.cpp
    unit/test_complex_scenarios.cpp
    unit/test_fuzz.cpp
    unit/test_node16_32_setops.cpp
)

# Create test executable
add_executable(vebitset_tests ${UNIT_TESTS})
target_include_directories(vebitset_tests 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
    PRIVATE ${doctest_SOURCE_DIR}/doctest
)
target_link_libraries(vebitset_tests PRIVATE vebtree)

# Register tests with ctest
add_test(NAME VEBTreeTests COMMAND vebitset_tests)
