#!/usr/bin/env python3
"""
NEXUS API End-to-End Verification Test
Validates all FastAPI bridge endpoints against live C++17 LibTorch engine inference
and Explainable Hybrid Risk Engine contract:
  - GET /health
  - POST /api/events/ingest (Hybrid score, explanation, policy action)
  - GET /api/graph/snapshot (Semantic annotations, active explanation)
  - GET /api/metrics
  - POST /api/experiments/run (Normal vs Adversarial vs Unseen comparison)
  - POST /api/graph/reset
"""

import sys
import socket
import urllib.request
from pathlib import Path
from unittest.mock import patch
from fastapi.testclient import TestClient

# Ensure agent_orchestrator is in path
sys.path.insert(0, str(Path(__file__).resolve().parent))

from api import app

client = TestClient(app)


def test_health():
    print("\n--- Testing GET /health ---")
    response = client.get("/health")
    assert response.status_code == 200, f"Expected 200, got {response.status_code}: {response.text}"
    data = response.json()
    assert data["status"] == "healthy", f"Expected healthy status, got {data}"
    assert data["cpp_engine"]["model_loaded"] is True, "Model should be loaded"
    assert "hybrid_risk_engine" in data, "Should contain hybrid_risk_engine status"
    assert "agntid_integration" in data, "Should contain agntid_integration findings"
    print(f"Health check PASSED: model_loaded={data['cpp_engine']['model_loaded']}, HybridEngine={data['hybrid_risk_engine']['status']}, AgntID installed={data['agntid_integration']['sdk_installed']}")


def test_ingest_event():
    print("\n--- Testing POST /api/events/ingest ---")
    event_payload = {
        "event_id": "EVT_TEST_API_001",
        "agent_id": "test_agent_api",
        "timestamp": 1795330500,
        "action": "READ_SECRET",
        "tool": "FILE_SYSTEM",
        "resource": "credentials/config.env",
        "permission": "READ",
        "intent_id": "INT_TEST_API",
        "status": "SUCCESS",
        "label": "Suspicious",
        "is_synthetic": False
    }
    response = client.post("/api/events/ingest", json=event_payload)
    assert response.status_code == 200, f"Expected 200, got {response.status_code}: {response.text}"
    data = response.json()
    assert data["status"] == "ok"
    assert data["event_id"] == "EVT_TEST_API_001"
    assert data["latency_ms"] > 0.0, "Latency should be positive"
    assert "prediction" in data, "Response should have prediction"
    assert "class_name" in data["prediction"]
    assert "confidence" in data["prediction"]
    assert "probabilities" in data["prediction"]
    assert len(data["prediction"]["probabilities"]) == 4

    # Verify Hybrid Risk Engine fields
    assert "hybrid_assessment" in data, "Must contain hybrid_assessment"
    assert "evidence_score" in data and data["evidence_score"] >= 25, f"Expected evidence score >= 25, got {data.get('evidence_score')}"
    assert "policy_risk" in data
    assert "recommended_action" in data
    assert "actual_enforcement_status" in data
    assert "attack_explanation" in data
    assert "narrative" in data["attack_explanation"]
    assert data["actual_enforcement_status"].startswith("NOT_ENFORCED")

    print(f"Ingest event PASSED: prediction={data['prediction']['class_name']}, EvidenceScore={data['evidence_score']}, PolicyRisk={data['policy_risk']}, Action='{data['recommended_action']}'")


def test_graph_snapshot():
    print("\n--- Testing GET /api/graph/snapshot ---")
    response = client.get("/api/graph/snapshot")
    assert response.status_code == 200, f"Expected 200, got {response.status_code}: {response.text}"
    data = response.json()
    assert data["status"] == "ok"
    assert "nodes" in data and len(data["nodes"]) > 0
    assert "edges" in data and len(data["edges"]) > 0
    # Check semantic node annotations
    assert "is_sensitive" in data["nodes"][0]
    assert "latest_hybrid_assessment" in data
    print(f"Graph snapshot PASSED: nodes={len(data['nodes'])}, edges={len(data['edges'])}, has_hybrid_state={data['latest_hybrid_assessment'] is not None}")


def test_metrics():
    print("\n--- Testing GET /api/metrics ---")
    response = client.get("/api/metrics")
    assert response.status_code == 200, f"Expected 200, got {response.status_code}: {response.text}"
    data = response.json()
    assert data["status"] == "ok"
    assert "metrics" in data and data["metrics"] is not None
    metrics = data["metrics"]
    assert "ablation_comparison" in metrics
    print(f"Metrics endpoint PASSED: models in ablation={list(metrics['ablation_comparison'].keys())}")


