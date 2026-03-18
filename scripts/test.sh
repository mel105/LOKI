#!/usr/bin/env bash
# scripts/test.sh -- build (if needed) and run the LOKI test suite
#
# Assumes a debug build already exists. If the tests binary directory is
# missing, automatically rebuilds with --tests enabled.
#
# Usage:
#   ./scripts/test.sh [options]
#
# Options:
#   --rebuild       Force a clean rebuild before running tests.
#   --filter <pat>  Run only tests whose name contains <pat>.
#                   Passed directly to ctest -R.
#   --verbose       Print every assertion, not just failures.
#   --list          List all available test names without running them.
#
# Examples:
#   ./scripts/test.sh
#   ./scripts/test.sh --filter exceptions
#   ./scripts/test.sh --filter stats --verbose
#   ./scripts/test.sh --rebuild
#   ./scripts/test.sh --list

set -euo pipefail

# -----------------------------------------------------------------------------
# Configuration
# -----------------------------------------------------------------------------

UCRT64_BIN="/c/msys64/ucrt64/bin"
MAKE="${UCRT64_BIN}/mingw32-make.exe"
BUILD_DIR="build/debug"
TESTS_DIR="${BUILD_DIR}/tests"

RUNTIME_DLLS=(
    "libgcc_s_seh-1.dll"
    "libstdc++-6.dll"
    "libwinpthread-1.dll"
)

# -----------------------------------------------------------------------------
# Argument parsing
# -----------------------------------------------------------------------------

REBUILD=0
FILTER=""
VERBOSE=0
LIST_ONLY=0

while [[ $# -gt 0 ]]; do
    case "${1}" in
        --rebuild)        REBUILD=1 ;;
        --verbose)        VERBOSE=1 ;;
        --list)           LIST_ONLY=1 ;;
        --filter)
            shift
            FILTER="${1}"
            ;;
        *)
            echo "Usage: test.sh [--rebuild] [--filter <pat>] [--verbose] [--list]" >&2
            exit 1
            ;;
    esac
    shift
done

# -----------------------------------------------------------------------------
# Sanity check
# -----------------------------------------------------------------------------

if [ ! -f "CMakeLists.txt" ]; then
    echo "[LOKI] ERROR: Run from the repository root." >&2
    exit 1
fi

# -----------------------------------------------------------------------------
# Rebuild if requested or if tests were never built
# -----------------------------------------------------------------------------

if [ "${REBUILD}" -eq 1 ] || [ ! -d "${TESTS_DIR}" ]; then
    echo "[LOKI] Building with tests enabled..."
    bash "$(dirname "$0")/build.sh" debug --tests --copy-dlls
else
    # Incremental build only -- recompile changed files, keep tests
    echo "[LOKI] Incremental build..."
    "${MAKE}" -C "${BUILD_DIR}" -j4
fi

# -----------------------------------------------------------------------------
# DLL check for tests directory
# -----------------------------------------------------------------------------

for dll in "${RUNTIME_DLLS[@]}"; do
    if [ ! -f "${TESTS_DIR}/${dll}" ]; then
        src="${UCRT64_BIN}/${dll}"
        if [ -f "${src}" ]; then
            cp "${src}" "${TESTS_DIR}/"
            echo "[LOKI] Copied missing DLL to tests/: ${dll}"
        fi
    fi
done

# -----------------------------------------------------------------------------
# List mode -- just print available tests and exit
# -----------------------------------------------------------------------------

if [ "${LIST_ONLY}" -eq 1 ]; then
    echo "[LOKI] Available tests:"
    cd "${BUILD_DIR}"
    ctest -N
    exit 0
fi

# -----------------------------------------------------------------------------
# Run tests via ctest
# -----------------------------------------------------------------------------

echo "[LOKI] Running tests..."
echo "-----------------------------------------------"

cd "${BUILD_DIR}"

CTEST_ARGS="--output-on-failure"

if [ -n "${FILTER}" ]; then
    CTEST_ARGS="${CTEST_ARGS} -R ${FILTER}"
fi

if [ "${VERBOSE}" -eq 1 ]; then
    CTEST_ARGS="${CTEST_ARGS} -V"
fi

# shellcheck disable=SC2086
ctest ${CTEST_ARGS}