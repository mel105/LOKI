cmake_minimum_required(VERSION 3.25)

add_executable(loki_gnss_app main.cpp)

set_target_properties(loki_gnss_app PROPERTIES OUTPUT_NAME "loki_gnss")

target_link_libraries(loki_gnss_app
    PRIVATE
        loki_gnss
        loki_core
)

target_compile_features(loki_gnss_app PRIVATE cxx_std_20)
