#!/usr/bin/env python3
"""
Unit tests for NEXUS AgntID Adapter Interface (agent_orchestrator/agntid_adapter.py).
Verifies:
- Default unconfigured behavior returns NOT_EVALUATED and enforced=False
- Mock official SDK module contract and decision normalization
- Raw response secret stripping
- Factory get_agntid_adapter() fallback
"""

import sys
from pathlib import Path
import unittest
from unittest.mock import MagicMock, patch

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from agent_orchestrator.agntid_adapter import (
    BaseAgntIDAdapter,
    IntegrationStatus,
    UnconfiguredAgntIDAdapter,
    ConfiguredUnverifiedAgntIDAdapter,
    OfficialSDKAgntIDAdapter,
    OfficialCLIAgntIDAdapter,
    OfficialAPIAgntIDAdapter,
    get_agntid_adapter
)


class TestAgntIDAdapter(unittest.TestCase):

    def setUp(self):
        self.sample_event = {
            "event_id": "EVT_TEST_001",
            "agent_id": "agent_analyst",
            "tool": "FILE_SYSTEM",
            "action": "READ_SECRET",
            "resource": "credentials/config.env",
            "permission": "READ",
            "intent_id": "INT_ROTATION_VALIDATE"
        }

    def test_unconfigured_adapter_contract(self):
        adapter = UnconfiguredAgntIDAdapter()
        self.assertFalse(adapter.is_available())
        self.assertEqual(adapter.get_status(), IntegrationStatus.UNAVAILABLE)
        self.assertEqual(adapter.get_source(), "unavailable")

        res = adapter.evaluate(self.sample_event, intent_context="Validate credential rotation")
        self.assertFalse(res["agntid_available"])
        self.assertEqual(res["decision"], "NOT_EVALUATED")
        self.assertFalse(res["action_blocked"])
        self.assertFalse(res["enforced"])
        self.assertIn("AgntID adapter scaffold is present", res["reason"])
        self.assertEqual(res["intent_context"], "Validate credential rotation")
        self.assertEqual(res["event_id"], "EVT_TEST_001")
        self.assertIsNone(res["raw_response"])

    def test_configured_unverified_adapter_refuses_guesswork(self):
        adapter = ConfiguredUnverifiedAgntIDAdapter("https://unverified.endpoint.local")
        self.assertEqual(adapter.get_status(), IntegrationStatus.CONFIGURED_BUT_UNVERIFIED)
        self.assertFalse(adapter.is_verified_live())

        res = adapter.evaluate(self.sample_event)
        self.assertEqual(res["decision"], "NOT_EVALUATED")
        self.assertFalse(res["enforced"])
        self.assertIn("Refusing to make unverified assumptions", res["reason"])

    def test_mock_sdk_adapter_allow(self):
        mock_sdk = MagicMock()
        mock_sdk.__version__ = "1.2.0"
        mock_sdk.evaluate_action.return_value = {
            "decision": "ALLOW",
            "reason": "Authorized within declared rotation intent",
            "enforced": False
        }

        adapter = OfficialSDKAgntIDAdapter(mock_sdk)
        self.assertTrue(adapter.is_available())
        self.assertIn("official_sdk", adapter.get_source())

        res = adapter.evaluate(self.sample_event, intent_context="Rotation task")
        self.assertTrue(res["agntid_available"])
        self.assertEqual(res["decision"], "ALLOW")
        self.assertFalse(res["action_blocked"])
        self.assertFalse(res["enforced"])
        self.assertEqual(res["reason"], "Authorized within declared rotation intent")

    def test_mock_sdk_adapter_deny_and_strip_secrets(self):
        mock_sdk = MagicMock()
        mock_sdk.__version__ = "1.2.0"
        mock_sdk.evaluate_action.return_value = {
            "decision": "DENY",
            "reason": "Unauthorized egress policy violation",
            "enforced": True,
            "secret_token_leaked": "SHOULD_BE_STRIPPED",
            "api_key": "SHOULD_BE_STRIPPED_TOO",
            "safe_metadata": "allowed_info"
        }

        adapter = OfficialSDKAgntIDAdapter(mock_sdk)
        res = adapter.evaluate(self.sample_event)
        self.assertEqual(res["decision"], "DENY")
        self.assertTrue(res["action_blocked"])
        self.assertTrue(res["enforced"])

        raw = res["raw_response"]
        self.assertNotIn("secret_token_leaked", raw)
        self.assertNotIn("api_key", raw)
        self.assertIn("safe_metadata", raw)

    def test_factory_fallback(self):
        # In current environment, no agntid SDK/CLI/endpoint exists
        adapter = get_agntid_adapter()
        self.assertIsInstance(adapter, UnconfiguredAgntIDAdapter)
        self.assertFalse(adapter.is_available())


if __name__ == "__main__":
    unittest.main()