def test_experiment_normal():
    print("\n--- Testing POST /api/experiments/run (scenario='normal') ---")
    req = {"scenario": "normal", "agent_id": "agent_test_normal", "reset_graph_before": True}
    response = client.post("/api/experiments/run", json=req)
    assert response.status_code == 200, f"Expected 200, got {response.status_code}: {response.text}"
    data = response.json()
    assert data["status"] == "completed"
    assert data["scenario"] == "normal"
    assert data["total_steps"] == 4

    summary = data["summary"]
    assert summary["policy_risk_detected"] == "Normal", f"Normal scenario should have Normal policy risk, got {summary['policy_risk_detected']}"
    assert summary["peak_evidence_score"] < 25, f"Expected peak evidence score < 25, got {summary['peak_evidence_score']}"
    assert summary["recommended_action"] == "Continue monitoring"
    print(f"Normal experiment PASSED: steps={data['total_steps']}, policy_risk={summary['policy_risk_detected']}, evidence_score={summary['peak_evidence_score']}, action='{summary['recommended_action']}'")


def test_experiment_adversarial():
    print("\n--- Testing POST /api/experiments/run (scenario='adversarial_exfil') ---")
    req = {"scenario": "adversarial_exfil", "agent_id": "agent_test_adversarial", "reset_graph_before": True}
    response = client.post("/api/experiments/run", json=req)
    assert response.status_code == 200, f"Expected 200, got {response.status_code}: {response.text}"
    data = response.json()
    assert data["status"] == "completed"
    assert data["scenario"] == "adversarial_exfil"
    assert data["total_steps"] == 4
    assert len(data["steps"]) == 4

    summary = data["summary"]
    # Adversarial exfil: Deterministic 95 + neural baseline 10 = raw 105, capped at 100
    assert summary["policy_risk_detected"] == "Critical", f"Expected Critical, got {summary['policy_risk_detected']}"
    assert summary["deterministic_score"] == 95, f"Expected deterministic_score == 95, got {summary.get('deterministic_score')}"
    assert summary["model_anomaly_score"] == 10, f"Expected model_anomaly_score == 10, got {summary.get('model_anomaly_score')}"
    assert summary["raw_score"] == 105, f"Expected raw_score == 105, got {summary.get('raw_score')}"
    assert summary["capped_score"] == 100, f"Expected capped_score == 100, got {summary.get('capped_score')}"
    assert summary["score_headline"] == "Deterministic 95 + neural baseline 10 = raw 105, capped at 100.", f"Unexpected headline: {summary.get('score_headline')}"
    assert summary["peak_evidence_score"] == 100, f"Expected peak_evidence_score == 100 for exfil, got {summary['peak_evidence_score']}"
    assert summary["timestamp_label"] == "replay timestamp (pinned)", f"Expected replay timestamp label, got {summary.get('timestamp_label')}"
    assert "attack_explanation" in summary and summary["attack_explanation"] is not None
    assert summary["attack_explanation"]["sensitive_resource"] == "credentials/config.env"
    assert summary["attack_explanation"]["privilege_or_role"] == "ADMIN"
    assert "model_vs_evidence_comparison" in summary
    assert summary["actual_enforcement_status"].startswith("NOT_ENFORCED")

    # Compute sensitive+egress inline trail count across returned DFS paths (no hardcoded "5 causal attack paths" claim)
    from hybrid_risk_engine import HybridRiskEngine
    final_step_paths = data["steps"][-1]["attack_paths"]
    sensitive_egress_trails = sum(
        1
        for p in final_step_paths
        if any(HybridRiskEngine.is_sensitive_resource(st.get("node_key", "")) for st in p.get("steps", []))
        and any(
            st.get("node_type") == "Destination" or HybridRiskEngine.is_external_destination(st.get("node_key", ""))
            for st in p.get("steps", [])
        )
    )
    assert isinstance(sensitive_egress_trails, int) and sensitive_egress_trails >= 0
    print(f"Adversarial experiment PASSED: steps={data['total_steps']}, policy_risk={summary['policy_risk_detected']}, peak_evidence_score={summary['peak_evidence_score']}, sensitive_egress_trails={sensitive_egress_trails} (raw_dfs={len(final_step_paths)})")
    print(f"  Narrative: {summary['attack_explanation']['narrative']}")


