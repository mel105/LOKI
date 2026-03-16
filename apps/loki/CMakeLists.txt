cmake_minimum_required(VERSION 3.25)

# ─────────────────────────────────────────────────────────────────────────────
# loki — command-line application
# ─────────────────────────────────────────────────────────────────────────────

add_executable(loki
    main.cpp
)

target_link_libraries(loki
    PRIVATE loki::core
    PRIVATE loki::homogeneity
    PRIVATE loki_compiler_options
)

install(TARGETS loki RUNTIME DESTINATION bin)
