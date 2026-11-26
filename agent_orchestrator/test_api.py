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
from pathlib import Path
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
    # Adversarial exfil accesses config.env (+25), admin (+25), exfil (+35), full chain (+10), neural (+10) => Score >= 95
    assert summary["policy_risk_detected"] in ["HighRisk", "Critical"], f"Expected HighRisk or Critical, got {summary['policy_risk_detected']}"
    assert summary["peak_evidence_score"] >= 75, f"Expected evidence score >= 75 for exfil, got {summary['peak_evidence_score']}"
    assert "attack_explanation" in summary and summary["attack_explanation"] is not None
    assert summary["attack_explanation"]["sensitive_resource"] == "credentials/config.env"
    assert summary["attack_explanation"]["privilege_or_role"] == "ADMIN"
    assert "model_vs_evidence_comparison" in summary
    assert summary["actual_enforcement_status"].startswith("NOT_ENFORCED")
    print(f"Adversarial experiment PASSED: steps={data['total_steps']}, policy_risk={summary['policy_risk_detected']}, peak_evidence_score={summary['peak_evidence_score']}")
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
    assert "suspicious_exfiltration" in data["conditions"]
    assert "context_conflict" in data["conditions"]

    # Verify identical resource distinction:
    # Rotation intent vs Exfiltration intent on credentials/config.env
    cond_rot = data["conditions"]["approved_rotation"]
    cond_exfil = data["conditions"]["suspicious_exfiltration"]
    assert cond_rot["peak_evidence_score"] < cond_exfil["peak_evidence_score"]
    assert cond_exfil["final_policy_risk"] in ["HighRisk", "Critical"]
    print(f"Intent boundary experiment PASSED: Rotation Score={cond_rot['peak_evidence_score']}/100, Exfil Score={cond_exfil['peak_evidence_score']}/100")


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
        test_intent_boundary_experiment()
        test_graph_reset()

    print("\n" + "=" * 70)
    print("ALL HYBRID RISK & API VERIFICATIONS PASSED SUCCESSFULLY!")
    print("=" * 70)


if __name__ == "__main__":
    main()
