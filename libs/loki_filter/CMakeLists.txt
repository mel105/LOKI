cmake_minimum_required(VERSION 3.25)

add_library(loki_filter STATIC
    src/movingAverageFilter.cpp
    src/emaFilter.cpp
    src/weightedMovingAverageFilter.cpp
    src/kernelSmoother.cpp
    src/loessFilter.cpp
    src/savitzkyGolayFilter.cpp
    src/filterWindowAdvisor.cpp
    src/plotFilter.cpp
)

target_include_directories(loki_filter
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_filter
    PUBLIC  loki::core
    PRIVATE loki_compiler_options
)

add_library(loki::filter ALIAS loki_filter)