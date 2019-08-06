cmake_minimum_required      (VERSION 3.10.2)

project                     ("LOKI" LANGUAGES CXX)

file                        (GLOB_RECURSE "${CMAKE_PROJECT_NAME}_src" "*.cpp" "*.h" ".hpp")

add_executable              ("${CMAKE_PROJECT_NAME}" "${${CMAKE_PROJECT_NAME}_src}")

target_include_directories  ("${CMAKE_PROJECT_NAME}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories  ("${CMAKE_PROJECT_NAME}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/externals")
target_include_directories  ("${CMAKE_PROJECT_NAME}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tst")

target_link_libraries       ("${CMAKE_PROJECT_NAME}" PRIVATE "nlohmann_json::nlohmann_json")
target_link_libraries       ("${CMAKE_PROJECT_NAME}" PRIVATE "newmat")

source_group		    (TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES "${${CMAKE_PROJECT_NAME}_src}")

add_custom_command	    (TARGET "${CMAKE_PROJECT_NAME}" POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/res" "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>")