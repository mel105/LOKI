cmake_minimum_required(VERSION 3.25)

add_executable(loki_clustering_app
    main.cpp
)

target_include_directories(loki_clustering_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/loki_core/include
        ${CMAKE_SOURCE_DIR}/libs/loki_clustering/include
)

target_link_libraries(loki_clustering_app
    PRIVATE
        loki_core
        loki_clustering
)

set_target_properties(loki_clustering_app PROPERTIES
    OUTPUT_NAME           "loki_clustering"
    CXX_STANDARD          20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

target_compile_options(loki_clustering_app
    PRIVATE
        -Wall -Wextra -Wpedantic
)