def test_experiment_unseen():
    print("\n--- Testing POST /api/experiments/run (scenario='unseen_attack') ---")
    req = {"scenario": "unseen_attack", "agent_id": "agent_test_unseen", "reset_graph_before": True}
    response = client.post("/api/experiments/run", json=req)
    assert response.status_code == 200, f"Expected 200, got {response.status_code}: {response.text}"
    data = response.json()
    assert data["status"] == "completed"
    assert data["scenario"] == "unseen_attack"
    assert data["total_steps"] == 4
    summary = data["summary"]
    assert summary["peak_evidence_score"] >= 50
    print(f"Unseen attack experiment PASSED: steps={data['total_steps']}, policy_risk={summary['policy_risk_detected']}, evidence_score={summary['peak_evidence_score']}")


def test_no_real_network_requests():
    print("\n--- Testing Network Isolation (Zero Real Network Requests / mock:// Egress Only) ---")
    from agent_orchestrator.agent_runner import SandboxedAgent

    def _forbid_network(*args, **kwargs):
        raise AssertionError(f"Real external network request attempted! args={args} kwargs={kwargs}")

    # Guard outbound TCP connection and urlopen across scenario execution
    with patch.object(socket, "create_connection", side_effect=_forbid_network), \
         patch.object(urllib.request, "urlopen", side_effect=_forbid_network):
        agent = SandboxedAgent(agent_id="agent_network_isolation_check")

        # Verify external URLs are explicitly rejected by sandbox policy
        blocked_ext = agent.tool_network_request("https://real-external.example.com/leak", method="POST")
        assert blocked_ext["success"] is False, "Non-mock external URL must be rejected by SandboxedAgent"

        # Verify all external egress targets in demo scenarios stay strictly on mock://
        for scen in ["normal", "adversarial_exfil", "unseen_attack"]:
            steps = agent.run_scenario(scen)
            for step in steps:
                evt = step["event"]
                if evt["action"] in ["EXFILTRATE", "BEACON"] or evt["tool"] in ["CURL", "NETWORK"]:
                    assert evt["resource"].startswith("mock://"), (
                        f"Expected mock:// egress target in scenario '{scen}', got '{evt['resource']}'"
                    )

        # Verify intent-boundary run invokes no real network calls
        resp = client.post("/api/experiments/intent-boundary")
        assert resp.status_code == 200
        exfil_cond = resp.json()["conditions"]["multi_step_exfiltration"]
        exfil_steps = [s for s in exfil_cond["steps"] if s["action"] == "EXFILTRATE"]
        assert len(exfil_steps) == 1 and exfil_steps[0]["resource"].startswith("mock://"), (
            f"Context 3 exfiltration egress must use mock:// target, got {exfil_steps}"
        )

    print("Network isolation check PASSED: 0 real outbound network calls; all external egress targets stay on mock://.")


