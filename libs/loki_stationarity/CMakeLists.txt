cmake_minimum_required(VERSION 3.25)

add_library(loki_stationarity STATIC)

target_sources(loki_stationarity
    PRIVATE
        src/adfTest.cpp
        src/kpssTest.cpp
        src/ppTest.cpp
        src/stationarityAnalyzer.cpp
)

target_include_directories(loki_stationarity
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_stationarity
    PUBLIC  loki_core
    PRIVATE loki_compiler_options
)

target_include_directories(loki_stationarity SYSTEM
    PUBLIC
        $<TARGET_PROPERTY:Eigen3::Eigen,INTERFACE_INCLUDE_DIRECTORIES>
)

add_library(loki::stationarity ALIAS loki_stationarity)
