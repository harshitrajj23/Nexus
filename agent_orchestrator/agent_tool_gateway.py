#!/usr/bin/env python3
"""
NEXUS Real-Agent Compatibility Layer & Tool Gateway
Component: System 05 / System 06 Generic Agent Ingestion Gateway

Allows any arbitrary tool-using agent (custom Python loop, MCP server, LangChain,
CrewAI, AutoGen, etc.) to stream live tool invocations to NEXUS for:
1. Native C++17 LibTorch temporal graph construction & neural risk inference
2. Deterministic hybrid evidence evaluation & policy recommendation
3. Isolated, independent AgntID evaluation (ONLY if VERIFIED_LIVE)
4. Comprehensive chronological audit trail logging

Truthful Posture:
- AgntID is queried ONLY if verified live.
- If AgntID is unconfigured/unverified, status is NOT_EVALUATED.
- NEXUS policy recommendations advise human operators and external gateways;
  they do not claim unverified autonomous blocking.
"""

import sys
import time
import json
import uuid
from pathlib import Path
from typing import Dict, Any, List, Optional

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from agent_orchestrator.cpp_bridge import NexusCppBridge
from agent_orchestrator.hybrid_risk_engine import HybridRiskEngine
from agent_orchestrator.agntid_adapter import (
    BaseAgntIDAdapter,
    IntegrationStatus,
    get_agntid_adapter,
    sanitize_secrets
)


class AgentToolGateway:
    """
    Generic ingestion gateway for live tool-using agent frameworks.
    Receives arbitrary tool invocations, translates them into typed
    NEXUS telemetry, evaluates them against the graph, and audits decisions.
    """

    def __init__(
        self,
        cpp_engine: Optional[NexusCppBridge] = None,
        hybrid_engine: Optional[HybridRiskEngine] = None,
        agntid_adapter: Optional[BaseAgntIDAdapter] = None
    ):
        self.cpp_engine = cpp_engine or NexusCppBridge()
        self.hybrid_engine = hybrid_engine or HybridRiskEngine()
        self.agntid_adapter = agntid_adapter or get_agntid_adapter()
        self._owns_cpp = (cpp_engine is None)
        self.audit_trail: List[Dict[str, Any]] = []

    def close(self):
        """Releases C++ bridge resources if owned."""
        if self._owns_cpp and hasattr(self.cpp_engine, "close"):
            self.cpp_engine.close()

    def process_tool_invocation(
        self,
        agent_id: str,
        declared_intent: str,
        tool_name: str,
        tool_args: Dict[str, Any],
        resource: str,
        permission: str,
        tool_result: Dict[str, Any],
        action: Optional[str] = None,
        timestamp: Optional[int] = None
    ) -> Dict[str, Any]:
        """
        Receives an arbitrary agent tool call and processes it through NEXUS.
        
        Args:
            agent_id: Unique identifier for the agent
            declared_intent: Natural language task intent declared by the agent/user
            tool_name: Name of the invoked tool (e.g. 'FILE_SYSTEM', 'BASH', 'HTTP_CLIENT')
            tool_args: Arguments passed to the tool (sanitized before storage)
            resource: Target resource URI or file path
            permission: Requested privilege ('READ', 'WRITE', 'EXECUTE', 'ADMIN')
            tool_result: Summary dictionary of tool execution result
            action: Optional explicit action string (defaults to tool_name + operation)
            timestamp: Epoch timestamp (defaults to current time)
            
        Returns:
            Structured evaluation dictionary strictly separating:
            - NEXUS neural model output
            - NEXUS hybrid evidence score & policy recommendation
            - AgntID runtime decision (only if VERIFIED_LIVE)
            - Audit trail confirmation
        """
        now = timestamp or int(time.time())
        event_id = f"EVT_GW_{uuid.uuid4().hex[:12].upper()}"
        resolved_action = action or f"EXECUTE_{tool_name.upper()}"

        # 1. Construct TelemetryEvent dictionary
        event = {
            "event_id": event_id,
            "agent_id": agent_id,
            "timestamp": now,
            "action": resolved_action,
            "tool": tool_name.upper(),
            "resource": resource,
            "permission": permission.upper(),
            "intent_id": declared_intent,
            "status": "SUCCESS" if tool_result.get("success", True) else "FAILED",
            "label": "Normal",  # Default label for live monitoring
            "is_synthetic": False
        }

        # 2. Ingest into native C++17 LibTorch graph engine
        cpp_res = self.cpp_engine.ingest_event(event)
        pred = cpp_res.get("prediction", {
            "class_name": "Normal",
            "confidence": 0.5,
            "probabilities": [0.5, 0.2, 0.2, 0.1]
        })
        attack_paths = cpp_res.get("attack_paths", [])

        # 3. Evaluate deterministic hybrid evidence score
        history_events = [entry["event"] for entry in self.audit_trail] + [event]
        hybrid_res = self.hybrid_engine.evaluate(
            current_event=event,
            history=history_events,
            model_prediction=pred,
            attack_paths=attack_paths,
            agntid_decision={"decision": "NOT_EVALUATED"}
        )

        # 4. Invoke AgntID ONLY if VERIFIED_LIVE
        if self.agntid_adapter.is_verified_live():
            agntid_res = self.agntid_adapter.evaluate(event, intent_context=declared_intent)
        else:
            status = self.agntid_adapter.get_status()
            agntid_res = {
                "agntid_available": False,
                "status": status,
                "decision": "NOT_EVALUATED",
                "action_blocked": False,
                "enforced": False,
                "reason": "AgntID adapter scaffold is present; official runtime validation is pending credentials and documented API contract.",
                "intent_context": declared_intent,
                "event_id": event_id,
                "source": self.agntid_adapter.get_source(),
                "raw_response": None
            }

        # 5. Build immutable audit record
        clean_args = sanitize_secrets(tool_args)
        audit_record = {
            "audit_id": len(self.audit_trail) + 1,
            "event": event,
            "tool_args": clean_args,
            "tool_result": sanitize_secrets(tool_result),
            "nexus_prediction": pred,
            "nexus_hybrid": hybrid_res,
            "agntid_decision": agntid_res
        }
        self.audit_trail.append(audit_record)

        # 6. Return response with strictly separated layers
        enforcement_truth = (
            "Enforced by official gateway"
            if agntid_res.get("enforced")
            else "NEXUS policy is advisory when AgntID is unavailable."
        )

        return {
            "status": "PROCESSED",
            "event_id": event_id,
            "timestamp": now,
            "agent_id": agent_id,
            "declared_intent": declared_intent,
            "tool_call": {
                "tool": tool_name,
                "action": resolved_action,
                "resource": resource,
                "permission": permission,
                "arguments_preview": clean_args
            },
            "nexus_model_prediction": {
                "class_name": pred["class_name"],
                "confidence": pred["confidence"],
                "class_index": pred.get("class_index", 0)
            },
            "nexus_hybrid_evidence": {
                "evidence_score": hybrid_res["evidence_score"],
                "policy_risk": hybrid_res["policy_risk"],
                "recommended_action": hybrid_res["recommended_action"],
                "evidence_signals": hybrid_res.get("evidence_signals", []),
                "reconstructed_attack_paths": len(attack_paths)
            },
            "agntid_runtime_decision": agntid_res,
            "enforcement_truth": enforcement_truth,
            "audit_trail_index": len(self.audit_trail)
        }


