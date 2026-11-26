# Official AgntID Integration & Configuration Guide

**Platform:** NEXUS Temporal Security Intelligence for AI Agents  
**Component:** System 05 — AgntID Runtime Security Adapter (`agent_orchestrator/agntid_adapter.py`)  
**Current Evaluation Status:** `UNAVAILABLE / NOT_EVALUATED` (Transparent Advisory Mode)

---

## 1. Overview

NEXUS decouples **temporal anomaly detection** from **runtime policy enforcement**. While NEXUS constructs dynamic interaction graphs and reconstructs multi-hop attack trajectories, the official **AgntID runtime engine** is designed to evaluate individual tool actions against organizational policies and decide whether an action is:
- **`ALLOW`**: Permitted to execute within current context.
- **`DENY`**: Prohibited by policy; execution intercepted/blocked (`enforced: true`).
- **`CHALLENGE`**: Requires multi-factor authentication or interactive human operator approval.

In this competition submission environment, inspection confirmed that **no official AgntID SDK, CLI, or endpoint was installed or configured**. In accordance with hackathon scientific integrity rules, NEXUS reports AgntID decisions honestly as:
```json
{
  "agntid_available": false,
  "decision": "NOT_EVALUATED",
  "action_blocked": false,
  "enforced": false,
  "reason": "Official AgntID runtime SDK/CLI not configured in environment; NEXUS policy remains advisory.",
  "source": "unavailable"
}
```

This document explains exactly how evaluators or enterprise operators can connect official AgntID to NEXUS.

---

## 2. Integration Modes

The NEXUS AgntID adapter factory (`get_agntid_adapter()` in `agent_orchestrator/agntid_adapter.py`) dynamically detects and attaches to AgntID through three official mechanisms:

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
# In active virtual environment
pip install agntid
# Or from challenge distribution wheel:
pip install /path/to/agntid-*.whl
```
When `import agntid` succeeds, NEXUS automatically initializes `OfficialSDKAgntIDAdapter`, queries its exported evaluation method (`evaluate_action`, `check_permission`, or `evaluate`), and normalizes the response.

### Option B: Official CLI (`agntid` in PATH)
If an official AgntID binary is installed on the host:
```bash
which agntid
agntid auth login --api-key <YOUR_KEY>
```
NEXUS automatically detects the binary and invokes `agntid check --tool <TOOL> --action <ACTION> --resource <RESOURCE> --intent <INTENT> --json`.

### Option C: Official HTTP Runtime Endpoint
If an AgntID gateway or daemon is running locally or in the cloud, configure these environment variables:
```bash
export AGNTID_ENDPOINT="http://localhost:8080/v1/evaluate"  # or cloud endpoint
export AGNTID_API_KEY="agnt_live_xxxxxxxxxxxxxxxx"
export AGNTID_PROJECT_ID="proj_nexus_eval"
export AGNTID_ENFORCE_MODE="advisory"  # or "blocking"
```
When `AGNTID_ENDPOINT` (or `AGNTID_URL`) is present in the environment, NEXUS instantiates `OfficialAPIAgntIDAdapter`, issues POST requests with standard JSON payloads, and records the returned policy verdict.

---

## 3. Normalized Decision Schema

For every tool call, NEXUS records the AgntID verdict strictly separated from the NEXUS neural prediction and evidence score:

| Normalized Field | Type | Description |
|---|---|---|
| `agntid_available` | `boolean` | `true` if official integration is active; `false` otherwise. |
| `decision` | `string` | `ALLOW`, `DENY`, `CHALLENGE`, `ERROR`, or `NOT_EVALUATED`. |
| `action_blocked` | `boolean` | `true` only if the action was prevented from executing. |
| `enforced` | `boolean` | `true` only if actual runtime blocking occurred. |
| `reason` | `string` | Explanation provided by AgntID or adapter diagnostic. |
| `intent_context` | `string` | The declared agent task intent passed into the evaluation. |
| `source` | `string` | `official_sdk`, `official_cli`, `official_api`, or `unavailable`. |
| `raw_response` | `object \| null` | Sanitized response payload with all credentials/secrets stripped. |

---

## 4. Verification

To verify whether AgntID is currently detected:
```bash
./scripts/judge_preflight.sh
```
Look for the AgntID diagnostic line:
- If unconfigured:
  `[INFO]  Official AgntID Integration: Unavailable (unconfigured); NEXUS remains advisory`
- If configured:
  `[PASS]  Official AgntID Integration: Configured and verified via <source>`
