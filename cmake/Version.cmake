# cmake/Version.cmake
# ─────────────────────────────────────────────────────────────────────────────
# Exposes the project version (set in the root CMakeLists.txt project() call)
# as a configured header: libs/loki_core/include/loki/core/version.hpp
#
# The header is generated into the build tree and added to loki_core's include
# path so that #include <loki/core/version.hpp> always works.
# ─────────────────────────────────────────────────────────────────────────────

set(LOKI_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(LOKI_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(LOKI_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(LOKI_VERSION_STRING "${PROJECT_VERSION}")

configure_file(
    "${CMAKE_SOURCE_DIR}/libs/loki_core/include/loki/core/version.hpp.in"
    "${CMAKE_BINARY_DIR}/generated/loki/core/version.hpp"
    @ONLY
)
