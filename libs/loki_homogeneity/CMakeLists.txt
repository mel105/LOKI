cmake_minimum_required(VERSION 3.25)

# -----------------------------------------------------------------------------
# loki_homogeneity -- change point detection and homogeneity testing
#
# Sources are added incrementally as each thread is completed:
#   H1  changePointDetector   (done)
#   H2  multiChangePointDetector
#   H3  medianYearSeries
#   H4  harmonicSeries
#   H5  deseasonalizer
#   H6  seriesAdjuster
#   H7  homogenizer
# -----------------------------------------------------------------------------

add_library(loki_homogeneity STATIC
    src/changePointDetector.cpp
    src/multiChangePointDetector.cpp
    src/medianYearSeries.cpp
    src/deseasonalizer.cpp
)

target_include_directories(loki_homogeneity
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_homogeneity
    PUBLIC  loki::core
    PRIVATE loki_compiler_options
)

# Alias used by apps and tests: loki::homogeneity
add_library(loki::homogeneity ALIAS loki_homogeneity)