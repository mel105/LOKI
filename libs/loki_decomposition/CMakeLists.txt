cmake_minimum_required(VERSION 3.25)

add_library(loki_decomposition STATIC
    src/classicalDecomposer.cpp
    src/stlDecomposer.cpp
    src/decompositionAnalyzer.cpp
    src/plotDecomposition.cpp
)

target_include_directories(loki_decomposition
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/loki_core/include
)

target_link_libraries(loki_decomposition
    PUBLIC
        loki_core
)

set_target_properties(loki_decomposition PROPERTIES
    CXX_STANDARD          20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

target_compile_options(loki_decomposition
    PRIVATE
        -Wall -Wextra -Wpedantic
)
