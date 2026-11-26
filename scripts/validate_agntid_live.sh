#!/usr/bin/env bash
# ==============================================================================
# NEXUS — Official AgntID Live Integration Validator
# ==============================================================================
# Strict Safety Guardrails:
# 1. Runs preflight checks.
# 2. Runs AgntID contract probe to detect genuine SDK / CLI / Endpoint.
# 3. STOPS IMMEDIATELY if unconfigured or if API contract is unverified.
# 4. Never executes adversarial exfiltration or unauthorized tools against live services.
# 5. Executes exactly ONE safe approved-intent operation ONLY IF VERIFIED_LIVE.
# 6. Saves immutable JSON evidence artifact upon verified success.
# ==============================================================================

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

echo "========================================================================"
echo " NEXUS — Official AgntID Live Integration Verification Harness"
echo " Repo: $REPO_ROOT"
echo "========================================================================"

# Step 1: Preflight Verification
echo ""
echo "[Step 1/3] Running Environment & Subsystem Preflight..."
./scripts/judge_preflight.sh

# Step 2: Contract Probe Audit
echo ""
echo "[Step 2/3] Probing Official AgntID Contract & Credentials..."
PROBE_OUTPUT=""
if ! PROBE_OUTPUT=$(.venv/bin/python agent_orchestrator/agntid_contract_probe.py); then
    EXIT_CODE=$?
    echo "$PROBE_OUTPUT"
    echo ""
    echo "========================================================================"
    echo " [!] OFFICIAL AGNTID INTEGRATION IS NOT ACTIVE — VALIDATION HALTED"
    echo "========================================================================"
    echo " Truthful Diagnostic:"
    echo "   * AgntID adapter scaffold is present; official runtime validation"
    echo "     is pending credentials and documented API contract."
    echo "   * NEXUS policy is advisory when AgntID is unavailable."
    echo "   * All current decisions in local demos are NEXUS decisions, not AgntID decisions."
    echo "   * Mock external egress is simulated locally and performs no real network transfer."
    echo ""
    echo " Missing Requirements to Activate Live Evaluation:"
    echo "   1. Set official endpoint: export AGNTID_ENDPOINT=\"https://gateway...\""
    echo "   2. Set official API key:  export AGNTID_API_KEY=\"sec_...\""
    echo "   3. Or install SDK:        pip install /path/to/official_agntid.whl"
    echo ""
    echo " See docs/AGNTID_LIVE_INTEGRATION_CHECKLIST.md for complete instructions."
    echo "========================================================================"
    exit "$EXIT_CODE"
fi

echo "$PROBE_OUTPUT"

# Step 3: Safe Live Operation Execution (Only reachable if VERIFIED_LIVE)
echo ""
echo "[Step 3/3] Executing Verified Safe Approved-Intent Operation..."
.venv/bin/python agent_orchestrator/agntid_contract_probe.py --execute

echo ""
echo "========================================================================"
echo " [PASS] AGNTID LIVE VERIFICATION COMPLETED SUCCESSFULLY"
echo " Immutable evidence saved in data/agntid_evidence/"
echo "========================================================================"
