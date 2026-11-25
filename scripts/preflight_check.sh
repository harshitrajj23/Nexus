#!/usr/bin/env bash
# ==============================================================================
# NEXUS Preflight Check (Symlink/alias to judge_preflight.sh)
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SCRIPT_DIR}/judge_preflight.sh" "$@"
