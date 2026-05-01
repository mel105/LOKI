cmake_minimum_required(VERSION 3.20)

# ----------------------------------------------------------------------------
# loki_multivariate -- multivariate time series analysis application
# ----------------------------------------------------------------------------

add_executable(loki_multivariate_app
    main.cpp
)

set_target_properties(loki_multivariate_app PROPERTIES
    OUTPUT_NAME "loki_multivariate"
)

target_link_libraries(loki_multivariate_app
    PRIVATE loki::core
    PRIVATE loki::multivariate
    PRIVATE loki_compiler_options
)

install(TARGETS loki_multivariate_app RUNTIME DESTINATION bin)
