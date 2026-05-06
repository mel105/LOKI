cmake_minimum_required(VERSION 3.25)

add_library(loki_gnss STATIC
    src/rinexNavParser.cpp
    src/rinexObsParser.cpp
    src/keplerOrbit.cpp
)

target_include_directories(loki_gnss
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_gnss
    PUBLIC
        loki_core
    PRIVATE
        z
)

target_compile_features(loki_gnss PRIVATE cxx_std_20)
