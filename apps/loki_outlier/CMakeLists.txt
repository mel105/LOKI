cmake_minimum_required(VERSION 3.20)

add_executable(loki_outlier_app
    main.cpp
)

target_include_directories(loki_outlier_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/loki_core/include
        ${CMAKE_SOURCE_DIR}/libs/loki_homogeneity/include
        ${CMAKE_SOURCE_DIR}/libs/loki_outlier/include
)

target_link_libraries(loki_outlier_app
    PRIVATE
        loki_core
        loki_homogeneity
        loki_outlier
)

set_target_properties(loki_outlier_app PROPERTIES
    OUTPUT_NAME           "loki_outlier"
    CXX_STANDARD          20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

target_compile_options(loki_outlier_app
    PRIVATE
        -Wall -Wextra -Wpedantic
)