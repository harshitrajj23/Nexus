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
  edge_id: number;
  source_id: number;
  destination_id: number;
  source_key: string;
  destination_key: string;
  relation_name: string;
  timestamp: number;
  attribution_score: number;
  structural_multiplier: number;
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
}

export interface GraphEdge {
  id: number;
  source: number;
  destination: number;
  relation: number;
  relation_name: RelationType;
  timestamp: number;
}

export interface GraphSnapshot {
  status: string;
  nodes: GraphNode[];
  edges: GraphEdge[];
  top_paths?: AttackPath[];
  active_attack_path?: AttackPath | null;
  num_nodes?: number;
  num_edges?: number;
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
  };
  manifest?: {
    dataset_version: string;
    total_scenarios: number;
    total_events: number;
    splits: {
      train: { scenarios: number; events: number };
      val: { scenarios: number; events: number };
      test: { scenarios: number; events: number };
      test_unseen: { scenarios: number; events: number };
    };
    withheld_attack_templates: string[];
  };
}

export interface ExperimentStep {
  event: TelemetryEvent;
  tool_result: any;
  nexus_prediction: RiskPrediction;
  latency_ms: number;
  attack_paths: AttackPath[];
  agntid_decision: AgntIDDecision;
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
  };
}
