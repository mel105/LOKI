cmake_minimum_required(VERSION 3.25)

add_executable(loki_decomposition_app
    main.cpp
)

target_include_directories(loki_decomposition_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/loki_core/include
        ${CMAKE_SOURCE_DIR}/libs/loki_decomposition/include
)

target_link_libraries(loki_decomposition_app
    PRIVATE
        loki_core
        loki_decomposition
)

set_target_properties(loki_decomposition_app PROPERTIES
    OUTPUT_NAME           "loki_decomposition"
    CXX_STANDARD          20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

target_compile_options(loki_decomposition_app
    PRIVATE
        -Wall -Wextra -Wpedantic
)
