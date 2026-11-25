#!/usr/bin/env python3
"""
NEXUS Security Intelligence Platform - FastAPI Backend Bridge
Exposes C++17 LibTorch DynamicGraphEngine, TemporalRiskModel, and AttackPathAnalyzer
inference alongside AgntID runtime security evaluation, Explainable Hybrid Risk Engine,
and sandboxed competition experiments.
"""

import os
import sys
import json
import time
from contextlib import asynccontextmanager
from pathlib import Path
from typing import Dict, Any, Optional, List

# Ensure both orchestrator dir and repo root are in sys.path
_CURRENT_DIR = Path(__file__).resolve().parent
_REPO_ROOT = _CURRENT_DIR.parent
for _p in [str(_CURRENT_DIR), str(_REPO_ROOT)]:
    if _p not in sys.path:
        sys.path.insert(0, _p)

from fastapi import FastAPI, HTTPException, status
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field

try:
    from agent_orchestrator.agntid_bridge import AgntIDBridge, inspect_agntid_environment
    from agent_orchestrator.agent_runner import SandboxedAgent
    from agent_orchestrator.cpp_bridge import NexusCppBridge
    from agent_orchestrator.hybrid_risk_engine import HybridRiskEngine
except ImportError:
    from agntid_bridge import AgntIDBridge, inspect_agntid_environment
    from agent_runner import SandboxedAgent
    from cpp_bridge import NexusCppBridge
    from hybrid_risk_engine import HybridRiskEngine

# Global bridge singletons
cpp_engine: Optional[NexusCppBridge] = None
agntid_bridge: Optional[AgntIDBridge] = None
hybrid_engine = HybridRiskEngine()

# Session memory of ingested telemetry events
ingested_events: List[Dict[str, Any]] = []
latest_hybrid_result: Optional[Dict[str, Any]] = None

DATA_DIR = Path(__file__).resolve().parent.parent / "data"


@asynccontextmanager
async def lifespan(app: FastAPI):
    global cpp_engine, agntid_bridge
    # Startup: initialize C++ engine bridge and AgntID bridge
    cpp_engine = NexusCppBridge()
    agntid_bridge = AgntIDBridge(agent_id="nexus_core_api")
    yield
    # Shutdown: clean up child subprocess
    if cpp_engine:
        cpp_engine.close()


