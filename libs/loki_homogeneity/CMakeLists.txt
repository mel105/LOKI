cmake_minimum_required(VERSION 3.25)

add_library(loki_homogeneity STATIC
    src/changePointDetector.cpp
    src/multiChangePointDetector.cpp
    src/snhtDetector.cpp
    src/peltDetector.cpp
    src/bocpdDetector.cpp
    src/seriesAdjuster.cpp
    src/homogenizer.cpp
    src/plotHomogeneity.cpp
    src/homogeneityAnalyzer.cpp
)

target_include_directories(loki_homogeneity
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/loki_outlier/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_homogeneity
    PUBLIC
        loki::core
        loki_outlier
        Eigen3::Eigen
    PRIVATE loki_compiler_options
)

target_compile_features(loki_homogeneity PUBLIC cxx_std_20)

set_target_properties(loki_homogeneity PROPERTIES
    CXX_EXTENSIONS OFF
)

add_library(loki::homogeneity ALIAS loki_homogeneity)