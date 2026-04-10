cmake_minimum_required(VERSION 3.25)

add_executable(loki_evt_app main.cpp)

set_target_properties(loki_evt_app PROPERTIES OUTPUT_NAME "loki_evt")

target_link_libraries(loki_evt_app
    PRIVATE
        loki_evt
        loki_core
)

target_compile_features(loki_evt_app PRIVATE cxx_std_20)