app = FastAPI(
    title="NEXUS Security Intelligence API",
    description="Real-time Agentic Attack Vector Detection via C++17 Dynamic Graph Engine & R-GCN + GRU LibTorch Inference with Hybrid Evidence Scoring",
    version="1.1.0",
    lifespan=lifespan
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


# -----------------------------------------------------------------------------
# Request & Response Schemas
# -----------------------------------------------------------------------------

class TelemetryEventIn(BaseModel):
    event_id: str = Field(..., description="Unique event identifier")
    agent_id: str = Field(..., description="Agent performing the action")
    timestamp: int = Field(..., description="Epoch timestamp in seconds")
    action: str = Field(..., description="Concrete action (e.g. READ_SECRET, SQL_QUERY)")
    tool: str = Field(..., description="Tool used (e.g. FILE_SYSTEM, CURL, IAM)")
    resource: str = Field(default="NONE", description="Target resource")
    permission: str = Field(default="NONE", description="Permission level requested")
    intent_id: str = Field(default="", description="High-level task intent identifier")
    status: str = Field(default="SUCCESS", description="Execution outcome")
    label: str = Field(default="Normal", description="Telemetry classification label")
    is_synthetic: bool = Field(default=False, description="True for synthetic generation, False for live execution")


class ExperimentRunRequest(BaseModel):
    scenario: str = Field(..., description="Scenario to execute: 'normal', 'adversarial_exfil', or 'unseen_attack'")
    agent_id: Optional[str] = Field(default=None, description="Optional custom agent identifier")
    reset_graph_before: bool = Field(default=True, description="Reset graph state prior to execution")


# -----------------------------------------------------------------------------
# Endpoints
# -----------------------------------------------------------------------------

@app.get("/health", summary="System Health & Diagnostic Status")
def get_health() -> Dict[str, Any]:
    """Returns engine connectivity, model status, hybrid engine, and AgntID environment diagnostic."""
    if not cpp_engine:
        raise HTTPException(status_code=status.HTTP_503_SERVICE_UNAVAILABLE, detail="C++ Engine not initialized")

    try:
        engine_health = cpp_engine.health()
    except Exception as e:
        engine_health = {"status": "error", "error": str(e)}

    agntid_info = inspect_agntid_environment()

    return {
        "status": "healthy" if engine_health.get("status") == "ok" else "degraded",
        "timestamp": int(time.time()),
        "cpp_engine": engine_health,
        "hybrid_risk_engine": {
            "status": "active",
            "version": "1.1.0",
            "scoring_weights": {
                "credential_access": 25,
                "privilege_escalation": 25,
                "external_destination": 35,
                "full_chain_correlation": 10,
                "neural_model_suspicious": 10
            }
        },
        "agntid_integration": agntid_info
    }


@app.post("/api/events/ingest", summary="Ingest TelemetryEvent for Real-Time Inference & Hybrid Risk Evaluation")
def ingest_event(event: TelemetryEventIn) -> Dict[str, Any]:
    """
    Ingests a TelemetryEvent:
      1. Updates DynamicGraphEngine in C++.
      2. Executes LibTorch TemporalRiskModel forward pass.
      3. Reconstructs top attack paths via AttackPathAnalyzer.
      4. Queries AgntID bridge independently.
      5. Computes Explainable Hybrid Risk Score & Policy Recommendation.
    """
    global latest_hybrid_result
    if not cpp_engine:
        raise HTTPException(status_code=status.HTTP_503_SERVICE_UNAVAILABLE, detail="C++ Engine not initialized")

    event_dict = event.model_dump()
    ingested_events.append(event_dict)

    # 1. Native C++ LibTorch inference
    try:
        cpp_result = cpp_engine.ingest_event(event_dict)
    except Exception as e:
        raise HTTPException(status_code=status.HTTP_500_INTERNAL_SERVER_ERROR, detail=f"C++ Engine error: {e}")

    # 2. Independent AgntID runtime evaluation
    agntid_res = agntid_bridge.evaluate_event(event_dict) if agntid_bridge else {
        "agntid_available": False,
        "decision": "NOT_EVALUATED",
        "enforced": False
    }

    # 3. Explainable Hybrid Risk Assessment
    hybrid_assessment = hybrid_engine.evaluate(
        current_event=event_dict,
        history=ingested_events,
        model_prediction=cpp_result.get("prediction", {}),
        attack_paths=cpp_result.get("attack_paths", []),
        agntid_decision=agntid_res
    )
    latest_hybrid_result = hybrid_assessment

    return {
        "status": "ok",
        "event_id": event.event_id,
        "latency_ms": cpp_result.get("latency_ms", 0.0),
        "prediction": cpp_result.get("prediction", {}),
        "attack_paths": cpp_result.get("attack_paths", []),
        "agntid_decision": agntid_res,
        "graph_stats": cpp_result.get("graph_stats", {}),
        # Upgraded Hybrid Risk Contract
        "hybrid_assessment": hybrid_assessment,
        "evidence_score": hybrid_assessment["evidence_score"],
        "evidence_signals": hybrid_assessment["evidence_signals"],
        "policy_risk": hybrid_assessment["policy_risk"],
        "recommended_action": hybrid_assessment["recommended_action"],
        "actual_enforcement_status": hybrid_assessment["actual_enforcement_status"],
        "attack_explanation": hybrid_assessment["attack_explanation"]
    }


@app.get("/api/graph/snapshot", summary="Get Current Dynamic Graph Snapshot with Hybrid Security Annotations")
def get_graph_snapshot() -> Dict[str, Any]:
    """Returns nodes, edges, active highlighted attack path, and semantic risk annotations."""
    if not cpp_engine:
        raise HTTPException(status_code=status.HTTP_503_SERVICE_UNAVAILABLE, detail="C++ Engine not initialized")

    try:
        snapshot = cpp_engine.snapshot()
    except Exception as e:
        raise HTTPException(status_code=status.HTTP_500_INTERNAL_SERVER_ERROR, detail=f"C++ Engine snapshot error: {e}")

    # Enrich nodes with semantic flags for the Live Graph visualizer
    annotated_nodes = []
    for node in snapshot.get("nodes", []):
        key = node.get("key", "")
        n_copy = dict(node)
        n_copy["is_sensitive"] = HybridRiskEngine.is_sensitive_resource(key)
        n_copy["is_elevated"] = HybridRiskEngine.is_privileged_action(key)
        n_copy["is_external"] = HybridRiskEngine.is_external_destination(key)
        annotated_nodes.append(n_copy)

    snapshot["nodes"] = annotated_nodes
    snapshot["latest_hybrid_assessment"] = latest_hybrid_result
    snapshot["active_attack_explanation"] = latest_hybrid_result.get("attack_explanation") if latest_hybrid_result else None

    return snapshot


@app.post("/api/graph/reset", summary="Reset Graph, Memory, and Hybrid Risk State")
def reset_graph() -> Dict[str, Any]:
    """Resets the internal graph state, temporal sequence memory, and event history."""
    global latest_hybrid_result
    if not cpp_engine:
        raise HTTPException(status_code=status.HTTP_503_SERVICE_UNAVAILABLE, detail="C++ Engine not initialized")

    ingested_events.clear()
    latest_hybrid_result = None

    try:
        return cpp_engine.reset()
    except Exception as e:
        raise HTTPException(status_code=status.HTTP_500_INTERNAL_SERVER_ERROR, detail=f"C++ Engine reset error: {e}")


@app.get("/api/metrics", summary="Retrieve Training & Evaluation Metrics")
def get_metrics() -> Dict[str, Any]:
    """Returns honest metrics recorded during Day 5 training and ablation pipeline."""
    metrics_path = DATA_DIR / "metrics.json"
    manifest_path = DATA_DIR / "dataset_manifest.json"

    result: Dict[str, Any] = {"status": "ok", "metrics": None, "manifest": None}

    if metrics_path.exists():
        try:
            result["metrics"] = json.loads(metrics_path.read_text())
        except Exception as e:
            result["metrics_error"] = str(e)

    if manifest_path.exists():
        try:
            result["manifest"] = json.loads(manifest_path.read_text())
        except Exception as e:
            result["manifest_error"] = str(e)

    if result["metrics"] is None and result["manifest"] is None:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Metrics files not found in data directory")

    return result


@app.post("/api/experiments/run", summary="Run Sandboxed Agent Experiment Scenario with Hybrid Evaluation")
def run_experiment(req: ExperimentRunRequest) -> Dict[str, Any]:
    """
    Executes a named scenario (normal, adversarial_exfil, unseen_attack)
    through the sandboxed tool agent and streams each step through NEXUS C++ inference,
    AgntID evaluation, and the Explainable Hybrid Risk Engine.
    """
    global latest_hybrid_result
    if not cpp_engine:
        raise HTTPException(status_code=status.HTTP_503_SERVICE_UNAVAILABLE, detail="C++ Engine not initialized")

    valid_scenarios = ["normal", "adversarial_exfil", "unseen_attack"]
    if req.scenario not in valid_scenarios:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"Invalid scenario '{req.scenario}'. Permitted: {valid_scenarios}"
        )

    if req.reset_graph_before:
        cpp_engine.reset()
        ingested_events.clear()

    agent_id = req.agent_id or f"agent_exp_{req.scenario}"
    sandboxed_agent = SandboxedAgent(agent_id=agent_id)

    # Execute scenario steps in isolated sandbox
    scenario_steps = sandboxed_agent.run_scenario(req.scenario)

    processed_events = []
    risk_rank = {"Normal": 0, "Suspicious": 1, "HighRisk": 2, "Critical": 3}
    highest_model_risk = "Normal"
    highest_policy_risk = "Normal"
    peak_evidence_score = 0
    final_hybrid = None

    history_so_far: List[Dict[str, Any]] = []

    for step_record in scenario_steps:
        evt = step_record["event"]
        history_so_far.append(evt)
        ingested_events.append(evt)
        agnt_dec = step_record["agntid_decision"]

        # Run through C++ LibTorch inference
        cpp_res = cpp_engine.ingest_event(evt)
        pred = cpp_res.get("prediction", {})
        pred_class = pred.get("class_name", "Normal")

        if risk_rank.get(pred_class, 0) > risk_rank.get(highest_model_risk, 0):
            highest_model_risk = pred_class

        # Run through Hybrid Risk Engine
        hybrid_step = hybrid_engine.evaluate(
            current_event=evt,
            history=history_so_far,
            model_prediction=pred,
            attack_paths=cpp_res.get("attack_paths", []),
            agntid_decision=agnt_dec
        )
        final_hybrid = hybrid_step

        if hybrid_step["evidence_score"] > peak_evidence_score:
            peak_evidence_score = hybrid_step["evidence_score"]

        if risk_rank.get(hybrid_step["policy_risk"], 0) > risk_rank.get(highest_policy_risk, 0):
            highest_policy_risk = hybrid_step["policy_risk"]

        processed_events.append({
            "event": evt,
            "tool_result": step_record["tool_result"],
            "nexus_prediction": pred,
            "latency_ms": cpp_res.get("latency_ms", 0.0),
            "attack_paths": cpp_res.get("attack_paths", []),
            "agntid_decision": agnt_dec,
            # Hybrid risk additions per step
            "hybrid_assessment": hybrid_step,
            "evidence_score": hybrid_step["evidence_score"],
            "evidence_signals": hybrid_step["evidence_signals"],
            "policy_risk": hybrid_step["policy_risk"],
            "recommended_action": hybrid_step["recommended_action"]
        })

    latest_hybrid_result = final_hybrid

    # Get final snapshot
    final_snapshot = cpp_engine.snapshot()

    return {
        "status": "completed",
        "scenario": req.scenario,
        "agent_id": agent_id,
        "total_steps": len(processed_events),
        "steps": processed_events,
        "summary": {
            # Backward-compatible fields
            "highest_risk_detected": highest_model_risk,
            "detected_attack_paths_count": len(final_snapshot.get("top_paths", [])),
            "active_attack_path": final_snapshot.get("active_attack_path"),
            "agntid_enforcement_count": 0,
            "agntid_available": inspect_agntid_environment()["enforcement_active"],
            # Upgraded Hybrid Hackathon Judging Contract
            "policy_risk_detected": highest_policy_risk,
            "peak_evidence_score": peak_evidence_score,
            "recommended_action": final_hybrid["recommended_action"] if final_hybrid else "Continue monitoring",
            "actual_enforcement_status": final_hybrid["actual_enforcement_status"] if final_hybrid else "NOT_ENFORCED (No active blocking engine attached; policy is advisory)",
            "attack_explanation": final_hybrid.get("attack_explanation") if final_hybrid else None,
            "model_vs_evidence_comparison": {
                "neural_model_prediction": highest_model_risk,
                "deterministic_evidence_score": peak_evidence_score,
                "policy_risk": highest_policy_risk,
                "recommended_action": final_hybrid["recommended_action"] if final_hybrid else "Continue monitoring",
                "actual_enforcement_status": "NOT_ENFORCED (No active blocking engine attached; policy is advisory)"
            }
        }
    }


if __name__ == "__main__":
    import uvicorn
    uvicorn.run("api:app", host="0.0.0.0", port=8000, reload=False)
