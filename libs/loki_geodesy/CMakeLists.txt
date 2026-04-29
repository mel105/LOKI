cmake_minimum_required(VERSION 3.25)

add_library(loki_geodesy STATIC
    src/coordTransform.cpp
    src/geodesicLine.cpp
    src/geodesyAnalyzer.cpp
    src/plotGeodesy.cpp
)

target_include_directories(loki_geodesy
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_geodesy
    PUBLIC
        loki_core
)

target_include_directories(loki_geodesy SYSTEM PUBLIC
    $<TARGET_PROPERTY:Eigen3::Eigen,INTERFACE_INCLUDE_DIRECTORIES>)

target_compile_features(loki_geodesy PRIVATE cxx_std_20)
