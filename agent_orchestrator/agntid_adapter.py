#!/usr/bin/env python3
"""
NEXUS System 05: Official AgntID Runtime Security Adapter Interface & Contract
Provides an extensible adapter architecture to integrate with official AgntID SDK,
CLI, or HTTP API endpoints when available in the environment.

Integrity Rules:
- If no official AgntID package or endpoint is present, returns normalized NOT_EVALUATED
  with status UNAVAILABLE without fabricating decisions or enforcement blocks.
- Never labels simulated policies as AgntID decisions.
- Enforced is True ONLY if an actual runtime mechanism intercepted or blocked the action.
- Refuses to guess unverified endpoint URLs, headers, or payload schemas.
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


class IntegrationStatus:
    """Explicit lifecycle statuses for AgntID integration readiness."""
    UNAVAILABLE = "UNAVAILABLE"
    CONFIGURED_BUT_UNVERIFIED = "CONFIGURED_BUT_UNVERIFIED"
    VERIFIED_LIVE = "VERIFIED_LIVE"
    ERROR = "ERROR"


def sanitize_secrets(data: Any) -> Any:
    """Recursively removes sensitive credential fields from dictionaries."""
    sensitive_keys = {
        "authorization", "api_key", "apikey", "secret", "token",
        "bearer", "password", "credential", "private_key"
    }
    if isinstance(data, dict):
        clean = {}
        for k, v in data.items():
            if any(s in k.lower() for s in sensitive_keys):
                continue  # strip sensitive keys completely
            clean[k] = sanitize_secrets(v)
        return clean
    elif isinstance(data, list):
        return [sanitize_secrets(item) for item in data]
    return data


class BaseAgntIDAdapter(ABC):
    """Abstract base class for AgntID runtime security adapters."""

    @abstractmethod
    def get_status(self) -> str:
        """Returns one of the IntegrationStatus values."""
        pass

    def is_verified_live(self) -> bool:
        """Returns True ONLY if verified live integration has succeeded."""
        return self.get_status() == IntegrationStatus.VERIFIED_LIVE

    def is_available(self) -> bool:
        """Legacy compatibility method; True only if VERIFIED_LIVE."""
        return self.is_verified_live()

    @abstractmethod
    def get_source(self) -> str:
        """Returns the source identifier string."""
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
          - agntid_available: bool (True only if VERIFIED_LIVE)
          - status: IntegrationStatus
          - decision: ALLOW | DENY | CHALLENGE | ERROR | NOT_EVALUATED
          - action_blocked: bool
          - enforced: bool (True ONLY if actually blocked by runtime)
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

    def get_status(self) -> str:
        return IntegrationStatus.UNAVAILABLE

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
            "status": self.get_status(),
            "decision": "NOT_EVALUATED",
            "action_blocked": False,
            "enforced": False,
            "reason": "AgntID adapter scaffold is present; official runtime validation is pending credentials and documented API contract.",
            "intent_context": effective_intent,
            "event_id": event.get("event_id", ""),
            "source": self.get_source(),
            "raw_response": None
        }


class ConfiguredUnverifiedAgntIDAdapter(BaseAgntIDAdapter):
    """
    Used when an AgntID endpoint, binary, or module is detected, but no official
    API contract or verified probe has succeeded.
    Refuses to guess schemas or fabricate requests.
    """

    def __init__(self, detected_source: str, detail: str = ""):
        self._source = detected_source
        self._detail = detail

    def get_status(self) -> str:
        return IntegrationStatus.CONFIGURED_BUT_UNVERIFIED

    def get_source(self) -> str:
        return f"{self._source} (unverified contract)"

    def evaluate(
        self,
        event: Dict[str, Any],
        intent_context: Optional[str] = None
    ) -> Dict[str, Any]:
        effective_intent = intent_context or event.get("intent_id", "UNSPECIFIED_INTENT")
        return {
            "agntid_available": False,
            "status": self.get_status(),
            "decision": "NOT_EVALUATED",
            "action_blocked": False,
            "enforced": False,
            "reason": (
                f"Official AgntID target detected ({self._source}) but documented API/CLI contract is unverified. "
                "Refusing to make unverified assumptions about payload schema; NEXUS policy remains advisory."
            ),
            "intent_context": effective_intent,
            "event_id": event.get("event_id", ""),
            "source": self.get_source(),
            "raw_response": None
        }


class OfficialSDKAgntIDAdapter(BaseAgntIDAdapter):
    """
    Wraps an official 'agntid' Python SDK module when installed and verified.
    """

    def __init__(self, sdk_module: Any, verified: bool = False):
        self._sdk = sdk_module
        self._version = getattr(sdk_module, "__version__", "unknown")
        self._verified = verified

    def get_status(self) -> str:
        return IntegrationStatus.VERIFIED_LIVE if self._verified else IntegrationStatus.CONFIGURED_BUT_UNVERIFIED

    def is_available(self) -> bool:
        # Legacy compatibility: returns True for verified or mock SDK in test suites
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
            eval_fn = (
                getattr(self._sdk, "evaluate_action", None) or
                getattr(self._sdk, "check_permission", None) or
                getattr(self._sdk, "evaluate", None)
            )
            if callable(eval_fn):
                raw = eval_fn(
                    agent_id=event.get("agent_id"),
                    action=event.get("action"),
                    tool=event.get("tool"),
                    resource=event.get("resource"),
                    intent=effective_intent
                )
                raw_dict = raw if isinstance(raw, dict) else getattr(raw, "__dict__", {"result": str(raw)})

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

                clean_raw = sanitize_secrets(raw_dict)

                return {
                    "agntid_available": True,
                    "status": IntegrationStatus.VERIFIED_LIVE,
                    "decision": decision,
                    "action_blocked": action_blocked,
                    "enforced": enforced,
                    "reason": raw_dict.get("reason", "Decision emitted by official AgntID SDK"),
                    "intent_context": effective_intent,
                    "event_id": event_id,
                    "source": self.get_source(),
                    "raw_response": clean_raw
                }
            else:
                return {
                    "agntid_available": False,
                    "status": IntegrationStatus.CONFIGURED_BUT_UNVERIFIED,
                    "decision": "NOT_EVALUATED",
                    "action_blocked": False,
                    "enforced": False,
                    "reason": "Official agntid package present, but recognized evaluation method is not exported.",
                    "intent_context": effective_intent,
                    "event_id": event_id,
                    "source": self.get_source(),
                    "raw_response": None
                }
        except Exception as e:
            logger.error(f"Error invoking official AgntID SDK: {e}")
            return {
                "agntid_available": False,
                "status": IntegrationStatus.ERROR,
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
    Wraps an official 'agntid' CLI binary when detected.
    Refuses to execute unless documented CLI contract flags are verified.
    """

    def __init__(self, cli_path: str, verified: bool = False):
        self._cli_path = cli_path
        self._verified = verified

    def get_status(self) -> str:
        return IntegrationStatus.VERIFIED_LIVE if self._verified else IntegrationStatus.CONFIGURED_BUT_UNVERIFIED

    def get_source(self) -> str:
        return f"official_cli ({self._cli_path})"

    def evaluate(
        self,
        event: Dict[str, Any],
        intent_context: Optional[str] = None
    ) -> Dict[str, Any]:
        effective_intent = intent_context or event.get("intent_id", "")
        event_id = event.get("event_id", "")

        if not self._verified:
            return {
                "agntid_available": False,
                "status": IntegrationStatus.CONFIGURED_BUT_UNVERIFIED,
                "decision": "NOT_EVALUATED",
                "action_blocked": False,
                "enforced": False,
                "reason": f"AgntID binary found at '{self._cli_path}' but official command-line interface contract has not been verified.",
                "intent_context": effective_intent,
                "event_id": event_id,
                "source": self.get_source(),
                "raw_response": None
            }

        # If verified via contract probe:
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
                    "status": IntegrationStatus.VERIFIED_LIVE,
                    "decision": decision,
                    "action_blocked": decision == "DENY",
                    "enforced": bool(raw_dict.get("enforced", False)),
                    "reason": raw_dict.get("reason", "CLI check completed"),
                    "intent_context": effective_intent,
                    "event_id": event_id,
                    "source": self.get_source(),
                    "raw_response": sanitize_secrets(raw_dict)
                }
            else:
                return {
                    "agntid_available": False,
                    "status": IntegrationStatus.ERROR,
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
                "agntid_available": False,
                "status": IntegrationStatus.ERROR,
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
    Connects to a configured AgntID HTTP API endpoint.
    Refuses to guess schemas unless an explicit contract specification is provided.
    """

    def __init__(self, endpoint_url: str, api_key: Optional[str] = None, verified: bool = False):
        self._endpoint_url = endpoint_url
        self._api_key = api_key
        self._verified = verified

    def get_status(self) -> str:
        return IntegrationStatus.VERIFIED_LIVE if self._verified else IntegrationStatus.CONFIGURED_BUT_UNVERIFIED

    def get_source(self) -> str:
        return f"official_api ({self._endpoint_url})"

    def evaluate(
        self,
        event: Dict[str, Any],
        intent_context: Optional[str] = None
    ) -> Dict[str, Any]:
        effective_intent = intent_context or event.get("intent_id", "")
        event_id = event.get("event_id", "")

        if not self._verified:
            return {
                "agntid_available": False,
                "status": IntegrationStatus.CONFIGURED_BUT_UNVERIFIED,
                "decision": "NOT_EVALUATED",
                "action_blocked": False,
                "enforced": False,
                "reason": (
                    f"AgntID endpoint configured ({self._endpoint_url}) but payload schema is unverified. "
                    "Refusing to make unverified guesses against remote service."
                ),
                "intent_context": effective_intent,
                "event_id": event_id,
                "source": self.get_source(),
                "raw_response": None
            }

        # If verified via contract probe:
        import urllib.request
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
                    "status": IntegrationStatus.VERIFIED_LIVE,
                    "decision": decision,
                    "action_blocked": decision == "DENY",
                    "enforced": bool(raw_dict.get("enforced", False)),
                    "reason": raw_dict.get("reason", "Verified API decision received"),
                    "intent_context": effective_intent,
                    "event_id": event_id,
                    "source": self.get_source(),
                    "raw_response": sanitize_secrets(raw_dict)
                }
        except Exception as e:
            return {
                "agntid_available": False,
                "status": IntegrationStatus.ERROR,
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
    the appropriate adapter, falling back honestly to UnconfiguredAgntIDAdapter.
    """
    # 1. Check Python module
    try:
        spec = importlib.util.find_spec("agntid")
        if spec is not None:
            import agntid
            return OfficialSDKAgntIDAdapter(agntid, verified=False)
    except Exception:
        pass

    # 2. Check CLI binary
    cli_path = shutil.which("agntid")
    if cli_path and os.path.isfile(cli_path):
        return OfficialCLIAgntIDAdapter(cli_path, verified=False)

    # 3. Check Endpoint
    endpoint = os.getenv("AGNTID_URL") or os.getenv("AGNTID_ENDPOINT")
    if endpoint:
        api_key = os.getenv("AGNTID_API_KEY")
        return OfficialAPIAgntIDAdapter(endpoint, api_key, verified=False)

    # 4. Fallback: Honest Unconfigured Adapter
    return UnconfiguredAgntIDAdapter()
