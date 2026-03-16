cmake_minimum_required(VERSION 3.25)

# ─────────────────────────────────────────────────────────────────────────────
# LOKI test suite — Catch2 v3
# ─────────────────────────────────────────────────────────────────────────────

include(CTest)
include(Catch)   # provides catch_discover_tests()

add_subdirectory(unit)
add_subdirectory(integration)
