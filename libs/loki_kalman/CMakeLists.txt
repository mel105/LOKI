cmake_minimum_required(VERSION 3.25)

add_library(loki_kalman STATIC
    src/kalmanModelBuilder.cpp
    src/kalmanFilter.cpp
    src/rtsSmoother.cpp
    src/emEstimator.cpp
    src/kalmanAnalyzer.cpp
    src/plotKalman.cpp
)

target_include_directories(loki_kalman
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/loki_core/include
)

target_link_libraries(loki_kalman
    PUBLIC
        loki_core
)

set_target_properties(loki_kalman PROPERTIES
    CXX_STANDARD          20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

target_compile_options(loki_kalman
    PRIVATE
        -Wall -Wextra -Wpedantic
)
