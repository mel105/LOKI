#!/usr/bin/env bash
# scripts/loki.sh -- unified build/clean/run/test script for LOKI
#
# Usage:
#   ./scripts/loki.sh <command> [app] [options]
#
# Commands:
#   build    Configure and build (full cmake configure + make).
#   clean    Remove build directories.
#   run      Incremental build and run an application.
#   test     Build (if needed) and run the test suite.
#
# App (optional, default: loki):
#   loki             apps/loki/loki.exe
#   homogenization   apps/loki_homogeneity/homogenization.exe
#   outlier          apps/loki_outlier/loki_outlier.exe
#   filter           apps/loki_filter/loki_filter.exe
#   all              All apps (build/clean only).
#
# Options:
#   debug|release        Build preset (default: debug).
#   --tests              Enable Catch2 test suite (build/clean/test only).
#   --copy-dlls          Copy UCRT64 runtime DLLs after build.
#   --rebuild            Clean before building (clean/test only).
#   --filter <pattern>   Run only tests matching pattern (test only).
#   --verbose            Verbose test output (test only).
#   --list               List available tests without running (test only).
#   <config.json>        Config file path (run only, default per app).
#
# Examples:
#   ./scripts/loki.sh build
#   ./scripts/loki.sh build homogenization --copy-dlls
#   ./scripts/loki.sh build outlier --copy-dlls
#   ./scripts/loki.sh build all --tests --copy-dlls
#   ./scripts/loki.sh build release
#   ./scripts/loki.sh clean
#   ./scripts/loki.sh clean debug
#   ./scripts/loki.sh clean rebuild --tests
#   ./scripts/loki.sh run
#   ./scripts/loki.sh run homogenization
#   ./scripts/loki.sh run homogenization config/homogenization.json
#   ./scripts/loki.sh run outlier
#   ./scripts/loki.sh run outlier config/outlier.json
#   ./scripts/loki.sh test
#   ./scripts/loki.sh test --filter exceptions --verbose
#   ./scripts/loki.sh test --rebuild

set -euo pipefail

# -----------------------------------------------------------------------------
# Configuration
# -----------------------------------------------------------------------------

UCRT64_BIN="/c/msys64/ucrt64/bin"
CXX_COMPILER="${UCRT64_BIN}/g++.exe"
MAKE="${UCRT64_BIN}/mingw32-make.exe"
JOBS=4

RUNTIME_DLLS=(
    "libgcc_s_seh-1.dll"
    "libstdc++-6.dll"
    "libwinpthread-1.dll"
)

# App registry: app_name -> relative exe path and default config
declare -A APP_EXE=(
    [loki]="apps/loki/loki.exe"
    [homogenization]="apps/loki_homogeneity/homogenization.exe"
    [outlier]="apps/loki_outlier/loki_outlier.exe"
    [filter]="apps/loki_filter/loki_filter.exe"
)
declare -A APP_CONFIG=(
    [loki]="config/loki_homogeneity.json"
    [homogenization]="config/homogenization.json"
    [outlier]="config/outlier.json"
    [filter]="config/filter.json"
)

# -----------------------------------------------------------------------------
# Argument parsing
# -----------------------------------------------------------------------------

COMMAND=""
APP="loki"
PRESET="debug"
BUILD_TESTS=0
COPY_DLLS=0
REBUILD=0
FILTER=""
VERBOSE=0
LIST_ONLY=0
CONFIG_OVERRIDE=""

if [ $# -eq 0 ]; then
    echo "[LOKI] ERROR: No command given. Use: build | clean | run | test" >&2
    exit 1
fi

COMMAND="${1}"
shift

for arg in "$@"; do
    case "${arg}" in
        loki|homogenization|outlier|filter|all)
            APP="${arg}" ;;
        debug|release)
            PRESET="${arg}" ;;
        --tests)
            BUILD_TESTS=1 ;;
        --copy-dlls)
            COPY_DLLS=1 ;;
        --rebuild)
            REBUILD=1 ;;
        --verbose)
            VERBOSE=1 ;;
        --list)
            LIST_ONLY=1 ;;
        --filter)
            # handled below with index tracking
            ;;
        *)
            # Could be filter pattern or config file
            if [[ "${arg}" == *.json ]]; then
                CONFIG_OVERRIDE="${arg}"
            fi
            ;;
    esac
