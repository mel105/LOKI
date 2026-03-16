# cmake/Dependencies.cmake
# ─────────────────────────────────────────────────────────────────────────────
# All third-party dependencies fetched via CMake FetchContent.
# No vendored source code in the repository.
#
# Dependency table:
#   Eigen3         3.4.0    Linear algebra, LSQ, SVD, Kalman
#   nlohmann_json  3.11.3   Configuration files (loki.json)
#   Catch2         3.5.2    Unit and integration tests
# ─────────────────────────────────────────────────────────────────────────────

include(FetchContent)

# ── Eigen3 ────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    Eigen3
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG        3.4.0
    GIT_SHALLOW    TRUE
)
# Eigen is header-only; disable its own tests to keep our build fast.
set(EIGEN_BUILD_DOC     OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING       OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Eigen3)

# ── nlohmann_json ─────────────────────────────────────────────────────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# ── Catch2 (only when tests are enabled) ─────────────────────────────────────
if(LOKI_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.2
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    # Make Catch2's CMake helpers (catch_discover_tests) available.
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
endif()
