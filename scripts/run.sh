#!/usr/bin/env bash
# scripts/run.sh -- incremental build and run LOKI
#
# Compiles only changed files (incremental), then runs the executable.
# Automatically copies UCRT64 runtime DLLs if they are missing.
# Run from the repository root.
#
# Usage:
#   ./scripts/run.sh [config]   (default: config/loki_homogeneity.json)
#
# Examples:
#   ./scripts/run.sh
#   ./scripts/run.sh config/loki_homogeneity.json

set -euo pipefail

# -----------------------------------------------------------------------------
# Configuration
# -----------------------------------------------------------------------------

UCRT64_BIN="/c/msys64/ucrt64/bin"
MAKE="${UCRT64_BIN}/mingw32-make"
BUILD_DIR="build/debug"
EXE_DIR="${BUILD_DIR}/apps/loki"
EXE="${EXE_DIR}/loki.exe"
CONFIG="${1:-config/loki_homogeneity.json}"

RUNTIME_DLLS=(
    "libgcc_s_seh-1.dll"
    "libstdc++-6.dll"
    "libwinpthread-1.dll"
)

# -----------------------------------------------------------------------------
# Sanity checks
# -----------------------------------------------------------------------------

if [ ! -f "CMakeLists.txt" ]; then
    echo "[LOKI] ERROR: Run from the repository root." >&2
    exit 1
fi

if [ ! -d "${BUILD_DIR}" ]; then
    echo "[LOKI] Build directory not found. Run ./scripts/build.sh first." >&2
    exit 1
fi

# -----------------------------------------------------------------------------
# Incremental build
# -----------------------------------------------------------------------------

echo "[LOKI] Building (incremental)..."
"${MAKE}" -C "${BUILD_DIR}" -j4

# -----------------------------------------------------------------------------
# DLL check -- copy if missing
# -----------------------------------------------------------------------------

for dll in "${RUNTIME_DLLS[@]}"; do
    if [ ! -f "${EXE_DIR}/${dll}" ]; then
        src="${UCRT64_BIN}/${dll}"
        if [ -f "${src}" ]; then
            cp "${src}" "${EXE_DIR}/"
            echo "[LOKI] Copied missing DLL: ${dll}"
        else
            echo "[LOKI] WARNING: DLL not found: ${src}" >&2
        fi
    fi
done

# -----------------------------------------------------------------------------
# Run
# -----------------------------------------------------------------------------

echo "[LOKI] Running: ${EXE} ${CONFIG}"
echo "-----------------------------------------------"
"${EXE}" "${CONFIG}"