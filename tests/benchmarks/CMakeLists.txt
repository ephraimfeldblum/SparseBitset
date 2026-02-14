cmake_minimum_required(VERSION 3.16)

project(vEBitset_Benchmarks)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(vebitset_bench
    bench_main.cpp
    bench_veb_micro.cpp
    bench_vector_bool_micro.cpp
    bench_roaring_micro.cpp
    bench_macro.cpp
)

target_link_libraries(vebitset_bench
    PRIVATE
        vebitset
        roaring
        nanobench::nanobench
)

target_include_directories(vebitset_bench
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(vebitset_bench PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
