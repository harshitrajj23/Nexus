# AgntID Live Integration & Contract Verification Checklist

**Challenge Track:** AI Agent Security & Runtime Threat Intelligence (AgntID Challenge)  
**Component:** System 05 — AgntID Runtime Adapter & Verification Probe  
**Probe Tool:** `agent_orchestrator/agntid_contract_probe.py`  
**Execution Script:** `./scripts/validate_agntid_live.sh`  

---

## 1. Truthful Integration Status Baseline

In accordance with strict hackathon scientific integrity rules, NEXUS establishes unambiguous truth boundaries:

- **AgntID adapter scaffold is present; official runtime validation is pending credentials and documented API contract.**
- **NEXUS policy is advisory when AgntID is unavailable.**
- **All current decisions in local demos are NEXUS decisions, not AgntID decisions.**
- **Mock external egress is simulated locally and performs no real network transfer.**

### Four Explicit Lifecycle Statuses

```
+-----------------------------------------------------------------------------+
|                            AgntID Adapter Status                            |
+-----------------------------------------------------------------------------+
| 1. UNAVAILABLE                | No official SDK, binary, or endpoint found. |
|                               | Normalizes decision to NOT_EVALUATED.       |
| 2. CONFIGURED_BUT_UNVERIFIED  | Target detected, but contract schema        |
|                               | unverified. Refuses to guess payloads.      |
| 3. VERIFIED_LIVE              | Official probe verified via documented      |
|                               | contract. Live decisions unlocked.          |
| 4. ERROR                      | Official invocation threw an exception.     |
+-----------------------------------------------------------------------------+
```

> [!IMPORTANT]
> Only **`VERIFIED_LIVE`** may unlock labels such as *"AgntID decision observed"*. In all other states, the UI and API explicitly display `NOT_EVALUATED` and `enforced: false`.

---

## 2. Step-by-Step Evaluator Checklist

When official AgntID challenge wheels, binaries, or credentials become available during evaluation, follow this checklist to verify live integration:

### Step 1: Provide Official AgntID Credentials or Packages

Choose one of the official integration mechanisms:

#### Option A: Official Python SDK
If the AgntID organizers provide a Python wheel or pip package:
```bash
# Inside NEXUS repository root
source .venv/bin/activate
pip install /path/to/official_agntid.whl
# Or: pip install agntid
```

#### Option B: Official CLI Binary
If an official `agntid` binary executable is provided:
```bash
# Add executable to system PATH
chmod +x /path/to/agntid
export PATH="/path/to:$PATH"
which agntid
```

#### Option C: Official HTTP Gateway / Daemon
If an AgntID gateway endpoint is provided:
```bash
export AGNTID_ENDPOINT="https://gateway.agntid.dev/api/v1/evaluate"
export AGNTID_API_KEY="official_challenge_api_key_here"
```

---

### Step 2: Run the Contract Probe

Execute the non-intrusive environment contract audit:

```bash
.venv/bin/python agent_orchestrator/agntid_contract_probe.py
```

Expected diagnostic output when unconfigured:
```
============================================================================
NEXUS — AgntID Official Integration Contract Probe
============================================================================
Overall Status:  UNAVAILABLE
----------------------------------------------------------------------------
Detected Components:
  * Python SDK:    NO (not installed)
  * CLI Binary:    NO (not on PATH)
  * HTTP Endpoint: NO (AGNTID_ENDPOINT unset)
  * API Key:       NO (AGNTID_API_KEY unset)
  * Config File:   NO (none found)
----------------------------------------------------------------------------
Explanation:     AgntID adapter scaffold is present; official runtime validation is pending credentials and documented API contract.
============================================================================
```

---

### Step 3: Run the One-Command Live Validation

```bash
./scripts/validate_agntid_live.sh
```

**Guardrail behavior:**
- If unconfigured, the script halts immediately with an explanatory report and non-zero exit code.
- If verified live, it executes **exactly one safe, approved-intent operation**:
  - Task Intent: *"Validate mock credentials during an approved incident-response rotation."*
  - Action: `READ_SECRET` on `credentials/config.env` followed by local checksum validation.
- **Strict safety rule:** The script **never** sends adversarial exfiltration payloads or hazardous tool actions to a live external AgntID service.
- Upon verified completion, an immutable JSON evidence bundle is saved to `data/agntid_evidence/`.

---

## 3. What to Check in the Evidence Artifact

When a genuine call succeeds, inspect `data/agntid_evidence/agntid_live_evidence_<timestamp>.json`:

1. `integration_source`: Identifies whether `official_sdk`, `official_cli`, or `official_api` responded.
2. `raw_response`: Confirms secrets/tokens were automatically stripped (`[REDACTED_SECRET]`).
3. `normalized_decision`: Normalized to `ALLOW`, `DENY`, or `CHALLENGE`.
4. `enforcement_truth`: Accurately states whether the action was blocked or permitted by policy.
5. `reproducibility_command`: Exact command line used to reproduce the live decision.
