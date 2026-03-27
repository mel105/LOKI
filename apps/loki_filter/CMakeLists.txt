cmake_minimum_required(VERSION 3.20)

# ----------------------------------------------------------------------------
# loki_filter -- signal filtering application
# ----------------------------------------------------------------------------

add_executable(loki_filter_app
    main.cpp
)

set_target_properties(loki_filter_app PROPERTIES
    OUTPUT_NAME "loki_filter"
)

target_link_libraries(loki_filter_app
    PRIVATE loki::core
    PRIVATE loki::filter
    PRIVATE loki_compiler_options
)

install(TARGETS loki_filter_app RUNTIME DESTINATION bin)
