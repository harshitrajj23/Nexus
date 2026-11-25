#!/usr/bin/env python3
"""
NEXUS Explainable Hybrid Risk Engine
Combines native C++ LibTorch temporal model predictions with deterministic,
evidence-based security rules to compute a transparent, inspectable risk score,
human-readable attack narratives, and clear policy recommendations.
"""

from typing import Dict, Any, List, Optional


class HybridRiskEngine:
    """
    Evaluates agent telemetry and graph intelligence using deterministic security signals
    alongside neural temporal model predictions without overwriting either output.
    """

    # Keyword lists for deterministic detection
    SENSITIVE_RESOURCE_KEYWORDS = [
        "config.env", ".env", "credential", "id_rsa", "token", "secret",
        "password", "key", "security-credentials", "cloud/metadata",
        "metadata.json", "meta-data", "auth", "privkey"
    ]

    SENSITIVE_ACTION_KEYWORDS = [
        "READ_SECRET", "EXTRACT_TOKEN", "STEAL_ROLE_TOKEN", "METADATA_QUERY",
        "READ_CONFIG", "DUMP_CREDS", "EXFIL_ROLE_CREDS"
    ]

    PRIVILEGE_KEYWORDS = [
        "ADMIN", "ROOT", "IAM_ADMIN", "SUPERUSER", "PRIVILEGED",
        "WRITE_IAM", "CLUSTER_ADMIN"
    ]

    PRIVILEGE_ACTION_KEYWORDS = [
        "ESCALATE_ROLE", "PRIVILEGE_ESCALATION", "ASSUME_ROLE", "GRANT_PERMISSION",
        "MODIFY_SETUP", "INJECT_HOOK", "MODIFY_PERMISSIONS", "TAMPER"
    ]

    EXTERNAL_DESTINATION_KEYWORDS = [
        "mock://", "http://", "https://", "adversary", "exfil", "c2", "sink",
        "leak", "beacon", "dump", "169.254.169.254", "s3.amazonaws.com"
    ]

    EXTERNAL_ACTION_KEYWORDS = [
        "EXFILTRATE", "BEACON", "LEAK", "DUMP", "EXTERNAL_UPLOAD", "CONNECT_ADVERSARY"
    ]

    APPROVED_INTERNAL_HOSTS = [
        "127.0.0.1", "localhost", "internal.corp", "repo.internal"
    ]

    @classmethod
    def is_sensitive_resource(cls, resource: str, action: str = "") -> bool:
        res_lower = (resource or "").lower()
        act_upper = (action or "").upper()
        if any(kw in res_lower for kw in cls.SENSITIVE_RESOURCE_KEYWORDS):
            return True
        if any(kw in act_upper for kw in cls.SENSITIVE_ACTION_KEYWORDS):
            return True
        return False

    @classmethod
    def is_privileged_action(cls, permission: str, action: str = "") -> bool:
        perm_upper = (permission or "").upper()
        act_upper = (action or "").upper()
        if any(kw in perm_upper for kw in cls.PRIVILEGE_KEYWORDS):
            return True
        if any(kw in act_upper for kw in cls.PRIVILEGE_ACTION_KEYWORDS):
            return True
        return False

    @classmethod
    def is_external_destination(cls, resource: str, action: str = "") -> bool:
        res_lower = (resource or "").lower()
        act_upper = (action or "").upper()
        # Check if explicitly approved internal
        for host in cls.APPROVED_INTERNAL_HOSTS:
            if host in res_lower and not any(k in res_lower for k in ["exfil", "leak", "dump", "c2"]):
                return False

        if any(kw in res_lower for kw in cls.EXTERNAL_DESTINATION_KEYWORDS):
            return True
        if any(kw in act_upper for kw in cls.EXTERNAL_ACTION_KEYWORDS):
            return True
        return False

    def evaluate(
        self,
        current_event: Dict[str, Any],
        history: Optional[List[Dict[str, Any]]] = None,
        model_prediction: Optional[Dict[str, Any]] = None,
        attack_paths: Optional[List[Dict[str, Any]]] = None,
        agntid_decision: Optional[Dict[str, Any]] = None
    ) -> Dict[str, Any]:
        """
        Evaluates current event in context of execution history and model outputs.
        Returns hybrid risk score, discrete signals, human-readable narrative explanation,
        and honest policy recommendation.
        """
        all_events = list(history or [])
        if current_event not in all_events:
            all_events.append(current_event)

        agent_id = current_event.get("agent_id", "unknown_agent")

        # Collect observations across chronological sequence for this agent
        agent_events = [e for e in all_events if e.get("agent_id") == agent_id]
        if not agent_events:
            agent_events = [current_event]

        signals = []
        total_score = 0

        # Signal 1: Credential or Secret Access (+25)
        sensitive_events = [
            e for e in agent_events
            if self.is_sensitive_resource(e.get("resource", ""), e.get("action", ""))
        ]
        has_credential_access = len(sensitive_events) > 0
        if has_credential_access:
            res_names = ", ".join(sorted(set(e.get("resource", "secret") for e in sensitive_events)))
            signals.append({
                "id": "CREDENTIAL_ACCESS",
                "name": "Credential or Secret Access",
                "points": 25,
                "reason": f"Agent accessed sensitive credential/secret resource ({res_names})"
            })
            total_score += 25

        # Signal 2: Privileged / Admin / IAM Role Escalation (+25)
        privilege_events = [
            e for e in agent_events
            if self.is_privileged_action(e.get("permission", ""), e.get("action", ""))
        ]
        has_privilege_escalation = len(privilege_events) > 0
        if has_privilege_escalation:
            perms = ", ".join(sorted(set(e.get("permission", "ELEVATED") for e in privilege_events)))
            signals.append({
                "id": "PRIVILEGE_ESCALATION",
                "name": "Privileged / Admin Role Escalation",
                "points": 25,
                "reason": f"Agent requested or exercised elevated permissions ({perms})"
            })
            total_score += 25

        # Signal 3: Communication with External / Unapproved Destination (+35)
        external_events = [
            e for e in agent_events
            if self.is_external_destination(e.get("resource", ""), e.get("action", ""))
        ]
        has_external_destination = len(external_events) > 0
        if has_external_destination:
            dest_names = ", ".join(sorted(set(e.get("resource", "external endpoint") for e in external_events)))
            signals.append({
                "id": "EXTERNAL_DESTINATION",
                "name": "Unapproved External Communication",
                "points": 35,
                "reason": f"Agent attempted connection/data transfer to external destination ({dest_names})"
            })
            total_score += 35

        # Signal 4: Single Agent Connecting Full Attack Chain (+10)
        # Full chain = Credential Access + Privilege Escalation + External Egress
        has_full_chain = has_credential_access and has_privilege_escalation and has_external_destination
        if has_full_chain:
            signals.append({
                "id": "FULL_CHAIN_CORRELATION",
                "name": "Single Agent Multi-Step Attack Chain",
                "points": 10,
                "reason": "Single agent correlates full trajectory: secret access -> privilege escalation -> external egress"
            })
            total_score += 10
        elif len(attack_paths or []) > 0 and (has_credential_access or has_privilege_escalation):
            # Graph analyzer reconstructed a path
            signals.append({
                "id": "GRAPH_PATH_RECONSTRUCTED",
                "name": "Causal Attack Path Detected",
                "points": 5,
                "reason": f"Dynamic graph engine reconstructed {len(attack_paths)} causal multi-hop paths"
            })
            total_score += 5

        # Signal 5: Temporal Neural Model Anomaly (+10)
        pred_dict = model_prediction or {}
        pred_class = pred_dict.get("class_name", "Normal")
        pred_conf = float(pred_dict.get("confidence", 0.0))

        if pred_class in ["Suspicious", "HighRisk", "Critical"]:
            signals.append({
                "id": "NEURAL_TEMPORAL_ANOMALY",
                "name": "Neural Sequence Anomaly",
                "points": 10,
                "reason": f"Temporal R-GCN + GRU model flagged sequence as {pred_class} (confidence: {pred_conf:.1%})"
            })
            total_score += 10

        # Clamp final evidence score to [0, 100]
        final_score = max(0, min(100, total_score))

        # Map Evidence Score to Policy Risk & Recommended Action
        if final_score >= 75:
            policy_risk = "Critical"
            recommended_action = "Recommend workflow pause and investigation"
        elif final_score >= 50:
            policy_risk = "HighRisk"
            recommended_action = "Require human approval before next sensitive action"
        elif final_score >= 25:
            policy_risk = "Suspicious"
            recommended_action = "Alert analyst"
        else:
            policy_risk = "Normal"
            recommended_action = "Continue monitoring"

        # Honest enforcement disclosure
        enforced = False
        if agntid_decision and agntid_decision.get("enforced") is True:
            enforced = True
            enforcement_status = f"ENFORCED by AgntID ({agntid_decision.get('decision', 'BLOCK')})"
        else:
            enforcement_status = "NOT_ENFORCED (No active blocking engine attached; policy is advisory)"

        # Construct Attack Explanation Object
        explanation = self._build_explanation(
            agent_id=agent_id,
            events=agent_events,
            sensitive_events=sensitive_events,
            privilege_events=privilege_events,
            external_events=external_events,
            policy_risk=policy_risk,
            recommended_action=recommended_action,
            signals=signals,
            final_score=final_score,
            pred_dict=pred_dict,
            attack_paths=attack_paths or [],
            agntid_decision=agntid_decision
        )

        return {
            "model_prediction": pred_class,
            "model_confidence": pred_conf,
            "evidence_score": final_score,
            "evidence_signals": signals,
            "policy_risk": policy_risk,
            "recommended_action": recommended_action,
            "actual_enforcement_status": enforcement_status,
            "is_enforced": enforced,
            "attack_explanation": explanation
        }

    def _build_explanation(
        self,
        agent_id: str,
        events: List[Dict[str, Any]],
        sensitive_events: List[Dict[str, Any]],
        privilege_events: List[Dict[str, Any]],
        external_events: List[Dict[str, Any]],
        policy_risk: str,
        recommended_action: str,
        signals: List[Dict[str, Any]],
        final_score: int,
        pred_dict: Dict[str, Any],
        attack_paths: List[Dict[str, Any]],
        agntid_decision: Optional[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """Builds a human-readable narrative and structured audit object."""
        timestamps = [e.get("timestamp", 0) for e in events if e.get("timestamp")]
        time_span = max(timestamps) - min(timestamps) if len(timestamps) > 1 else 0

        event_ids = [e.get("event_id", "") for e in events if e.get("event_id")]

        sensitive_res = sensitive_events[0].get("resource") if sensitive_events else None
        privilege_role = privilege_events[0].get("permission") if privilege_events else None
        external_dest = external_events[0].get("resource") if external_events else None

        # Build clean narrative
        if policy_risk in ["HighRisk", "Critical"]:
            narrative_parts = [f"Agent '{agent_id}'"]
            if sensitive_res:
                narrative_parts.append(f"accessed sensitive resource '{sensitive_res}'")
            if privilege_role:
                narrative_parts.append(f"used elevated permission '{privilege_role}'")
            if external_dest:
                narrative_parts.append(f"attempted to communicate with unapproved destination '{external_dest}'")
            
            if time_span > 0:
                narrative_parts.append(f"within {time_span} seconds")

            narrative = " ".join(narrative_parts) + f". NEXUS computed an evidence score of {final_score}/100 ({policy_risk}) and recommends: {recommended_action}."
        elif policy_risk == "Suspicious":
            action_desc = events[-1].get("action", "activity")
            res_desc = events[-1].get("resource", "resource")
            narrative = f"Agent '{agent_id}' executed action '{action_desc}' on '{res_desc}', triggering early anomalous risk indicators (score {final_score}/100). Recommended response: {recommended_action}."
        else:
            narrative = f"Agent '{agent_id}' performed normal authorized operations with no unauthorized access or exfiltration detected (score {final_score}/100). Policy recommendation: {recommended_action}."

        # Extract top attack path metrics if available
        top_path_score = 0.0
        contributing_edges = []
        if attack_paths:
            top_path = attack_paths[0]
            top_path_score = float(top_path.get("total_score", 0.0))
            for step in top_path.get("steps", []):
                contributing_edges.append({
                    "source": step.get("source_key", ""),
                    "relation": step.get("relation_name", ""),
                    "destination": step.get("destination_key", ""),
                    "attribution": step.get("attribution_score", 0.0),
                    "multiplier": step.get("structural_multiplier", 1.0)
                })

        return {
            "narrative": narrative,
            "affected_agent": agent_id,
            "sensitive_resource": sensitive_res,
            "privilege_or_role": privilege_role,
            "external_destination": external_dest,
            "chronological_event_ids": event_ids,
            "time_span_seconds": time_span,
            "contributing_graph_edges": contributing_edges,
            "attack_path_score": top_path_score,
            "evidence_signals": signals,
            "model_output": {
                "prediction": pred_dict.get("class_name", "Normal"),
                "confidence": pred_dict.get("confidence", 0.0)
            },
            "policy_recommendation": {
                "risk": policy_risk,
                "action": recommended_action
            },
            "agntid_evaluation_status": agntid_decision.get("decision", "NOT_EVALUATED") if agntid_decision else "NOT_EVALUATED"
        }
