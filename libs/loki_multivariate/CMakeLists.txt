cmake_minimum_required(VERSION 3.25)

add_library(loki_multivariate STATIC)

target_sources(loki_multivariate
    PRIVATE
        src/multivariateSeries.cpp
        src/multivariateAssembler.cpp
        src/multivariateAnalyzer.cpp
        src/plotMultivariate.cpp
        src/pca.cpp
        src/mssa.cpp
        src/var.cpp
        src/factorAnalysis.cpp
        src/cca.cpp
        src/lda.cpp
        src/mahalanobis.cpp
        src/manova.cpp
        src/multivariateProtocol.cpp
)

target_include_directories(loki_multivariate
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_multivariate
    PUBLIC  loki::core
    PRIVATE loki_compiler_options
)

target_include_directories(loki_multivariate SYSTEM
    PUBLIC
        $<TARGET_PROPERTY:Eigen3::Eigen,INTERFACE_INCLUDE_DIRECTORIES>
)

add_library(loki::multivariate ALIAS loki_multivariate)
