cmake_minimum_required(VERSION 3.25)

# -----------------------------------------------------------------------------
# loki_outlier -- outlier detection and replacement
#   O1: outlierResult.hpp
#   O2: IqrDetector, MadDetector, ZScoreDetector
#   O3: OutlierCleaner (planned)
#   O4: HatMatrixDetector (planned)
# -----------------------------------------------------------------------------

add_library(loki_outlier STATIC)

target_sources(loki_outlier
    PRIVATE
        src/outlierDetector.cpp
        src/iqrDetector.cpp
        src/madDetector.cpp
        src/zScoreDetector.cpp
        src/outlierCleaner.cpp
        src/plotOutlier.cpp
)

target_include_directories(loki_outlier
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_outlier
    PUBLIC  loki_core
    PRIVATE loki_compiler_options
)

add_library(loki::outlier ALIAS loki_outlier)