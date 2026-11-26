#!/usr/bin/env python3
"""
NEXUS System 05: AgntID Runtime Security Integration Bridge
Translates agent tool invocations into TelemetryEvent records, inspects the environment
for official AgntID SDK / CLI availability, invokes AgntID when available, and records
AgntID decisions independently from NEXUS predictions without fabricating enforcement claims.
"""

import os
import shutil
import importlib
import importlib.util
import time
import uuid
from typing import Dict, Any, Optional, List


try:
    from agent_orchestrator.agntid_adapter import get_agntid_adapter, BaseAgntIDAdapter
except ImportError:
    from agntid_adapter import get_agntid_adapter, BaseAgntIDAdapter


def inspect_agntid_environment() -> Dict[str, Any]:
    """
    Inspects Python environment, system PATH, and environment variables
    for official AgntID SDK, CLI, or API configurations.
    Returns honest diagnostic findings without fabricating enforcement claims.
    """
    findings: Dict[str, Any] = {
        "sdk_installed": False,
        "sdk_module": None,
        "sdk_version": None,
        "cli_available": False,
        "cli_path": None,
        "endpoint_configured": False,
        "endpoint_url": None,
        "enforcement_active": False,
        "decision_mode": "INDEPENDENT_EVALUATION",
        "notes": []
    }

    # 1. Check Python SDK package
    try:
        agntid_spec = importlib.util.find_spec("agntid")
        if agntid_spec is not None:
            findings["sdk_installed"] = True
            findings["sdk_module"] = "agntid"
            try:
                import agntid
                findings["sdk_version"] = getattr(agntid, "__version__", "unknown")
            except Exception as e:
                findings["notes"].append(f"agntid import failed: {e}")
        else:
            findings["notes"].append("Python package 'agntid' is not installed in the active environment.")
    except Exception as e:
        findings["notes"].append(f"Error checking python module: {e}")

    # 2. Check System PATH for CLI binary
    cli_path = shutil.which("agntid")
    if cli_path:
        findings["cli_available"] = True
        findings["cli_path"] = cli_path
    else:
        findings["notes"].append("CLI binary 'agntid' not found in system PATH.")

    # 3. Check for AgntID runtime service endpoint
    endpoint = os.getenv("AGNTID_URL") or os.getenv("AGNTID_ENDPOINT")
    if endpoint:
        findings["endpoint_configured"] = True
        findings["endpoint_url"] = endpoint
    else:
        findings["notes"].append("No AGNTID_URL or AGNTID_ENDPOINT environment variable configured.")

    # Enforcement is strictly False unless official live interception gateway is verified
    findings["enforcement_active"] = False
    findings["truthful_notice"] = (
        "AgntID adapter scaffold is present; official runtime validation is pending credentials and documented API contract. "
        "NEXUS policy is advisory when AgntID is unavailable. "
        "All current decisions in local demos are NEXUS decisions, not AgntID decisions. "
        "Mock external egress is simulated locally and performs no real network transfer."
    )
    return findings


class AgntIDBridge:
    """
    Bridge connecting agent tool execution to TelemetryEvent records and
    independent AgntID runtime security checks.
    """

    def __init__(self, agent_id: str = "agent_default"):
        self.agent_id = agent_id
        self.inspection = inspect_agntid_environment()
        self.adapter: BaseAgntIDAdapter = get_agntid_adapter()
        self.evaluated_count = 0
        self.action_history: List[Dict[str, Any]] = []

    def is_available(self) -> bool:
        """Returns True if an official AgntID SDK, CLI, or endpoint is actively present and ready."""
        return self.adapter.is_available()

    def get_status(self) -> Dict[str, Any]:
        """Returns diagnostic status of the AgntID integration."""
        return {
            "available": self.is_available(),
            "source": self.adapter.get_source(),
            "inspection": self.inspection,
            "evaluated_count": self.evaluated_count
        }

    def tool_call_to_telemetry(
        self,
        tool: str,
        action: str,
        resource: str,
        permission: str,
        status: str = "SUCCESS",
        agent_id: Optional[str] = None,
        intent_id: str = "",
        label: str = "Normal",
        is_synthetic: bool = False,
        timestamp: Optional[int] = None
    ) -> Dict[str, Any]:
        """
        Translates a live tool invocation into a concrete TelemetryEvent conforming to
        nexus::telemetry::TelemetryEvent schema.
        is_synthetic is carefully set:
          - False for live agent sandboxed execution
          - True for synthetic replay or dataset generation
        """
        if not tool or not action:
            raise ValueError("TelemetryEvent requires non-empty 'tool' and 'action'.")

        effective_agent_id = agent_id or self.agent_id
        if not effective_agent_id:
            raise ValueError("TelemetryEvent requires non-empty 'agent_id'.")

        effective_ts = int(time.time()) if timestamp is None else int(timestamp)
        if effective_ts < 0:
            raise ValueError(f"TelemetryEvent timestamp must be >= 0, got {effective_ts}")

        event_id = f"EVT_LIVE_{uuid.uuid4().hex[:8].upper()}"

        event: Dict[str, Any] = {
            "event_id": event_id,
            "agent_id": effective_agent_id,
            "timestamp": effective_ts,
            "action": action,
            "tool": tool,
            "resource": resource or "NONE",
            "permission": permission or "NONE",
            "intent_id": intent_id or f"INT_{effective_agent_id}_{uuid.uuid4().hex[:6]}",
            "status": status,
            "label": label,
            "is_synthetic": bool(is_synthetic)
        }
        return event

    def evaluate_event(
        self,
        event: Dict[str, Any],
        intent_context: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Evaluates a TelemetryEvent against AgntID runtime security rules via the adapter.
        If an official AgntID SDK or endpoint is available, queries it.
        If not available, records AgntID's status honestly as NOT_EVALUATED
        without fabricating fake enforcement blocks.
        """
        self.evaluated_count += 1
        decision = self.adapter.evaluate(event, intent_context=intent_context)
        self.action_history.append(decision)
        return decision
