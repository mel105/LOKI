cmake_minimum_required(VERSION 3.25)

add_library(loki_kriging STATIC
    src/krigingAnalyzer.cpp
    src/plotKriging.cpp
)

target_include_directories(loki_kriging
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_kriging
    PUBLIC
        loki_core
)

target_include_directories(loki_kriging SYSTEM PUBLIC
    $<TARGET_PROPERTY:Eigen3::Eigen,INTERFACE_INCLUDE_DIRECTORIES>)

target_compile_features(loki_kriging PRIVATE cxx_std_20)
