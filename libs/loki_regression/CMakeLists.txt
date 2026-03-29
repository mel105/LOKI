cmake_minimum_required(VERSION 3.25)

add_library(loki_regression STATIC)

target_sources(loki_regression
    PRIVATE
        src/calibrationRegressor.cpp
        src/harmonicRegressor.cpp
        src/linearRegressor.cpp
        src/polynomialRegressor.cpp
        src/regressionDiagnostics.cpp
        src/regressorUtils.cpp
        src/robustRegressor.cpp
        src/trendEstimator.cpp
        src/plotRegression.cpp
)

target_include_directories(loki_regression
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_regression
    PUBLIC  loki_core
    PRIVATE loki_compiler_options
)

add_library(loki::regression ALIAS loki_regression)
