cmake_minimum_required(VERSION 3.25)

# ─────────────────────────────────────────────────────────────────────────────
# loki_core — foundational library
#   • exception hierarchy
#   • Logger
#   • TimeSeries / TimeStamp
#   • Config + ConfigLoader
#   • DirectoryScanner
#   • Loader / DataManager
# ─────────────────────────────────────────────────────────────────────────────

add_library(loki_core STATIC)

target_sources(loki_core
    PRIVATE
        src/core/logger.cpp
        src/core/configLoader.cpp
        src/core/directoryScanner.cpp
        src/timeseries/timeStamp.cpp
        src/timeseries/timeSeries.cpp
        src/io/loader.cpp
        src/io/dataManager.cpp
        src/io/gnuplot.cpp
        src/io/plot.cpp
        src/stats/descriptive.cpp
)

target_include_directories(loki_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_core
    PUBLIC  Eigen3::Eigen
    PUBLIC  nlohmann_json::nlohmann_json
    PRIVATE loki_compiler_options
)

add_library(loki::core ALIAS loki_core)
