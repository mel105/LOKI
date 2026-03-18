#!/usr/bin/env bash
# scripts/build.sh -- configure and build LOKI
#
# Usage:
#   ./scripts/build.sh [debug|release] [--tests] [--copy-dlls]
#
# Options:
#   --tests       Enable building of unit tests (Catch2). Off by default.
#   --copy-dlls   Copy required UCRT64 runtime DLLs next to the executable.
#                 Required after first build or when switching toolchains.
#
# Examples:
#   ./scripts/build.sh
#   ./scripts/build.sh debug --tests
#   ./scripts/build.sh debug --tests --copy-dlls
#   ./scripts/build.sh release

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

# -----------------------------------------------------------------------------
# Argument parsing
# -----------------------------------------------------------------------------

PRESET="debug"
COPY_DLLS=0
BUILD_TESTS=0

for arg in "$@"; do
    case "${arg}" in
        debug|release) PRESET="${arg}" ;;
        --copy-dlls)   COPY_DLLS=1 ;;
        --tests)       BUILD_TESTS=1 ;;
        *)
            echo "Usage: build.sh [debug|release] [--tests] [--copy-dlls]" >&2
            exit 1
            ;;
    esac
done

BUILD_DIR="build/${PRESET}"
EXE_DIR="${BUILD_DIR}/apps/loki"

# -----------------------------------------------------------------------------
# Sanity checks
# -----------------------------------------------------------------------------

if [ ! -f "CMakeLists.txt" ]; then
    echo "[LOKI] ERROR: Run this script from the repository root." >&2
    exit 1
fi

if [ ! -f "${CXX_COMPILER}" ]; then
    echo "[LOKI] ERROR: Compiler not found: ${CXX_COMPILER}" >&2
    echo "       Install MSYS2 UCRT64 toolchain or adjust UCRT64_BIN in build.sh." >&2
    exit 1
fi

# -----------------------------------------------------------------------------
# CMake flags per preset
# -----------------------------------------------------------------------------

case "${PRESET}" in
    debug)
        CMAKE_BUILD_TYPE="Debug"
        ;;
    release)
        CMAKE_BUILD_TYPE="Release"
        ;;
esac

if [ "${BUILD_TESTS}" -eq 1 ]; then
    TESTS_FLAG="-DLOKI_BUILD_TESTS=ON"
else
    TESTS_FLAG="-DLOKI_BUILD_TESTS=OFF"
fi

# -----------------------------------------------------------------------------
# Configure
# -----------------------------------------------------------------------------

echo "[LOKI] Configuring '${PRESET}' build in ${BUILD_DIR}/ ..."
mkdir -p "${BUILD_DIR}"

cmake -S . -B "${BUILD_DIR}" \
    -G "MinGW Makefiles" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -DCMAKE_CXX_COMPILER="${CXX_COMPILER}" \
    -DCMAKE_MAKE_PROGRAM="${MAKE}" \
    "${TESTS_FLAG}"

# -----------------------------------------------------------------------------
# Build
# -----------------------------------------------------------------------------

echo "[LOKI] Building with ${JOBS} jobs..."
"${MAKE}" -C "${BUILD_DIR}" -j${JOBS}

echo "[LOKI] Build complete --> ${BUILD_DIR}/"

# -----------------------------------------------------------------------------
# Copy runtime DLLs (optional)
# -----------------------------------------------------------------------------

if [ "${COPY_DLLS}" -eq 1 ]; then
    TESTS_DIR="${BUILD_DIR}/tests"

    for dest in "${EXE_DIR}" "${TESTS_DIR}"; do
        if [ ! -d "${dest}" ]; then
            continue
        fi
        echo "[LOKI] Copying UCRT64 runtime DLLs to ${dest}/ ..."
        for dll in "${RUNTIME_DLLS[@]}"; do
            src="${UCRT64_BIN}/${dll}"
            if [ -f "${src}" ]; then
                cp "${src}" "${dest}/"
                echo "       copied: ${dll}"
            else
                echo "[LOKI] WARNING: DLL not found: ${src}" >&2
            fi
        done
    done
    echo "[LOKI] DLLs ready."
fi

echo "[LOKI] Done."