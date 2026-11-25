import {
  HealthResponse,
  GraphSnapshot,
  MetricsData,
  IngestResponse,
  ExperimentResponse,
  TelemetryEvent
} from '../types';

const API_BASE = 'http://localhost:8000';

async function fetchJson<T>(url: string, options?: RequestInit): Promise<T> {
  const res = await fetch(url, options);
  if (!res.ok) {
    const errorText = await res.text();
    throw new Error(`API error ${res.status}: ${errorText}`);
  }
  return res.json() as Promise<T>;
}

export async function getHealth(): Promise<HealthResponse> {
  return fetchJson<HealthResponse>(`${API_BASE}/health`);
}

export async function getGraphSnapshot(): Promise<GraphSnapshot> {
  return fetchJson<GraphSnapshot>(`${API_BASE}/api/graph/snapshot`);
}

export async function getMetrics(): Promise<MetricsData> {
  return fetchJson<MetricsData>(`${API_BASE}/api/metrics`);
}

export async function ingestEvent(event: TelemetryEvent): Promise<IngestResponse> {
  return fetchJson<IngestResponse>(`${API_BASE}/api/events/ingest`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(event),
  });
}

export async function runExperiment(
  scenario: 'normal' | 'adversarial_exfil' | 'unseen_attack',
  agentId?: string
): Promise<ExperimentResponse> {
  return fetchJson<ExperimentResponse>(`${API_BASE}/api/experiments/run`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      scenario,
      agent_id: agentId,
      reset_graph_before: true
    }),
  });
}

export async function resetGraph(): Promise<{ status: string; message: string }> {
  return fetchJson<{ status: string; message: string }>(`${API_BASE}/api/graph/reset`, {
    method: 'POST'
  });
}
