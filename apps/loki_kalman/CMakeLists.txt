cmake_minimum_required(VERSION 3.25)

add_executable(loki_kalman_app
    main.cpp
)

target_include_directories(loki_kalman_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/loki_core/include
        ${CMAKE_SOURCE_DIR}/libs/loki_kalman/include
)

target_link_libraries(loki_kalman_app
    PRIVATE
        loki_core
        loki_kalman
)

set_target_properties(loki_kalman_app PROPERTIES
    OUTPUT_NAME           "loki_kalman"
    CXX_STANDARD          20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

target_compile_options(loki_kalman_app
    PRIVATE
        -Wall -Wextra -Wpedantic
)
