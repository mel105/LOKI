cmake_minimum_required(VERSION 3.25)

add_library(loki_ssa STATIC)

target_sources(loki_ssa
    PRIVATE
        src/ssaGrouper.cpp
        src/ssaReconstructor.cpp
        src/ssaAnalyzer.cpp
        src/plotSsa.cpp
)

target_include_directories(loki_ssa
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_ssa
    PUBLIC  loki_core
    PRIVATE loki_compiler_options
)

add_library(loki::ssa ALIAS loki_ssa)
