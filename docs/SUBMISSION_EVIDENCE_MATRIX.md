# NEXUS Submission Evidence Matrix

**Competition:** AgntID Agent Security Challenge  
**Evaluation Standard:** Objective Scientific Proof & Non-Fabrication Guarantee  
**Repository:** `/Users/harshitrajsingh/Nexus`  

---

## 1. Compliance & Evidence Matrix

| Requirement | Evidence File | Current Status | Truthful Limitation |
| :--- | :--- | :---: | :--- |
| **Useful Tool-Using Agent** | [`agent_orchestrator/agent_runner.py`](file:///Users/harshitrajsingh/Nexus/agent_orchestrator/agent_runner.py)<br>[`agent_orchestrator/agent_tool_gateway.py`](file:///Users/harshitrajsingh/Nexus/agent_orchestrator/agent_tool_gateway.py) | **Verified Local Execution** | Agent operates within a safe local filesystem sandbox (`/tmp`); mock external egress is simulated locally and performs no real network transfer. |
| **Temporal Graph Detection** | [`cpp_engine/src/graph_engine.cpp`](file:///Users/harshitrajsingh/Nexus/cpp_engine/src/graph_engine.cpp)<br>[`cpp_engine/tests/graph_test.cpp`](file:///Users/harshitrajsingh/Nexus/cpp_engine/tests/graph_test.cpp) | **Verified Live C++ Core** | Dynamic heterogeneous graphs enforce chronological edge order ($t_k \ge t_{k-1}$); CTest suite passes 4/4 in 0.59s. |
| **Model Inference** | [`cpp_engine/src/nexus_model.cpp`](file:///Users/harshitrajsingh/Nexus/cpp_engine/src/nexus_model.cpp)<br>[`data/nexus_best.pt`](file:///Users/harshitrajsingh/Nexus/data/nexus_best.pt) | **Verified Checkpoint** | Neural forward pass computes 4-class risk probabilities in ~4.5 ms. Predictions cluster in Normal/Suspicious on synthetic holdouts. |
| **Deterministic Evidence Scoring** | [`agent_orchestrator/hybrid_risk_engine.py`](file:///Users/harshitrajsingh/Nexus/agent_orchestrator/hybrid_risk_engine.py) | **Verified Deterministic** | Explicit, transparent scoring weights (+25 secret, +25 admin, +35 egress, +10 chain) prevent neural hallucination and explain policy decisions. |
| **Intent-Boundary Scenarios** | [`agent_orchestrator/intent_experiment.py`](file:///Users/harshitrajsingh/Nexus/agent_orchestrator/intent_experiment.py) | **Verified Live Evaluation** | Tests identical sensitive file (`credentials/config.env`) across Approved Rotation (40/100), Intent Mismatch (75/100), and Exfiltration (100/100). |
| **Causal Attack Path Reconstruction** | [`cpp_engine/src/graph_engine.cpp`](file:///Users/harshitrajsingh/Nexus/cpp_engine/src/graph_engine.cpp)<br>[`data/demo_runs/JUDGE_EVIDENCE.md`](file:///Users/harshitrajsingh/Nexus/data/demo_runs/JUDGE_EVIDENCE.md) | **Verified Automated Graph DFS** | Identifies exact multi-hop compromised paths (e.g. `Agent -> Secret -> Role -> Egress`) and generates natural language breach narrative. |
| **AgntID Adapter Readiness** | [`agent_orchestrator/agntid_adapter.py`](file:///Users/harshitrajsingh/Nexus/agent_orchestrator/agntid_adapter.py)<br>[`agent_orchestrator/agntid_contract_probe.py`](file:///Users/harshitrajsingh/Nexus/agent_orchestrator/agntid_contract_probe.py) | **Readiness Harness Ready** | AgntID adapter scaffold is present; official runtime validation is pending credentials and documented API contract. Refuses to guess payload schemas. |
| **Official Live AgntID Decision** | [`data/agntid_evidence/`](file:///Users/harshitrajsingh/Nexus/data/agntid_evidence/) | **Pending official credentials/API contract** | No official AgntID SDK or gateway was installed on the local test machine; decisions are recorded honestly as `NOT_EVALUATED`. All current decisions in local demos are NEXUS decisions, not AgntID decisions. |
| **Enforcement** | [`agent_orchestrator/agntid_bridge.py`](file:///Users/harshitrajsingh/Nexus/agent_orchestrator/agntid_bridge.py) | **Pending official credentials/API contract** | NEXUS policy is advisory when AgntID is unavailable. `enforced` is strictly `false` because no live interception daemon is attached. |
| **Reproducibility** | [`scripts/judge_preflight.sh`](file:///Users/harshitrajsingh/Nexus/scripts/judge_preflight.sh)<br>[`scripts/judge_demo.sh`](file:///Users/harshitrajsingh/Nexus/scripts/judge_demo.sh)<br>[`scripts/validate_agntid_live.sh`](file:///Users/harshitrajsingh/Nexus/scripts/validate_agntid_live.sh) | **100% Turnkey Repro** | Single-command preflight, demo, and live validation scripts verify the entire project without manual setup steps. |

---

## 2. Integrity Guarantees & Defense Summary

1. **No Fabricated Decisions:**  
   When official AgntID is absent, the system emits `NOT_EVALUATED` (`source: "unavailable"`). It never simulates fake AgntID blocks or pretends local heuristic scores came from AgntID.
2. **Separation of Concerns:**  
   Model predictions, deterministic evidence scores, policy recommendations, and runtime enforcement status occupy separate, non-overlapping JSON fields.
3. **Mock Egress Safety:**  
   All network actions are directed to `mock://` URIs or loopback addresses. No real credentials or live corporate endpoints are ever touched.
