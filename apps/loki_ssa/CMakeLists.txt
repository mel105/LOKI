cmake_minimum_required(VERSION 3.25)

add_executable(loki_ssa_app
    main.cpp
)

target_include_directories(loki_ssa_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/loki_core/include
        ${CMAKE_SOURCE_DIR}/libs/loki_ssa/include
)

target_link_libraries(loki_ssa_app
    PRIVATE
        loki_core
        loki_ssa
)

set_target_properties(loki_ssa_app PROPERTIES
    OUTPUT_NAME           "loki_ssa"
    CXX_STANDARD          20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

target_compile_options(loki_ssa_app
    PRIVATE
        -Wall -Wextra -Wpedantic
)
