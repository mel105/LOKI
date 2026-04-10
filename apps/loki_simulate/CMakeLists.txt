cmake_minimum_required(VERSION 3.25)

add_executable(loki_simulate_app main.cpp)

set_target_properties(loki_simulate_app PROPERTIES OUTPUT_NAME "loki_simulate")

target_link_libraries(loki_simulate_app
    PRIVATE
        loki_simulate
        loki_core
)

target_compile_features(loki_simulate_app PRIVATE cxx_std_20)
