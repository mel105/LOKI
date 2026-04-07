cmake_minimum_required(VERSION 3.25)

add_library(loki_qc STATIC)

target_sources(loki_qc
    PRIVATE
        src/qcAnalyzer.cpp
        src/plotQc.cpp
)

target_include_directories(loki_qc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_qc
    PUBLIC
        loki_core
        loki_outlier
    PRIVATE
        loki_compiler_options
)

add_library(loki::qc ALIAS loki_qc)
