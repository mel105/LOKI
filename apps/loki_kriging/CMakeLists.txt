cmake_minimum_required(VERSION 3.25)

add_executable(loki_kriging_app main.cpp)

set_target_properties(loki_kriging_app PROPERTIES OUTPUT_NAME "loki_kriging")

target_link_libraries(loki_kriging_app
    PRIVATE
        loki_kriging
        loki_core
)

target_compile_features(loki_kriging_app PRIVATE cxx_std_20)
