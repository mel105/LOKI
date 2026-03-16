#!/usr/bin/env bash
# scripts/clean.sh — remove all build artefacts
# Usage: ./scripts/clean.sh [debug|release|all]   (default: all)

set -euo pipefail

TARGET="${1:-all}"

remove() {
    local dir="build/${1}"
    if [ -d "${dir}" ]; then
        echo "[LOKI] Removing ${dir}..."
        rm -rf "${dir}"
    else
        echo "[LOKI] ${dir} does not exist — skipping."
    fi
}

case "${TARGET}" in
    debug)   remove debug   ;;
    release) remove release ;;
    all)     remove debug; remove release ;;
    *)
        echo "Usage: clean.sh [debug|release|all]" >&2
        exit 1
        ;;
esac

echo "[LOKI] Clean complete."
