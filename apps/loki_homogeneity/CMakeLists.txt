cmake_minimum_required(VERSION 3.20)

# ----------------------------------------------------------------------------
# loki_homogeneity -- homogeneity analysis application
# ----------------------------------------------------------------------------

add_executable(homogenization
    main.cpp
)

target_link_libraries(homogenization
    PRIVATE loki::core
    PRIVATE loki::homogeneity
    PRIVATE loki_compiler_options
)

install(TARGETS homogenization RUNTIME DESTINATION bin)
