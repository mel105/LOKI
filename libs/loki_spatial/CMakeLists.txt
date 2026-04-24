cmake_minimum_required(VERSION 3.25)

add_library(loki_spatial STATIC
    src/spatialKriging.cpp
    src/spatialKrigingBase.cpp
    src/spatialInterp.cpp
    src/spatialAnalyzer.cpp
    src/plotSpatial.cpp
)

target_include_directories(loki_spatial
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_spatial
    PUBLIC
        loki_core
)

target_include_directories(loki_spatial SYSTEM PUBLIC
    $<TARGET_PROPERTY:Eigen3::Eigen,INTERFACE_INCLUDE_DIRECTORIES>)

target_compile_features(loki_spatial PRIVATE cxx_std_20)