done

# Handle --filter <pattern> properly (needs next arg)
ARGS=("$@")
for (( i=0; i<${#ARGS[@]}; i++ )); do
    if [[ "${ARGS[$i]}" == "--filter" ]]; then
        FILTER="${ARGS[$i+1]:-}"
    fi
done

BUILD_DIR="build/${PRESET}"
TESTS_DIR="${BUILD_DIR}/tests"

# -----------------------------------------------------------------------------
# Sanity checks
# -----------------------------------------------------------------------------

if [ ! -f "CMakeLists.txt" ]; then
    echo "[LOKI] ERROR: Run from the repository root (where CMakeLists.txt is)." >&2
    exit 1
fi

if [ ! -f "${CXX_COMPILER}" ] && [ "${COMMAND}" = "build" ]; then
    echo "[LOKI] ERROR: Compiler not found: ${CXX_COMPILER}" >&2
    echo "       Install MSYS2 UCRT64 toolchain or adjust UCRT64_BIN in loki.sh." >&2
    exit 1
fi

# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------

copy_dlls_to() {
    local dest="${1}"
    [ -d "${dest}" ] || return 0
    echo "[LOKI] Copying UCRT64 runtime DLLs to ${dest}/ ..."
    for dll in "${RUNTIME_DLLS[@]}"; do
        local src="${UCRT64_BIN}/${dll}"
        if [ -f "${src}" ]; then
            cp "${src}" "${dest}/"
            echo "       copied: ${dll}"
        else
            echo "[LOKI] WARNING: DLL not found: ${src}" >&2
        fi
    done
}

check_dlls_in() {
    local dest="${1}"
    for dll in "${RUNTIME_DLLS[@]}"; do
        if [ ! -f "${dest}/${dll}" ]; then
            src="${UCRT64_BIN}/${dll}"
            [ -f "${src}" ] && cp "${src}" "${dest}/" && \
                echo "[LOKI] Copied missing DLL: ${dll}"
        fi
    done
}

do_cmake_configure() {
    local tests_flag
    [ "${BUILD_TESTS}" -eq 1 ] && tests_flag="-DLOKI_BUILD_TESTS=ON" \
                                || tests_flag="-DLOKI_BUILD_TESTS=OFF"

    local build_type
    [ "${PRESET}" = "release" ] && build_type="Release" || build_type="Debug"

    echo "[LOKI] Configuring '${PRESET}' build in ${BUILD_DIR}/ ..."
    mkdir -p "${BUILD_DIR}"

    cmake -S . -B "${BUILD_DIR}" \
        -G "MinGW Makefiles" \
        -DCMAKE_BUILD_TYPE="${build_type}" \
        -DCMAKE_CXX_COMPILER="${CXX_COMPILER}" \
        -DCMAKE_MAKE_PROGRAM="${MAKE}" \
        "${tests_flag}"
}

do_make() {
    echo "[LOKI] Building with ${JOBS} jobs..."
    "${MAKE}" -C "${BUILD_DIR}" -j${JOBS}
    echo "[LOKI] Build complete --> ${BUILD_DIR}/"
}

do_copy_dlls() {
    [ "${COPY_DLLS}" -eq 0 ] && return 0

    # Copy to all known app dirs
    for app_key in "${!APP_EXE[@]}"; do
        local exe_dir="${BUILD_DIR}/$(dirname "${APP_EXE[$app_key]}")"
        copy_dlls_to "${exe_dir}"
    done

    # Copy to tests dir if it exists
    copy_dlls_to "${TESTS_DIR}"

    echo "[LOKI] DLLs ready."
}

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
# Commands
# -----------------------------------------------------------------------------

cmd_build() {
    do_cmake_configure
    do_make
    do_copy_dlls
    echo "[LOKI] Done."
}

cmd_clean() {
    case "${APP}" in
        all|loki|homogenization)
            # clean operates on preset, not app
            ;;
    esac

    if [ "${REBUILD}" -eq 1 ]; then
        remove_build debug
        remove_build release
        echo "[LOKI] Starting fresh build..."
        BUILD_TESTS_SAVED=${BUILD_TESTS}
        COPY_DLLS=1
        cmd_build
        BUILD_TESTS=${BUILD_TESTS_SAVED}
    else
        case "${PRESET}" in
            debug)   remove_build debug ;;
            release) remove_build release ;;
            *)
                remove_build debug
                remove_build release
                ;;
        esac
    fi
    echo "[LOKI] Clean complete."
}

