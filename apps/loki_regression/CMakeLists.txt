cmake_minimum_required(VERSION 3.20)

# ----------------------------------------------------------------------------
# loki_regression -- regression analysis application
# ----------------------------------------------------------------------------

add_executable(loki_regression_app
    main.cpp
)

set_target_properties(loki_regression_app PROPERTIES
    OUTPUT_NAME "loki_regression"
)

target_link_libraries(loki_regression_app
    PRIVATE loki::core
    PRIVATE loki::regression
    PRIVATE loki_compiler_options
)

install(TARGETS loki_regression_app RUNTIME DESTINATION bin)
