cmake_minimum_required(VERSION 3.25)

add_library(loki_spectral STATIC
    src/fftAnalyzer.cpp
    src/lombScargle.cpp
    src/peakDetector.cpp
    src/spectrogramAnalyzer.cpp
    src/spectralAnalyzer.cpp
    src/plotSpectral.cpp
)

target_include_directories(loki_spectral
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/loki_core/include
)

target_link_libraries(loki_spectral
    PUBLIC
        loki_core
)

set_target_properties(loki_spectral PROPERTIES
    CXX_STANDARD          20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

target_compile_options(loki_spectral
    PRIVATE
        -Wall -Wextra -Wpedantic
)