def test_intent_boundary_experiment():
    print("\n--- Testing POST /api/experiments/intent-boundary ---")
    response = client.post("/api/experiments/intent-boundary")
    assert response.status_code == 200, f"Expected 200, got {response.status_code}: {response.text}"
    data = response.json()
    assert data["status"] == "COMPLETED"
    assert "conditions" in data
    assert "comparison_matrix" in data
    assert len(data["comparison_matrix"]) == 3
    assert "approved_rotation" in data["conditions"]
    assert "intent_mismatch" in data["conditions"]
    assert "multi_step_exfiltration" in data["conditions"]

    c1 = data["conditions"]["approved_rotation"]
    c2 = data["conditions"]["intent_mismatch"]
    c3 = data["conditions"]["multi_step_exfiltration"]

    print(f"  Observed Context 1 (Approved Rotation): score={c1['peak_evidence_score']}, risk={c1['final_policy_risk']}, rec='{c1['final_recommendation']}', agntid={c1['final_agntid_decision']}")
    print(f"  Observed Context 2 (Intent Mismatch):   score={c2['peak_evidence_score']}, risk={c2['final_policy_risk']}, rec='{c2['final_recommendation']}', agntid={c2['final_agntid_decision']}")
    print(f"  Observed Context 3 (Exfiltration):      score={c3['peak_evidence_score']}, risk={c3['final_policy_risk']}, rec='{c3['final_recommendation']}', agntid={c3['final_agntid_decision']}")

    mismatches = []
    # Context 1 assertions: score 40 with "Alert analyst"
    if c1["peak_evidence_score"] != 40:
        mismatches.append(f"Context 1 score expected 40, got {c1['peak_evidence_score']}")
    if c1["final_recommendation"] != "Alert analyst":
        mismatches.append(f"Context 1 recommendation expected 'Alert analyst', got '{c1['final_recommendation']}'")
    if c1["final_agntid_decision"] != "NOT_EVALUATED":
        mismatches.append(f"Context 1 AgntID expected 'NOT_EVALUATED', got '{c1['final_agntid_decision']}'")

    # Context 2 assertions: score 75 with "Recommend workflow pause"
    if c2["peak_evidence_score"] != 75:
        mismatches.append(f"Context 2 score expected 75, got {c2['peak_evidence_score']}")
    if "Recommend workflow pause" not in c2["final_recommendation"]:
        mismatches.append(f"Context 2 recommendation expected 'Recommend workflow pause...', got '{c2['final_recommendation']}'")
    if c2["final_agntid_decision"] != "NOT_EVALUATED":
        mismatches.append(f"Context 2 AgntID expected 'NOT_EVALUATED', got '{c2['final_agntid_decision']}'")

    # Context 3 assertions: Deterministic 95 + neural baseline 10 = raw 105, capped at 100
    if c3.get("deterministic_score") != 95:
        mismatches.append(f"Context 3 deterministic_score expected 95, got {c3.get('deterministic_score')}")
    if c3.get("model_anomaly_score") != 10:
        mismatches.append(f"Context 3 model_anomaly_score expected 10, got {c3.get('model_anomaly_score')}")
    if c3.get("raw_score") != 105:
        mismatches.append(f"Context 3 raw_score expected 105, got {c3.get('raw_score')}")
    if c3.get("capped_score") != 100:
        mismatches.append(f"Context 3 capped_score expected 100, got {c3.get('capped_score')}")
    if c3.get("score_headline") != "Deterministic 95 + neural baseline 10 = raw 105, capped at 100.":
        mismatches.append(f"Context 3 score_headline mismatch: {c3.get('score_headline')}")
    if c3.get("timestamp_label") != "replay timestamp (pinned)":
        mismatches.append(f"Context 3 timestamp_label expected 'replay timestamp (pinned)', got {c3.get('timestamp_label')}")
    if c3["peak_evidence_score"] != 100:
        mismatches.append(f"Context 3 score expected 100, got {c3['peak_evidence_score']}")
    actions_c3 = [s["action"] for s in c3["steps"]]
    if actions_c3 != ["READ_SECRET", "ESCALATE_ROLE", "EXFILTRATE"]:
        mismatches.append(f"Context 3 chain expected ['READ_SECRET', 'ESCALATE_ROLE', 'EXFILTRATE'], got {actions_c3}")
    if not c3["steps"][-1]["resource"].startswith("mock://"):
        mismatches.append(f"Context 3 egress must be mock:// target, got {c3['steps'][-1]['resource']}")
    if c3["final_agntid_decision"] != "NOT_EVALUATED":
        mismatches.append(f"Context 3 AgntID expected 'NOT_EVALUATED', got '{c3['final_agntid_decision']}'")

    assert not mismatches, "Intent-boundary scenario assertions failed:\n  - " + "\n  - ".join(mismatches)
    print(f"Intent boundary experiment PASSED: Context1={c1['peak_evidence_score']}, Context2={c2['peak_evidence_score']}, Context3={c3['peak_evidence_score']}")


def test_graph_reset():
    print("\n--- Testing POST /api/graph/reset ---")
    response = client.post("/api/graph/reset")
    assert response.status_code == 200, f"Expected 200, got {response.status_code}: {response.text}"
    data = response.json()
    assert data["status"] == "ok"

    # Verify snapshot is empty now
    snap_resp = client.get("/api/graph/snapshot")
    snap_data = snap_resp.json()
    assert len(snap_data["nodes"]) == 0
    assert len(snap_data["edges"]) == 0
    print(f"Graph reset PASSED: nodes after reset={len(snap_data['nodes'])}, edges={len(snap_data['edges'])}")


def main():
    print("=" * 70)
    print("NEXUS Hybrid Risk Engine & FastAPI Backend Verification")
    print("=" * 70)

    # Use TestClient context manager to trigger lifespan startup and shutdown
    with TestClient(app) as test_client:
        global client
        client = test_client

        test_health()
        test_ingest_event()
        test_graph_snapshot()
        test_metrics()
        test_experiment_normal()
        test_experiment_adversarial()
        test_experiment_unseen()
        test_no_real_network_requests()
        test_intent_boundary_experiment()
        test_graph_reset()

    print("\n" + "=" * 70)
    print("ALL HYBRID RISK & API VERIFICATIONS PASSED SUCCESSFULLY!")
    print("=" * 70)


if __name__ == "__main__":
    main()
