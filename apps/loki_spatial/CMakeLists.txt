cmake_minimum_required(VERSION 3.25)

add_executable(loki_spatial_app main.cpp)

set_target_properties(loki_spatial_app PROPERTIES OUTPUT_NAME "loki_spatial")

target_link_libraries(loki_spatial_app
    PRIVATE
        loki_spatial
        loki_core
)

target_compile_features(loki_spatial_app PRIVATE cxx_std_20)