cmd_run() {
    # Validate app
    if [ "${APP}" = "all" ]; then
        echo "[LOKI] ERROR: 'run' does not support app=all. Specify: loki | homogenization | outlier" >&2
        exit 1
    fi
    if [ -z "${APP_EXE[$APP]+x}" ]; then
        echo "[LOKI] ERROR: Unknown app '${APP}'. Known: ${!APP_EXE[*]}" >&2
        exit 1
    fi

    if [ ! -d "${BUILD_DIR}" ]; then
        echo "[LOKI] Build directory not found. Run: ./scripts/loki.sh build first." >&2
        exit 1
    fi

    local exe_dir="${BUILD_DIR}/$(dirname "${APP_EXE[$APP]}")"
    local exe="${BUILD_DIR}/${APP_EXE[$APP]}"
    local config="${CONFIG_OVERRIDE:-${APP_CONFIG[$APP]}}"

    echo "[LOKI] Building (incremental)..."
    "${MAKE}" -C "${BUILD_DIR}" -j${JOBS}

    check_dlls_in "${exe_dir}"

    echo "[LOKI] Running: ${exe} ${config}"
    echo "-----------------------------------------------"
    "${exe}" "${config}"
}

cmd_test() {
    if [ "${PRESET}" = "release" ]; then
        echo "[LOKI] ERROR: Tests are only supported in debug builds." >&2
        exit 1
    fi

    if [ "${REBUILD}" -eq 1 ] || [ ! -d "${TESTS_DIR}" ]; then
        echo "[LOKI] Building with tests enabled..."
        BUILD_TESTS=1
        COPY_DLLS=1
        cmd_build
    else
        echo "[LOKI] Incremental build..."
        "${MAKE}" -C "${BUILD_DIR}" -j${JOBS}
    fi

    check_dlls_in "${TESTS_DIR}"

    if [ "${LIST_ONLY}" -eq 1 ]; then
        echo "[LOKI] Available tests:"
        cd "${BUILD_DIR}"
        ctest -N
        exit 0
    fi

    echo "[LOKI] Running tests..."
    echo "-----------------------------------------------"
    cd "${BUILD_DIR}"

    CTEST_ARGS="--output-on-failure"
    [ -n "${FILTER}" ] && CTEST_ARGS="${CTEST_ARGS} -R ${FILTER}"
    [ "${VERBOSE}" -eq 1 ] && CTEST_ARGS="${CTEST_ARGS} -V"

    # shellcheck disable=SC2086
    ctest ${CTEST_ARGS}
}

# -----------------------------------------------------------------------------
# Dispatch
# -----------------------------------------------------------------------------

case "${COMMAND}" in
    build) cmd_build ;;
    clean) cmd_clean ;;
    run)   cmd_run   ;;
    test)  cmd_test  ;;
    *)
        echo "[LOKI] ERROR: Unknown command '${COMMAND}'. Use: build | clean | run | test" >&2
        exit 1
        ;;
esac