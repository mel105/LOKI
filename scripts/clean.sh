#!/usr/bin/env bash
# scripts/clean.sh -- remove build artefacts and optionally rebuild
#
# Usage:
#   ./scripts/clean.sh [debug|release|all|rebuild] [--tests]
#
# Commands:
#   debug     Remove build/debug/ only.
#   release   Remove build/release/ only.
#   all       Remove both build/debug/ and build/release/  (default).
#   rebuild   Clean all, then run build.sh debug --copy-dlls.
#             Pass --tests to also build the test suite.
#
# Examples:
#   ./scripts/clean.sh
#   ./scripts/clean.sh debug
#   ./scripts/clean.sh rebuild
#   ./scripts/clean.sh rebuild --tests

set -euo pipefail

# -----------------------------------------------------------------------------
# Argument parsing
# -----------------------------------------------------------------------------

TARGET="all"
EXTRA_FLAGS=""

for arg in "$@"; do
    case "${arg}" in
        debug|release|all|rebuild) TARGET="${arg}" ;;
        --tests) EXTRA_FLAGS="${EXTRA_FLAGS} --tests" ;;
        *)
            echo "Usage: clean.sh [debug|release|all|rebuild] [--tests]" >&2
            exit 1
            ;;
    esac
done

# -----------------------------------------------------------------------------
# Sanity check
# -----------------------------------------------------------------------------

if [ ! -f "CMakeLists.txt" ]; then
    echo "[LOKI] ERROR: Run this script from the repository root." >&2
    exit 1
fi

# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------

remove_build() {
    local dir="build/${1}"
    if [ -d "${dir}" ]; then
        echo "[LOKI] Removing ${dir}/ ..."
        rm -rf "${dir}"
        echo "[LOKI] Removed ${dir}/"
    else
        echo "[LOKI] ${dir}/ does not exist -- skipping."
    fi
}

# -----------------------------------------------------------------------------
# Execute
# -----------------------------------------------------------------------------

case "${TARGET}" in
    debug)
        remove_build debug
        ;;
    release)
        remove_build release
        ;;
    all)
        remove_build debug
        remove_build release
        ;;
    rebuild)
        remove_build debug
        remove_build release
        echo "[LOKI] Starting fresh build (debug + DLLs)..."
        # shellcheck disable=SC2086
        bash "$(dirname "$0")/build.sh" debug --copy-dlls ${EXTRA_FLAGS}
        ;;
    *)
        echo "Usage: clean.sh [debug|release|all|rebuild] [--tests]" >&2
        exit 1
        ;;
esac

echo "[LOKI] Clean complete."