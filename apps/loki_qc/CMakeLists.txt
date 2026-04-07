cmake_minimum_required(VERSION 3.25)

add_executable(loki_qc_app
    main.cpp
)

target_include_directories(loki_qc_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/loki_core/include
        ${CMAKE_SOURCE_DIR}/libs/loki_outlier/include
        ${CMAKE_SOURCE_DIR}/libs/loki_qc/include
)

target_link_libraries(loki_qc_app
    PRIVATE
        loki_core
        loki_outlier
        loki_qc
)

set_target_properties(loki_qc_app PROPERTIES
    OUTPUT_NAME           "loki_qc"
    CXX_STANDARD          20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

target_compile_options(loki_qc_app
    PRIVATE
        -Wall -Wextra -Wpedantic
)
