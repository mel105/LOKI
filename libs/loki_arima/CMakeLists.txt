cmake_minimum_required(VERSION 3.25)

add_library(loki_arima STATIC
    src/arimaFitter.cpp
    src/arimaOrderSelector.cpp
    src/arimaForecaster.cpp
    src/arimaAnalyzer.cpp
)

target_include_directories(loki_arima
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/loki_core/include
        ${CMAKE_SOURCE_DIR}/libs/loki_stationarity/include
)

target_link_libraries(loki_arima
    PUBLIC
        loki_core
        loki_stationarity
)

set_target_properties(loki_arima PROPERTIES
    CXX_STANDARD          20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

target_compile_options(loki_arima
    PRIVATE
        -Wall -Wextra -Wpedantic
)
