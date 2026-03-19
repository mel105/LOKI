cmake_minimum_required(VERSION 3.20)

# =============================================================================
# tests/CMakeLists.txt
#
# Registers all Catch2 unit tests for the LOKI project.
# Enabled when cmake option LOKI_BUILD_TESTS=ON (default: ON in debug preset).
# =============================================================================

# Catch2 is fetched by the top-level Dependencies.cmake.
# It must be available before this file is processed.
# find_package(Catch2 3 REQUIRED)

include(CTest)
include(Catch)   # provides catch_discover_tests()

# =============================================================================
# Helper macro: loki_add_test_exe
#
# Creates a test executable from one or more source files, links it against
# the requested LOKI libraries and Catch2, then auto-registers all TEST_CASE
# macros found in the sources via catch_discover_tests().
#
# Usage:
#   loki_add_test_exe(
#     NAME    <target-name>
#     SOURCES <file1.cpp> [file2.cpp ...]
#     LIBS    <lib1> [lib2 ...]
#   )
# =============================================================================
macro(loki_add_test_exe)
    cmake_parse_arguments(ARG "" "NAME" "SOURCES;LIBS" ${ARGN})

    add_executable(${ARG_NAME} ${ARG_SOURCES})

    target_link_libraries(${ARG_NAME}
        PRIVATE
            ${ARG_LIBS}
            Catch2::Catch2WithMain
    )

    # Make project headers available without install step
    target_include_directories(${ARG_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/libs/loki_core/include
    )

    catch_discover_tests(${ARG_NAME}
        DISCOVERY_MODE PRE_TEST
    )
endmacro()

# =============================================================================
# Test executables
# =============================================================================

# --- core/exceptions ----------------------------------------------------------
loki_add_test_exe(
    NAME    test_exceptions
    SOURCES unit/core/test_exceptions.cpp
    LIBS    loki_core
)

# --- timeseries/timeStamp -----------------------------------------------------
loki_add_test_exe(
    NAME    test_timeStamp
    SOURCES unit/timeseries/test_timeStamp.cpp
    LIBS    loki_core
)

# --- timeseries/timeSeries ----------------------------------------------------
loki_add_test_exe(
    NAME    test_timeSeries
    SOURCES unit/timeseries/test_timeSeries.cpp
    LIBS    loki_core
)

# --- stats/descriptive --------------------------------------------------------
loki_add_test_exe(
    NAME    test_descriptive
    SOURCES unit/stats/test_descriptive.cpp
    LIBS    loki_core
)

# --- stats/summarize + NanPolicy ----------------------------------------------
loki_add_test_exe(
    NAME    test_summarize
    SOURCES unit/stats/test_summarize.cpp
    LIBS    loki_core
)

# --- changepoint detector -----------------------------------------------------
loki_add_test_exe(
    NAME    test_changePointDetector
    SOURCES unit/homogeneity/test_changePointDetector.cpp
    LIBS    loki_homogeneity loki_core
)

# --- multi-changepoint detector -----------------------------------------------
loki_add_test_exe(
    NAME    test_multiChangePointDetector
    SOURCES unit/homogeneity/test_multiChangePointDetector.cpp
    LIBS    loki_homogeneity loki_core
)

# --- gap filler----------------------------------------------------------------
loki_add_test_exe(
    NAME    test_gapFiller
    SOURCES unit/homogeneity/test_gapFiller.cpp
    LIBS    loki_homogeneity loki_core
)

# --- median year series constructor -------------------------------------------
loki_add_test_exe(
    NAME    test_medianYearSeries
    SOURCES unit/homogeneity/test_medianYearSeries.cpp
    LIBS    loki_core loki_homogeneity
)

# --- signal filter ------------------------------------------------------------
loki_add_test_exe(
    NAME    test_filter
    SOURCES unit/stats/test_filter.cpp
    LIBS    loki_core
)
 
# --- time series deseasonalising ----------------------------------------------
loki_add_test_exe(
    NAME    test_deseasonalizer
    SOURCES unit/homogeneity/test_deseasonalizer.cpp
    LIBS    loki_core loki_homogeneity
)

# --- time series adjustment ----------------------------------------------
loki_add_test_exe(
    NAME    test_seriesAdjuster
    SOURCES unit/homogeneity/test_seriesAdjuster.cpp
    LIBS    loki_core loki_homogeneity
)