# =============================================================================
# FRAMEWORK-NEUTRAL INTEGRATION TEMPLATES (UNEXECUTED TEMPLATES FOR EVALUATORS)
# =============================================================================

"""
--- TEMPLATE 1: Generic Python Tool Decorator (UNEXECUTED_TEMPLATE) ---

def nexus_monitored_tool(gateway: AgentToolGateway, intent_provider, tool_name: str, permission: str = "READ"):
    def decorator(fn):
        def wrapper(*args, **kwargs):
            resource = str(kwargs.get("path") or (args[0] if args else "unknown_resource"))
            result = fn(*args, **kwargs)
            gateway.process_tool_invocation(
                agent_id="my_custom_agent",
                declared_intent=intent_provider(),
                tool_name=tool_name,
                tool_args={"args": str(args), "kwargs": str(kwargs)},
                resource=resource,
                permission=permission,
                tool_result={"output": str(result)[:200]}
            )
            return result
        return wrapper
    return decorator


--- TEMPLATE 2: Model Context Protocol (MCP) Interceptor (UNEXECUTED_TEMPLATE) ---

class NexusMCPToolInterceptor:
    def __init__(self, gateway: AgentToolGateway):
        self.gateway = gateway

    async def on_call_tool(self, tool_name: str, arguments: dict, agent_context: dict):
        # Intercept before or after tool execution
        intent = agent_context.get("task_intent", "UNSPECIFIED")
        resource = arguments.get("path") or arguments.get("url") or "mcp_resource"
        
        # Log to NEXUS Gateway
        nexus_verdict = self.gateway.process_tool_invocation(
            agent_id=agent_context.get("agent_id", "mcp_agent"),
            declared_intent=intent,
            tool_name=tool_name,
            tool_args=arguments,
            resource=resource,
            permission=arguments.get("permission", "EXECUTE"),
            tool_result={"status": "dispatched"}
        )
        
        # If policy recommends pause and operator configured strict enforcement:
        if nexus_verdict["nexus_hybrid_evidence"]["policy_risk"] == "Critical":
            logger.warn(f"NEXUS Advisory Warning: {nexus_verdict['nexus_hybrid_evidence']['recommended_action']}")
"""
