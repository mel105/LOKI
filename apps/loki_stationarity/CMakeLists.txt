cmake_minimum_required(VERSION 3.25)

add_executable(loki_stationarity_app
    main.cpp
)

target_include_directories(loki_stationarity_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/loki_core/include
        ${CMAKE_SOURCE_DIR}/libs/loki_stationarity/include
)

target_link_libraries(loki_stationarity_app
    PRIVATE
        loki_core
        loki_stationarity
)

set_target_properties(loki_stationarity_app PROPERTIES
    OUTPUT_NAME           "loki_stationarity"
    CXX_STANDARD          20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

target_compile_options(loki_stationarity_app
    PRIVATE
        -Wall -Wextra -Wpedantic
)
