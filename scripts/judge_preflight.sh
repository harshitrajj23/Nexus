#!/usr/bin/env bash
# ==============================================================================
# NEXUS Hackathon Judge Preflight Verification Script
# Validates all Python, C++ LibTorch, and dashboard dependencies for evaluators.
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${REPO_ROOT}"

# Prefer virtual environment Python if available
PYTHON_BIN="python3"
if [ -f "${REPO_ROOT}/.venv/bin/python3" ]; then
    PYTHON_BIN="${REPO_ROOT}/.venv/bin/python3"
elif [ -f "${REPO_ROOT}/venv/bin/python3" ]; then
    PYTHON_BIN="${REPO_ROOT}/venv/bin/python3"
fi

"${PYTHON_BIN}" "${REPO_ROOT}/agent_orchestrator/judge_preflight.py"
