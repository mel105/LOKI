cmake_minimum_required(VERSION 3.25)

add_executable(loki_geodesy_app main.cpp)

set_target_properties(loki_geodesy_app PROPERTIES OUTPUT_NAME "loki_geodesy")

target_link_libraries(loki_geodesy_app
    PRIVATE
        loki_geodesy
        loki_core
)

target_compile_features(loki_geodesy_app PRIVATE cxx_std_20)
