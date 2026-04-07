cmake_minimum_required(VERSION 3.25)

add_library(loki_clustering STATIC)

target_sources(loki_clustering
    PRIVATE
        src/kMeans.cpp
        src/dbscan.cpp
        src/clusteringAnalyzer.cpp
        src/plotClustering.cpp
)

target_include_directories(loki_clustering
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_clustering
    PUBLIC
        loki_core
    PRIVATE
        loki_compiler_options
)

target_include_directories(loki_clustering SYSTEM
    PUBLIC
        $<TARGET_PROPERTY:Eigen3::Eigen,INTERFACE_INCLUDE_DIRECTORIES>
)

add_library(loki::clustering ALIAS loki_clustering)
