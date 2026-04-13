cmake_minimum_required(VERSION 3.25)

add_library(loki_spline STATIC
    src/splineAnalyzer.cpp
    src/plotSpline.cpp
)

target_include_directories(loki_spline
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_spline
    PUBLIC
        loki_core
)

target_compile_features(loki_spline PRIVATE cxx_std_20)
