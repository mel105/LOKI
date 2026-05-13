cmake_minimum_required(VERSION 3.25)

add_library(loki_gnss STATIC
    src/rinexNavParser.cpp
    src/rinexObsParser.cpp
    src/keplerOrbit.cpp
    src/satVisibility.cpp
    src/sppSolver.cpp
    src/broadcastOrbit.cpp
    src/gnssUtils.cpp
    src/ionosphere.cpp
    src/troposphere.cpp
    src/relativity.cpp
    src/gnssAnalyzer.cpp
    src/gnssProtocol.cpp
    src/gnssCsvExport.cpp
    src/plotGnss.cpp
    src/solidTides.cpp
)

target_include_directories(loki_gnss
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(loki_gnss
    PUBLIC
        loki_core
        loki_geodesy
    PRIVATE
        -lz
)

target_include_directories(loki_gnss SYSTEM PUBLIC
    $<TARGET_PROPERTY:Eigen3::Eigen,INTERFACE_INCLUDE_DIRECTORIES>)

target_compile_features(loki_gnss PRIVATE cxx_std_20)