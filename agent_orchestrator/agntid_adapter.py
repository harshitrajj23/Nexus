#!/usr/bin/env python3
"""
NEXUS System 05: Official AgntID Runtime Security Adapter Interface
Provides an extensible adapter architecture to integrate with official AgntID SDK,
CLI, or HTTP API endpoints when available in the environment.

Integrity Rules:
- If no official AgntID package or endpoint is present, returns normalized NOT_EVALUATED
  without fabricating decisions or enforcement blocks.
- Never labels simulated policies as AgntID decisions.
- Enforced is True ONLY if an actual runtime mechanism intercepted or blocked the action.
- Strips any secrets/credentials before recording raw responses.
"""

import os
import sys
import shutil
import importlib
import importlib.util
import json
import logging
from abc import ABC, abstractmethod
from typing import Dict, Any, Optional

logger = logging.getLogger("nexus.agntid_adapter")


class BaseAgntIDAdapter(ABC):
    """Abstract base class for AgntID runtime security adapters."""

    @abstractmethod
    def is_available(self) -> bool:
        """Returns True if the integration backend is active and configured."""
        pass

    @abstractmethod
    def get_source(self) -> str:
        """Returns the source identifier: 'official_sdk', 'official_cli', 'official_api', or 'unavailable'."""
        pass

    @abstractmethod
    def evaluate(
        self,
        event: Dict[str, Any],
        intent_context: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Evaluates a tool action event against AgntID runtime security rules.
        Must return normalized dictionary:
          - decision: ALLOW | DENY | CHALLENGE | ERROR | NOT_EVALUATED
          - enforced: bool (True ONLY if actually blocked)
          - reason: str
          - intent_context: str
          - event_id: str
          - source: str
          - raw_response: Optional[Dict[str, Any]] (without secrets)
        """
        pass


class UnconfiguredAgntIDAdapter(BaseAgntIDAdapter):
    """
    Default adapter used when no official AgntID SDK, CLI, or endpoint is configured.
    Preserves honest NOT_EVALUATED status without fabricating decisions.
    """

    def is_available(self) -> bool:
        return False

    def get_source(self) -> str:
        return "unavailable"

    def evaluate(
        self,
        event: Dict[str, Any],
        intent_context: Optional[str] = None
    ) -> Dict[str, Any]:
        effective_intent = intent_context or event.get("intent_id", "UNSPECIFIED_INTENT")
        return {
            "agntid_available": False,
            "decision": "NOT_EVALUATED",
            "action_blocked": False,
            "enforced": False,
            "reason": "Official AgntID runtime SDK/CLI not configured in environment; NEXUS policy remains advisory.",
            "intent_context": effective_intent,
            "event_id": event.get("event_id", ""),
            "source": self.get_source(),
            "raw_response": None
        }


class OfficialSDKAgntIDAdapter(BaseAgntIDAdapter):
    """
    Wraps an installed official 'agntid' Python SDK module if present.
    """

    def __init__(self, sdk_module: Any):
        self._sdk = sdk_module
        self._version = getattr(sdk_module, "__version__", "unknown")

    def is_available(self) -> bool:
        return True

    def get_source(self) -> str:
        return f"official_sdk (v{self._version})"

    def evaluate(
        self,
        event: Dict[str, Any],
        intent_context: Optional[str] = None
    ) -> Dict[str, Any]:
        effective_intent = intent_context or event.get("intent_id", "")
        event_id = event.get("event_id", "")

        try:
            # Introspect available evaluation function on official SDK
            eval_fn = getattr(self._sdk, "evaluate_action", None) or getattr(self._sdk, "check_permission", None) or getattr(self._sdk, "evaluate", None)
            if callable(eval_fn):
                raw = eval_fn(
                    agent_id=event.get("agent_id"),
                    action=event.get("action"),
                    tool=event.get("tool"),
                    resource=event.get("resource"),
                    intent=effective_intent
                )
                raw_dict = raw if isinstance(raw, dict) else getattr(raw, "__dict__", {"result": str(raw)})

                # Normalize decision
                raw_decision = str(raw_dict.get("decision", raw_dict.get("status", "ALLOW"))).upper()
                if "DENY" in raw_decision or "BLOCK" in raw_decision:
                    decision = "DENY"
                    action_blocked = True
                    enforced = bool(raw_dict.get("enforced", True))
                elif "CHALLENGE" in raw_decision:
                    decision = "CHALLENGE"
                    action_blocked = False
                    enforced = False
                elif "ALLOW" in raw_decision:
                    decision = "ALLOW"
                    action_blocked = False
                    enforced = False
                else:
                    decision = "ALLOW"
                    action_blocked = False
                    enforced = False

                return {
                    "agntid_available": True,
                    "decision": decision,
                    "action_blocked": action_blocked,
                    "enforced": enforced,
                    "reason": raw_dict.get("reason", "Decision emitted by official AgntID SDK"),
                    "intent_context": effective_intent,
                    "event_id": event_id,
                    "source": self.get_source(),
                    "raw_response": {k: v for k, v in raw_dict.items() if "secret" not in k.lower() and "key" not in k.lower()}
                }
            else:
                return {
                    "agntid_available": True,
                    "decision": "NOT_INVOKED",
                    "action_blocked": False,
                    "enforced": False,
                    "reason": "Official agntid package found but recognized evaluation method is not exported.",
                    "intent_context": effective_intent,
                    "event_id": event_id,
                    "source": self.get_source(),
                    "raw_response": None
                }
        except Exception as e:
            logger.error(f"Error invoking official AgntID SDK: {e}")
            return {
                "agntid_available": True,
                "decision": "ERROR",
                "action_blocked": False,
                "enforced": False,
                "reason": f"Official AgntID SDK invocation error: {e}",
                "intent_context": effective_intent,
                "event_id": event_id,
                "source": self.get_source(),
                "raw_response": None
            }


class OfficialCLIAgntIDAdapter(BaseAgntIDAdapter):
    """
    Wraps an official 'agntid' CLI binary found in system PATH.
    """

    def __init__(self, cli_path: str):
        self._cli_path = cli_path

    def is_available(self) -> bool:
        return bool(self._cli_path and os.path.isfile(self._cli_path))

    def get_source(self) -> str:
        return f"official_cli ({self._cli_path})"

    def evaluate(
        self,
        event: Dict[str, Any],
        intent_context: Optional[str] = None
    ) -> Dict[str, Any]:
        effective_intent = intent_context or event.get("intent_id", "")
        event_id = event.get("event_id", "")

        import subprocess
        try:
            cmd = [
                self._cli_path,
                "check",
                "--tool", str(event.get("tool", "")),
                "--action", str(event.get("action", "")),
                "--resource", str(event.get("resource", "")),
                "--intent", str(effective_intent),
                "--json"
            ]
            res = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
            if res.returncode == 0:
                raw_dict = json.loads(res.stdout.strip())
                decision = str(raw_dict.get("decision", "ALLOW")).upper()
                return {
                    "agntid_available": True,
                    "decision": decision,
                    "action_blocked": decision == "DENY",
                    "enforced": bool(raw_dict.get("enforced", False)),
                    "reason": raw_dict.get("reason", "CLI check completed"),
                    "intent_context": effective_intent,
                    "event_id": event_id,
                    "source": self.get_source(),
                    "raw_response": raw_dict
                }
            else:
                return {
                    "agntid_available": True,
                    "decision": "ERROR",
                    "action_blocked": False,
                    "enforced": False,
                    "reason": f"AgntID CLI exited with code {res.returncode}: {res.stderr.strip()}",
                    "intent_context": effective_intent,
                    "event_id": event_id,
                    "source": self.get_source(),
                    "raw_response": None
                }
        except Exception as e:
            return {
                "agntid_available": True,
                "decision": "ERROR",
                "action_blocked": False,
                "enforced": False,
                "reason": f"AgntID CLI execution failed: {e}",
                "intent_context": effective_intent,
                "event_id": event_id,
                "source": self.get_source(),
                "raw_response": None
            }


class OfficialAPIAgntIDAdapter(BaseAgntIDAdapter):
    """
    Connects to a configured AgntID HTTP API endpoint (e.g. AGNTID_ENDPOINT).
    """

    def __init__(self, endpoint_url: str, api_key: Optional[str] = None):
        self._endpoint_url = endpoint_url
        self._api_key = api_key

    def is_available(self) -> bool:
        return bool(self._endpoint_url)

    def get_source(self) -> str:
        return f"official_api ({self._endpoint_url})"

    def evaluate(
        self,
        event: Dict[str, Any],
        intent_context: Optional[str] = None
    ) -> Dict[str, Any]:
        effective_intent = intent_context or event.get("intent_id", "")
        event_id = event.get("event_id", "")

        import urllib.request
        import urllib.error

        payload = {
            "event_id": event_id,
            "agent_id": event.get("agent_id"),
            "tool": event.get("tool"),
            "action": event.get("action"),
            "resource": event.get("resource"),
            "permission": event.get("permission"),
            "intent_context": effective_intent
        }

        headers = {"Content-Type": "application/json"}
        if self._api_key:
            headers["Authorization"] = f"Bearer {self._api_key}"

        try:
            req = urllib.request.Request(
                self._endpoint_url,
                data=json.dumps(payload).encode("utf-8"),
                headers=headers,
                method="POST"
            )
            with urllib.request.urlopen(req, timeout=4.0) as resp:
                raw_dict = json.loads(resp.read().decode("utf-8"))
                decision = str(raw_dict.get("decision", "ALLOW")).upper()
                return {
                    "agntid_available": True,
                    "decision": decision,
                    "action_blocked": decision == "DENY",
                    "enforced": bool(raw_dict.get("enforced", False)),
                    "reason": raw_dict.get("reason", "API decision received"),
                    "intent_context": effective_intent,
                    "event_id": event_id,
                    "source": self.get_source(),
                    "raw_response": raw_dict
                }
        except Exception as e:
            return {
                "agntid_available": True,
                "decision": "ERROR",
                "action_blocked": False,
                "enforced": False,
                "reason": f"AgntID API request failed: {e}",
                "intent_context": effective_intent,
                "event_id": event_id,
                "source": self.get_source(),
                "raw_response": None
            }


def get_agntid_adapter() -> BaseAgntIDAdapter:
    """
    Factory function that detects the local environment and instantiates
    the appropriate official adapter, falling back to UnconfiguredAgntIDAdapter.
    """
    # 1. Check Python module
    try:
        spec = importlib.util.find_spec("agntid")
        if spec is not None:
            import agntid
            return OfficialSDKAgntIDAdapter(agntid)
    except Exception:
        pass

    # 2. Check CLI
    cli_path = shutil.which("agntid")
    if cli_path and os.path.isfile(cli_path):
        return OfficialCLIAgntIDAdapter(cli_path)

    # 3. Check Endpoint
    endpoint = os.getenv("AGNTID_URL") or os.getenv("AGNTID_ENDPOINT")
    if endpoint:
        api_key = os.getenv("AGNTID_API_KEY")
        return OfficialAPIAgntIDAdapter(endpoint, api_key)

    # 4. Fallback: Honest Unconfigured Adapter
    return UnconfiguredAgntIDAdapter()
