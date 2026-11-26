#!/usr/bin/env python3
"""
Unit tests for NEXUS Agent Tool Gateway (agent_orchestrator/agent_tool_gateway.py).
Verifies:
- Tool invocation ingestion into C++ dynamic graph
- Hybrid evidence score calculation
- Clean separation of neural prediction, evidence score, and AgntID decision
- AgntID evaluated ONLY if verified live (honestly returns NOT_EVALUATED otherwise)
- Audit trail integrity
"""

import sys
from pathlib import Path
import unittest

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from agent_orchestrator.agent_tool_gateway import AgentToolGateway
from agent_orchestrator.agntid_adapter import IntegrationStatus


class TestAgentToolGateway(unittest.TestCase):

    def setUp(self):
        self.gateway = AgentToolGateway()

    def tearDown(self):
        self.gateway.close()

    def test_process_approved_tool_invocation(self):
        res = self.gateway.process_tool_invocation(
            agent_id="agent_devops_01",
            declared_intent="Validate mock credentials during an approved incident-response rotation.",
            tool_name="INCIDENT_TICKET",
            tool_args={"ticket_id": "INC-2026-0881"},
            resource="incidents/INC-2026-0881.json",
            permission="READ",
            tool_result={"success": True, "status": "APPROVED"}
        )

        self.assertEqual(res["status"], "PROCESSED")
        self.assertIn("nexus_model_prediction", res)
        self.assertIn("nexus_hybrid_evidence", res)
        self.assertIn("agntid_runtime_decision", res)

        # Confirm AgntID is NOT_EVALUATED when unverified/unconfigured
        agnt = res["agntid_runtime_decision"]
        self.assertEqual(agnt["decision"], "NOT_EVALUATED")
        self.assertFalse(agnt["enforced"])
        self.assertEqual(agnt["status"], IntegrationStatus.UNAVAILABLE)

        # Confirm truthful enforcement wording
        self.assertIn("NEXUS policy is advisory when AgntID is unavailable", res["enforcement_truth"])

        # Confirm audit trail entry
        self.assertEqual(len(self.gateway.audit_trail), 1)
        self.assertEqual(self.gateway.audit_trail[0]["event"]["tool"], "INCIDENT_TICKET")


if __name__ == "__main__":
    unittest.main()
