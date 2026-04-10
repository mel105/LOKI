cmake_minimum_required(VERSION 3.25)

add_library(loki_evt STATIC
    src/gpd.cpp
    src/gev.cpp
    src/thresholdSelector.cpp
    src/evtAnalyzer.cpp
    src/plotEvt.cpp
)

target_include_directories(loki_evt PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_evt PUBLIC loki_core)

target_compile_features(loki_evt PUBLIC cxx_std_20)
