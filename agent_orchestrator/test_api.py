#!/usr/bin/env python3
"""
NEXUS API End-to-End Verification Test
Validates all FastAPI bridge endpoints against live C++17 LibTorch engine inference:
  - GET /health
  - POST /api/events/ingest
  - GET /api/graph/snapshot
  - GET /api/metrics
  - POST /api/experiments/run
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
    assert "agntid_integration" in data, "Should contain agntid_integration findings"
    print(f"Health check PASSED: model_loaded={data['cpp_engine']['model_loaded']}, AgntID installed={data['agntid_integration']['sdk_installed']}")


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
    assert "agntid_decision" in data, "Response should include independent AgntID decision"
    assert data["agntid_decision"]["decision"] == "NOT_EVALUATED"
    print(f"Ingest event PASSED: prediction={data['prediction']['class_name']} (conf={data['prediction']['confidence']:.3f}), latency={data['latency_ms']:.2f}ms, AgntID={data['agntid_decision']['decision']}")


def test_graph_snapshot():
    print("\n--- Testing GET /api/graph/snapshot ---")
    response = client.get("/api/graph/snapshot")
    assert response.status_code == 200, f"Expected 200, got {response.status_code}: {response.text}"
    data = response.json()
    assert data["status"] == "ok"
    assert "nodes" in data and len(data["nodes"]) > 0
    assert "edges" in data and len(data["edges"]) > 0
    print(f"Graph snapshot PASSED: nodes={len(data['nodes'])}, edges={len(data['edges'])}, top_paths={len(data.get('top_paths', []))}")


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
    print(f"Normal experiment PASSED: total_steps={data['total_steps']}, highest_risk={data['summary']['highest_risk_detected']}")


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
    # Check that attack paths or elevated risk were captured
    assert data["summary"]["detected_attack_paths_count"] > 0 or data["summary"]["highest_risk_detected"] != "Normal"
    print(f"Adversarial experiment PASSED: steps={data['total_steps']}, highest_risk={data['summary']['highest_risk_detected']}, attack_paths={data['summary']['detected_attack_paths_count']}")


def test_experiment_unseen():
    print("\n--- Testing POST /api/experiments/run (scenario='unseen_attack') ---")
    req = {"scenario": "unseen_attack", "agent_id": "agent_test_unseen", "reset_graph_before": True}
    response = client.post("/api/experiments/run", json=req)
    assert response.status_code == 200, f"Expected 200, got {response.status_code}: {response.text}"
    data = response.json()
    assert data["status"] == "completed"
    assert data["scenario"] == "unseen_attack"
    assert data["total_steps"] == 4
    print(f"Unseen attack experiment PASSED: steps={data['total_steps']}, highest_risk={data['summary']['highest_risk_detected']}")


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
    print("NEXUS Day 6 End-to-End FastAPI Backend & C++ Engine Bridge Verification")
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
        test_graph_reset()

    print("\n" + "=" * 70)
    print("ALL DAY 6 ENDPOINTS & VERIFICATIONS PASSED SUCCESSFULLY!")
    print("=" * 70)


if __name__ == "__main__":
    main()
