#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

echo "[1/2] Limpando e compilando..."
make clean || true
make

echo "[2/2] OK: binários em build/"
