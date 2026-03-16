#!/usr/bin/env bash
# scripts/build.sh — configure and build LOKI
# Usage: ./scripts/build.sh [debug|release]   (default: debug)

set -euo pipefail

PRESET="${1:-debug}"

echo "[LOKI] Configuring with preset '${PRESET}'..."
cmake --preset "${PRESET}"

echo "[LOKI] Building..."
cmake --build --preset "${PRESET}"

echo "[LOKI] Build complete → build/${PRESET}/"
