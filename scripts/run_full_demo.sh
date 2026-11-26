#!/usr/bin/env bash
# ==============================================================================
# NEXUS End-to-End Verification & Demo Runner
# Phases:
#   Phase 1: Environment & Subsystem Preflight (judge_preflight.sh)
#   Phase 2: Scenarios A-D Safe Demonstration (judge_demo.py)
#   Phase 3: C++ Engine Unit & Integration Tests (CTest)
#   Phase 4: Python Adapter, Gateway & API Tests (including outcome assertions)
#
# Exits 0 ONLY if every phase passes; otherwise exits non-zero and names
# the failing phase explicitly.
# ==============================================================================
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${REPO_ROOT}"

PYTHON_BIN="python3"
if [ -f "${REPO_ROOT}/.venv/bin/python3" ]; then
    PYTHON_BIN="${REPO_ROOT}/.venv/bin/python3"
elif [ -f "${REPO_ROOT}/venv/bin/python3" ]; then
    PYTHON_BIN="${REPO_ROOT}/venv/bin/python3"
fi

fail_phase() {
    local phase_name="$1"
    local exit_code="$2"
    echo ""
    echo "========================================================================"
    echo " [FAIL] END-TO-END RUN FAILED IN PHASE: ${phase_name} (exit=${exit_code})"
    echo "========================================================================"
    exit "${exit_code}"
}

echo "========================================================================"
echo " NEXUS — FULL END-TO-END DEMO & TEST VERIFICATION RUNNER"
echo " Repo: ${REPO_ROOT}"
echo " Python: ${PYTHON_BIN}"
echo "========================================================================"

echo ""
echo ">>> [PHASE 1/4] Environment Preflight Check (scripts/judge_preflight.sh)"
./scripts/judge_preflight.sh || fail_phase "Phase 1: Preflight Check (scripts/judge_preflight.sh)" $?

echo ""
echo ">>> [PHASE 2/4] Scenarios A-D Judge Demonstration (agent_orchestrator/judge_demo.py)"
"${PYTHON_BIN}" "${REPO_ROOT}/agent_orchestrator/judge_demo.py" || fail_phase "Phase 2: Scenarios A-D Demo (judge_demo.py)" $?

echo ""
echo ">>> [PHASE 3/4] C++17 LibTorch CTest Suite (cpp_engine/build)"
ctest --test-dir "${REPO_ROOT}/cpp_engine/build" --output-on-failure || fail_phase "Phase 3: C++ Engine CTest Suite" $?

echo ""
echo ">>> [PHASE 4a/4] AgntID Adapter Contract Tests (test_agntid_adapter.py)"
"${PYTHON_BIN}" "${REPO_ROOT}/agent_orchestrator/test_agntid_adapter.py" || fail_phase "Phase 4a: AgntID Adapter Tests (test_agntid_adapter.py)" $?

echo ""
echo ">>> [PHASE 4b/4] Agent Tool Gateway Tests (test_agent_tool_gateway.py)"
"${PYTHON_BIN}" "${REPO_ROOT}/agent_orchestrator/test_agent_tool_gateway.py" || fail_phase "Phase 4b: Agent Tool Gateway Tests (test_agent_tool_gateway.py)" $?

echo ""
echo ">>> [PHASE 4c/4] FastAPI & Scenario Outcome Assertions (test_api.py)"
"${PYTHON_BIN}" "${REPO_ROOT}/agent_orchestrator/test_api.py" || fail_phase "Phase 4c: API & Scenario Assertions (test_api.py)" $?

echo ""
echo "========================================================================"
echo " [PASS] ALL PHASES COMPLETED SUCCESSFULLY (Preflight -> Demo -> Tests)"
echo "========================================================================"
exit 0
