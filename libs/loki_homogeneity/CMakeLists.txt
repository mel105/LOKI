cmake_minimum_required(VERSION 3.25)

# ─────────────────────────────────────────────────────────────────────────────
# loki_homogeneity — change point detection and homogeneity testing
#   • ChangePointDetector   (abstract base)
#   • SnhtDetector          (Standard Normal Homogeneity Test)
#   • SeriesAdjuster        (applies corrections after detection)
#   • Homogenizer           (orchestrates the full workflow)
# ─────────────────────────────────────────────────────────────────────────────

add_library(loki_homogeneity STATIC
    src/changePointDetector.cpp
    src/snhtDetector.cpp
    src/seriesAdjuster.cpp
    src/homogenizer.cpp
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
