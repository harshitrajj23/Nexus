export type RiskLevel = 'Normal' | 'Suspicious' | 'HighRisk' | 'Critical';

export type NodeType =
  | 'Agent'
  | 'Tool'
  | 'Resource'
  | 'Action'
  | 'Permission'
  | 'Identity'
  | 'Destination';

export type RelationType =
  | 'USES'
  | 'ACCESSES'
  | 'PERFORMS'
  | 'AUTHORIZED_BY'
  | 'MODIFIES'
  | 'FOLLOWS'
  | 'COMMUNICATES_WITH';

export interface TelemetryEvent {
  event_id: string;
  agent_id: string;
  timestamp: number;
  action: string;
  tool: string;
  resource: string;
  permission: string;
  intent_id: string;
  status: string;
  label: string;
  is_synthetic: boolean;
}

export interface RiskPrediction {
  class_name: RiskLevel;
  class_index: number;
  confidence: number;
  probabilities: number[];
  logits: number[];
}

export interface PathStep {
  edge_id?: number;
  edge_score?: number;
  node_id?: number;
  node_key?: string;
  node_type?: string;
  relation?: string;
  source_id?: number;
  destination_id?: number;
  source_key?: string;
  destination_key?: string;
  relation_name?: string;
  timestamp: number;
  attribution_score?: number;
  structural_multiplier?: number;
}

export interface AttackPath {
  steps: PathStep[];
  total_score: number;
  confidence: number;
  summary: string;
  target_resource: string;
  compromised_agent: string;
}

export interface GraphNode {
  id: number;
  type: number;
  type_name: NodeType;
  key: string;
  created_at: number;
  is_sensitive?: boolean;
  is_elevated?: boolean;
  is_external?: boolean;
}

export interface GraphEdge {
  id: number;
  source: number;
  destination: number;
  relation: number;
  relation_name: RelationType;
  timestamp: number;
}

export interface EvidenceSignal {
  id: string;
  name: string;
  points: number;
  reason: string;
}

export interface AttackExplanation {
  narrative: string;
  affected_agent: string;
  sensitive_resource?: string | null;
  privilege_or_role?: string | null;
  external_destination?: string | null;
  chronological_event_ids: string[];
  time_span_seconds: number;
  contributing_graph_edges: any[];
  attack_path_score: number;
  evidence_signals: EvidenceSignal[];
  model_output: {
    prediction: string;
    confidence: number;
  };
  policy_recommendation: {
    risk: RiskLevel;
    action: string;
  };
  agntid_evaluation_status: string;
  held_out_status?: string;
}

export interface HybridAssessment {
  model_prediction: RiskLevel;
  model_confidence: number;
  evidence_score: number;
  evidence_signals: EvidenceSignal[];
  policy_risk: RiskLevel;
  recommended_action: string;
  actual_enforcement_status: string;
  is_enforced: boolean;
  attack_explanation: AttackExplanation;
}

export interface GraphSnapshot {
  status: string;
  nodes: GraphNode[];
  edges: GraphEdge[];
  top_paths?: AttackPath[];
  active_attack_path?: AttackPath | null;
  num_nodes?: number;
  num_edges?: number;
  latest_hybrid_assessment?: HybridAssessment | null;
  active_attack_explanation?: AttackExplanation | null;
}

export interface AgntIDDecision {
  agntid_available: boolean;
  decision: string;
  action_blocked: boolean;
  enforced: boolean;
  reason: string;
  raw_response?: any;
  event_id?: string;
}

export interface IngestResponse {
  status: string;
  event_id: string;
  latency_ms: number;
  prediction: RiskPrediction;
  attack_paths: AttackPath[];
  agntid_decision: AgntIDDecision;
  graph_stats: {
    nodes: number;
    edges: number;
  };
  hybrid_assessment?: HybridAssessment;
  evidence_score?: number;
  evidence_signals?: EvidenceSignal[];
  policy_risk?: RiskLevel;
  recommended_action?: string;
  actual_enforcement_status?: string;
  attack_explanation?: AttackExplanation;
}

