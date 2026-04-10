cmake_minimum_required(VERSION 3.25)

add_library(loki_simulate STATIC
    src/arimaSimulator.cpp
    src/kalmanSimulator.cpp
    src/simulateAnalyzer.cpp
    src/plotSimulate.cpp
)

target_include_directories(loki_simulate
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_simulate
    PUBLIC
        loki_core
        loki_arima
        loki_kalman
)

target_compile_features(loki_simulate PUBLIC cxx_std_20)
