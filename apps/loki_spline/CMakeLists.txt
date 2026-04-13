cmake_minimum_required(VERSION 3.25)

add_executable(loki_spline_app main.cpp)

set_target_properties(loki_spline_app PROPERTIES OUTPUT_NAME "loki_spline")

target_link_libraries(loki_spline_app
    PRIVATE
        loki_spline
        loki_core
)

target_compile_features(loki_spline_app PRIVATE cxx_std_20)