export interface HealthResponse {
  status: string;
  timestamp: number;
  cpp_engine: {
    status: string;
    message: string;
    model_loaded: boolean;
    model_path: string;
    nodes_count: number;
    edges_count: number;
    sequence_length: number;
  };
  hybrid_risk_engine?: {
    status: string;
    version: string;
    scoring_weights: Record<string, number>;
  };
  agntid_integration: {
    sdk_installed: boolean;
    sdk_module: string | null;
    sdk_version: string | null;
    cli_available: boolean;
    cli_path: string | null;
    endpoint_configured: boolean;
    endpoint_url: string | null;
    enforcement_active: boolean;
    decision_mode: string;
    notes: string[];
  };
}

export interface MetricsData {
  metrics: {
    ablation_comparison: {
      temporal_rgcn_gru_macro_f1: number;
      static_rgcn_baseline_macro_f1: number;
      temporal_delta_f1: number;
      temporal_latency_ms: number;
      static_latency_ms: number;
    };
    confusion_matrices?: {
      seen_test: number[][];
      unseen_generalization?: number[][];
    };
    class_metrics?: {
      [key: string]: {
        precision: number;
        recall: number;
        f1: number;
        support: number;
      };
    };
    evaluation_summary?: {
      fnr_temporal: number;
      fnr_static: number;
      fpr_temporal: number;
      fpr_static: number;
    };
    temporal_model_test?: {
      macro_f1: number;
      avg_latency_ms: number;
      false_negative_rate: number;
      false_positive_rate: number;
      f1_per_class: number[];
      precision_per_class: number[];
      recall_per_class: number[];
      confusion_matrix: number[][];
    };
    train_loss_history?: number[];
    val_loss_history?: number[];
  };
  manifest?: {
    dataset_name?: string;
    version?: string;
    dataset_version?: string;
    total_scenarios?: number;
    total_events?: number;
    scenarios_summary?: any;
    splits?: {
      train: { scenarios: number; events: number };
      val: { scenarios: number; events: number };
      test: { scenarios: number; events: number };
      test_unseen: { scenarios: number; events: number };
    };
    withheld_attack_templates: any[];
  };
}

export interface ExperimentStep {
  step_index?: number;
  event: TelemetryEvent;
  tool_result: any;
  nexus_prediction: RiskPrediction;
  latency_ms: number;
  attack_paths: AttackPath[];
  agntid_decision: AgntIDDecision;
  hybrid_assessment?: HybridAssessment;
  evidence_score?: number;
  evidence_signals?: EvidenceSignal[];
  policy_risk?: RiskLevel;
  recommended_action?: string;
}

export interface ExperimentResponse {
  status: string;
  scenario: string;
  agent_id: string;
  total_steps: number;
  steps: ExperimentStep[];
  summary: {
    highest_risk_detected: RiskLevel;
    detected_attack_paths_count: number;
    active_attack_path?: AttackPath | null;
    agntid_enforcement_count: number;
    agntid_available: boolean;
    policy_risk_detected?: RiskLevel;
    peak_evidence_score?: number;
    recommended_action?: string;
    actual_enforcement_status?: string;
    attack_explanation?: AttackExplanation | null;
    model_vs_evidence_comparison?: {
      neural_model_prediction: string;
      deterministic_evidence_score: number;
      policy_risk: string;
      recommended_action: string;
      actual_enforcement_status: string;
    };
  };
}

export interface IntentComparisonRow {
  scenario: string;
  intent?: string;
  declared_intent?: string;
  action?: string;
  tool_action?: string;
  resource: string;
  nexus_model_output?: string;
  nexus_policy: RiskLevel;
  nexus_score: number;
  nexus_evidence_score?: number;
  nexus_policy_recommendation?: string;
  agntid_decision: string;
  enforcement: string;
  finding: string;
}

export interface IntentExperimentResponse {
  status: string;
  timestamp: number;
  agntid_status: {
    available: boolean;
    source: string;
    inspection: any;
  };
  conditions: Record<string, any>;
  comparison_matrix: IntentComparisonRow[];
  gap_analysis: {
    identical_resource_contrast: string;
    temporal_reconstruction_value: string;
    enforcement_transparency: string;
  };
}
