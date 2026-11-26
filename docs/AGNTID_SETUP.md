# AgntID Integration Architecture & Verification Guide

**Platform:** NEXUS Temporal Security Intelligence for AI Agents  
**Component:** System 05 — AgntID Runtime Security Adapter (`agent_orchestrator/agntid_adapter.py`)  
**Current Evaluation Status:** `UNAVAILABLE / NOT_EVALUATED` (Advisory Mode)  

---

## 1. Truthful Integration Status Baseline

In accordance with strict hackathon scientific integrity rules:
- **AgntID adapter scaffold is present; official runtime validation is pending credentials and documented API contract.**
- **NEXUS policy is advisory when AgntID is unavailable.**
- **All current decisions in local demos are NEXUS decisions, not AgntID decisions.**
- **Mock external egress is simulated locally and performs no real network transfer.**

When no official AgntID package or endpoint is configured, NEXUS records AgntID decisions honestly as:
```json
{
  "agntid_available": false,
  "status": "UNAVAILABLE",
  "decision": "NOT_EVALUATED",
  "action_blocked": false,
  "enforced": false,
  "reason": "AgntID adapter scaffold is present; official runtime validation is pending credentials and documented API contract.",
  "source": "unavailable",
  "raw_response": null
}
```

---

## 2. Integration Modes & Contract Verification

NEXUS decouples **temporal anomaly detection** from **runtime policy enforcement**. The adapter architecture is designed to attach to official AgntID distribution channels without inventing unverified APIs:

```
+-----------------------------------------------------------------------------------+
|                        NEXUS Tool Execution Harness                               |
|                (agent_orchestrator/agent_runner.py)                               |
+-----------------------------------------+-----------------------------------------+
                                          |
                                          v
+-----------------------------------------------------------------------------------+
|               NEXUS AgntID Adapter Factory (get_agntid_adapter)                    |
+-------------------+---------------------+--------------------+--------------------+
                    |                     |                    |
        (Option A: Python SDK)   (Option B: CLI)      (Option C: HTTP API)
                    |                     |                    |
                    v                     v                    v
         [agntid Python Package]     [agntid binary]      [AGNTID_ENDPOINT]
```

### Option A: Official Python SDK (`agntid` package)
If the official AgntID Python package or challenge wheel is provided:
```bash
pip install /path/to/official_agntid.whl
```
NEXUS inspects exported evaluation methods (`evaluate_action`, `check_permission`, or `evaluate`). If methods are confirmed, it activates verified execution.

### Option B: Official CLI (`agntid` in PATH)
If an official AgntID binary is installed:
```bash
which agntid
```
NEXUS detects the binary via system PATH. To prevent guessing command-line syntax, it requires the official CLI contract flags documented by organizers.

### Option C: Official HTTP Runtime Endpoint
If an AgntID gateway or daemon is running:
```bash
export AGNTID_ENDPOINT="https://<official-gateway-url>"
export AGNTID_API_KEY="<official-api-key>"
```
NEXUS attaches to the endpoint and executes the contract probe to verify connectivity before dispatching live requests.

---

## 3. Four Lifecycle Statuses

| Status | Meaning | Live Execution Allowed? |
| :--- | :--- | :---: |
| **`UNAVAILABLE`** | No SDK, binary, or endpoint found. Decisions recorded as `NOT_EVALUATED`. | No |
| **`CONFIGURED_BUT_UNVERIFIED`** | Target detected, but contract schema unverified. Refuses to guess payload schemas. | No |
| **`VERIFIED_LIVE`** | Official probe verified via documented contract. Live decisions unlocked. | **Yes** |
| **`ERROR`** | Target threw an exception during invocation. | No |

---

## 4. Verification Commands

1. **Audit Environment Components**:
   ```bash
   .venv/bin/python agent_orchestrator/agntid_contract_probe.py
   ```
2. **Execute Live Verification Harness**:
   ```bash
   ./scripts/validate_agntid_live.sh
   ```
   *Note: Halts immediately if unconfigured. Executes one safe approved-intent operation only if VERIFIED_LIVE.*
