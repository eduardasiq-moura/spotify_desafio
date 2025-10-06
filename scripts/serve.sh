#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

PORT="${1:-8000}"
echo "🚀 Servindo HTTP em: http://localhost:${PORT}/out/dashboard.html"
echo "Pressione Ctrl+C para encerrar."
python3 -m http.server "$PORT"
