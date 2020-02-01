cmake_minimum_required      (VERSION 3.10.0)

project                     ("LOKI" LANGUAGES CXX)

set                         (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/Build)
set                         (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/Build)
set                         (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/Build)

set                         (JSON_BuildTests OFF CACHE INTERNAL "")

set                         (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_subdirectory            ("${CMAKE_HOME_DIRECTORY}/Dependencies/json-3.5.0")
add_subdirectory            ("${CMAKE_PROJECT_NAME}")

set_property                (GLOBAL PROPERTY USE_FOLDERS ON)

if (MSVC)
    set_property            (DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${CMAKE_PROJECT_NAME}")
endif()