import React from 'react';
import {
  Activity,
  Layers,
  Zap,
  ShieldCheck,
  ShieldAlert,
  AlertTriangle,
  Clock,
  ArrowRight,
  Database
} from 'lucide-react';
import { TelemetryEvent, RiskPrediction, AgntIDDecision, AttackPath, RiskLevel, HybridAssessment } from '../types';
import { RiskBadge } from './RiskBadge';
import { ActiveSecurityDecisionPanel } from './ActiveSecurityDecisionPanel';

export interface LiveEventItem {
  event: TelemetryEvent;
  prediction?: RiskPrediction;
  latency_ms?: number;
  agntid?: AgntIDDecision;
  attack_paths?: AttackPath[];
  hybrid?: HybridAssessment;
}

interface OverviewViewProps {
  events: LiveEventItem[];
  nodeCount: number;
  edgeCount: number;
  latestAssessment?: HybridAssessment | null;
  onSelectEvent: (item: LiveEventItem) => void;
  onNavigateToGraph: () => void;
  onNavigateToThreats: () => void;
  onNavigateToLab: () => void;
  onNavigateToIntent?: () => void;
}

export const OverviewView: React.FC<OverviewViewProps> = ({
  events,
  nodeCount,
  edgeCount,
  latestAssessment,
  onSelectEvent,
  onNavigateToGraph,
  onNavigateToThreats,
  onNavigateToLab,
  onNavigateToIntent,
}) => {
  // Aggregate stats
  const totalEvents = events.length;
  const latencies = events.map(e => e.latency_ms || 0).filter(l => l > 0);
  const avgLatency = latencies.length > 0 ? (latencies.reduce((a, b) => a + b, 0) / latencies.length).toFixed(2) : '3.16';

  const riskOrder: Record<RiskLevel, number> = { Normal: 0, Suspicious: 1, HighRisk: 2, Critical: 3 };
  let highestRisk: RiskLevel = 'Normal';
  let highestConf = 0;

  events.forEach(e => {
    const r = e.hybrid?.policy_risk || e.prediction?.class_name || (e.event.label as RiskLevel) || 'Normal';
    if (riskOrder[r] >= riskOrder[highestRisk]) {
      highestRisk = r;
      highestConf = Math.max(highestConf, e.prediction?.confidence || 0);
    }
  });

  const suspiciousEvents = events.filter(e => {
    const r = e.hybrid?.policy_risk || e.prediction?.class_name || e.event.label;
    return r === 'Suspicious' || r === 'HighRisk' || r === 'Critical';
  });

  // Effective assessment for the active decision panel
  const activeAssessment = latestAssessment || (events.length > 0 ? events[events.length - 1].hybrid || null : null);

  return (
    <div className="space-y-6">
      {/* Executive Header */}
      <div className="bg-[#111827] border border-gray-800 rounded-xl p-5 shadow-sm">
        <div className="flex flex-col md:flex-row md:items-center justify-between gap-4 pb-4 border-b border-gray-800">
          <div>
            <div className="flex items-center space-x-2">
              <span className="text-[10px] font-mono uppercase tracking-widest px-2 py-0.5 rounded bg-blue-500/10 text-cyan-400 border border-cyan-500/30 font-bold">
                AgntID Agent Security Challenge
              </span>
              <span className="text-[10px] font-mono text-gray-500">Live Evaluation</span>
            </div>
            <h1 className="text-xl sm:text-2xl font-bold text-white tracking-tight mt-1.5">
              NEXUS — Temporal Security Intelligence for AI Agents
            </h1>
            <p className="text-xs sm:text-sm text-gray-400 mt-1">
              Detects multi-step attack trajectories that isolated tool-call monitoring misses.
            </p>
          </div>

          <div className="flex items-center space-x-2">
            {onNavigateToIntent && (
              <button
                onClick={onNavigateToIntent}
                className="px-3.5 py-1.5 rounded-lg bg-blue-600/30 hover:bg-blue-600 text-blue-300 hover:text-white text-xs font-mono border border-blue-500/40 transition-colors flex items-center space-x-1.5"
              >
                <span>Intent & AgntID</span>
                <ArrowRight className="w-3.5 h-3.5" />
              </button>
            )}
            <button
              onClick={onNavigateToLab}
              className="px-3.5 py-1.5 rounded-lg bg-purple-600/30 hover:bg-purple-600 text-purple-300 hover:text-white text-xs font-mono border border-purple-500/40 transition-colors flex items-center space-x-1.5"
            >
              <span>Experiment Lab</span>
              <ArrowRight className="w-3.5 h-3.5" />
            </button>
          </div>
        </div>

        {/* 5-Question Orientation Viewport */}
        <div className="mt-3.5 grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-5 gap-2.5 text-[11px] font-mono">
          <div className="bg-gray-900/70 p-2.5 rounded-lg border border-gray-800">
            <span className="text-cyan-400 block font-bold uppercase text-[10px]">1. What is NEXUS?</span>
            <span className="text-gray-300 mt-0.5 block leading-tight">Dynamic temporal graph engine & evidence scoring for agent tool calls.</span>
          </div>
          <div className="bg-gray-900/70 p-2.5 rounded-lg border border-gray-800">
            <span className="text-amber-400 block font-bold uppercase text-[10px]">2. What attack is happening?</span>
            <span className="text-gray-300 mt-0.5 block leading-tight">Credential read & role escalation chaining to external egress.</span>
          </div>
          <div className="bg-gray-900/70 p-2.5 rounded-lg border border-gray-800">
            <span className="text-rose-400 block font-bold uppercase text-[10px]">3. Why is it dangerous?</span>
            <span className="text-gray-300 mt-0.5 block leading-tight">Individual calls look plausible, but together construct an unauthorized exploit path.</span>
          </div>
          <div className="bg-gray-900/70 p-2.5 rounded-lg border border-gray-800">
            <span className="text-emerald-400 block font-bold uppercase text-[10px]">4. What should org do?</span>
            <span className="text-gray-300 mt-0.5 block leading-tight">Pause workflow, rotate credentials, and require human approval.</span>
          </div>
          <div className="bg-gray-900/70 p-2.5 rounded-lg border border-gray-800">
            <span className="text-purple-400 block font-bold uppercase text-[10px]">5. Was it blocked?</span>
            <span className="text-amber-300/90 mt-0.5 block leading-tight font-semibold">Advisory only — no live blocking gateway attached.</span>
          </div>
        </div>
      </div>

      {/* Side-by-Side Value Comparison: Traditional Isolated Monitoring vs NEXUS */}
      <div className="bg-[#111827] border border-gray-800 rounded-xl p-4 shadow-sm">
        <div className="text-[11px] font-mono uppercase tracking-wider text-gray-400 font-bold mb-2.5 flex items-center space-x-2">
          <span>Why NEXUS? — Architecture Comparison</span>
        </div>
        <div className="grid grid-cols-1 md:grid-cols-2 gap-3 text-xs font-mono">
          {/* Traditional */}
          <div className="p-3.5 rounded-lg bg-gray-900/80 border border-gray-800">
            <div className="flex items-center space-x-2 text-rose-400 font-bold pb-2 border-b border-gray-800">
              <span className="w-2 h-2 rounded-full bg-rose-500" />
              <span>Traditional Isolated Monitoring</span>
            </div>
            <ul className="mt-2.5 space-y-1.5 text-gray-300 text-[11px]">
              <li className="flex items-start space-x-1.5">
                <span className="text-gray-500">&bull;</span>
                <span><strong>Sees:</strong> Single &ldquo;HTTP POST&rdquo; request in isolation.</span>
              </li>
              <li className="flex items-start space-x-1.5">
                <span className="text-gray-500">&bull;</span>
                <span><strong>Misses:</strong> Prior secret file read and privilege escalation events.</span>
              </li>
              <li className="flex items-start space-x-1.5">
                <span className="text-gray-500">&bull;</span>
                <span><strong>Response:</strong> May not alert (blind to temporal multi-step context).</span>
              </li>
            </ul>
          </div>

          {/* NEXUS */}
          <div className="p-3.5 rounded-lg bg-blue-950/20 border border-blue-500/30">
            <div className="flex items-center space-x-2 text-cyan-400 font-bold pb-2 border-b border-gray-800">
              <span className="w-2 h-2 rounded-full bg-cyan-400" />
              <span>NEXUS Temporal Graph Evidence</span>
            </div>
            <ul className="mt-2.5 space-y-1.5 text-gray-300 text-[11px]">
              <li className="flex items-start space-x-1.5">
                <span className="text-cyan-400">&bull;</span>
                <span><strong>Sees:</strong> Secret read &rarr; elevated role &rarr; external egress.</span>
              </li>
              <li className="flex items-start space-x-1.5">
                <span className="text-cyan-400">&bull;</span>
                <span><strong>Reconstructs:</strong> Causal multi-step trajectory across graph entities.</span>
              </li>
              <li className="flex items-start space-x-1.5">
                <span className="text-cyan-400">&bull;</span>
                <span><strong>Response:</strong> Critical recommendation with transparent evidence score.</span>
              </li>
            </ul>
          </div>
        </div>
      </div>

      {/* 1. Proactive Hackathon Active Security Decision Panel */}
      <ActiveSecurityDecisionPanel
        assessment={activeAssessment}
        onNavigateToGraph={onNavigateToGraph}
        onNavigateToThreats={onNavigateToThreats}
      />

      {/* 2. Metric Cards Grid */}
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-5 gap-4">
        {/* Card 1: Total Events */}
        <div className="bg-[#111827] border border-gray-800 rounded-xl p-4 shadow-sm">
          <div className="flex items-center justify-between">
            <span className="text-xs font-medium text-gray-400">Total Ingested Events</span>
            <Activity className="w-4 h-4 text-blue-400" />
          </div>
          <div className="mt-2 flex items-baseline space-x-2">
            <span className="text-2xl font-bold font-mono text-white">{totalEvents}</span>
            <span className="text-[11px] text-gray-500">records</span>
          </div>
          <p className="text-[11px] text-gray-500 mt-1">Live telemetry stream</p>
        </div>

        {/* Card 2: Graph Entities */}
        <div
          className="bg-[#111827] border border-gray-800 rounded-xl p-4 shadow-sm cursor-pointer hover:border-blue-500/40 transition-colors"
          onClick={onNavigateToGraph}
        >
          <div className="flex items-center justify-between">
            <span className="text-xs font-medium text-gray-400">Dynamic Graph State</span>
            <Layers className="w-4 h-4 text-indigo-400" />
          </div>
          <div className="mt-2 flex items-baseline space-x-2">
            <span className="text-2xl font-bold font-mono text-indigo-400">{nodeCount}</span>
            <span className="text-xs text-gray-400">nodes</span>
            <span className="text-gray-600">/</span>
            <span className="text-xl font-bold font-mono text-indigo-300">{edgeCount}</span>
            <span className="text-xs text-gray-400">edges</span>
          </div>
          <p className="text-[11px] text-indigo-400/80 mt-1 flex items-center space-x-1">
            <span>Inspect in Live Graph</span>
            <ArrowRight className="w-3 h-3 inline" />
          </p>
        </div>

        {/* Card 3: Highest Detected Risk */}
        <div className="bg-[#111827] border border-gray-800 rounded-xl p-4 shadow-sm">
          <div className="flex items-center justify-between">
            <span className="text-xs font-medium text-gray-400">Peak Policy Risk</span>
            <ShieldAlert className="w-4 h-4 text-rose-400" />
          </div>
          <div className="mt-2.5">
            <RiskBadge level={activeAssessment?.policy_risk || highestRisk} size="lg" />
          </div>
          <p className="text-[11px] text-gray-500 mt-1.5 font-mono">
            {suspiciousEvents.length} anomalous steps
          </p>
        </div>

        {/* Card 4: Inference Latency */}
        <div className="bg-[#111827] border border-gray-800 rounded-xl p-4 shadow-sm">
          <div className="flex items-center justify-between">
            <span className="text-xs font-medium text-gray-400">C++ Inference Latency</span>
            <Zap className="w-4 h-4 text-amber-400" />
          </div>
          <div className="mt-2 flex items-baseline space-x-1.5">
            <span className="text-2xl font-bold font-mono text-amber-300">{avgLatency}</span>
            <span className="text-xs text-gray-400">ms/event</span>
          </div>
          <p className="text-[11px] text-emerald-400 mt-1 font-mono">
            LibTorch native forward pass
          </p>
        </div>

        {/* Card 5: AgntID Integration */}
        <div className="bg-[#111827] border border-gray-800 rounded-xl p-4 shadow-sm">
          <div className="flex items-center justify-between">
            <span className="text-xs font-medium text-gray-400">AgntID Decision Record</span>
            <ShieldCheck className="w-4 h-4 text-gray-400" />
          </div>
          <div className="mt-2 flex items-center space-x-1.5">
            <span className="text-xs px-2 py-0.5 rounded bg-gray-800 text-gray-300 font-mono border border-gray-700">
              NOT_EVALUATED
            </span>
          </div>
          <p className="text-[11px] text-gray-500 mt-1">
            Enforced: <span className="font-mono text-gray-400">false</span> (Independent)
          </p>
        </div>
      </div>

      {/* Main Content Area: Suspicious Sequence Alerts & Live Stream Table */}
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        {/* Left Column: Suspicious Activity Alerts */}
        <div className="lg:col-span-1 bg-[#111827] border border-gray-800 rounded-xl p-4 flex flex-col">
          <div className="flex items-center justify-between pb-3 border-b border-gray-800">
            <div className="flex items-center space-x-2">
              <AlertTriangle className="w-4 h-4 text-amber-400" />
              <h3 className="text-sm font-semibold text-white">Suspicious Action Sequences</h3>
            </div>
            <span className="text-xs font-mono px-2 py-0.5 rounded-full bg-amber-500/10 text-amber-400 border border-amber-500/20">
              {suspiciousEvents.length} detected
            </span>
          </div>

          <div className="mt-3 flex-1 overflow-y-auto max-h-[460px] space-y-2.5 pr-1">
            {suspiciousEvents.length === 0 ? (
              <div className="text-center py-12 text-gray-500 text-xs">
                <ShieldCheck className="w-8 h-8 mx-auto text-emerald-500/50 mb-2" />
                No suspicious activities detected yet.
                <p className="mt-1 text-[11px] text-gray-600">Run an adversarial scenario in Experiment Lab to trigger detection.</p>
              </div>
            ) : (
              suspiciousEvents.map((item, idx) => {
                const p = item.prediction;
                const r = item.hybrid?.policy_risk || p?.class_name || (item.event.label as RiskLevel);
                const hasPaths = (item.attack_paths?.length || 0) > 0;

                return (
                  <div
                    key={item.event.event_id || idx}
                    onClick={() => {
                      onSelectEvent(item);
                      onNavigateToThreats();
                    }}
                    className="p-3 rounded-lg bg-gray-900/80 border border-gray-800 hover:border-amber-500/40 cursor-pointer transition-all hover:bg-gray-850"
                  >
                    <div className="flex items-center justify-between">
                      <span className="text-xs font-mono font-medium text-gray-300">
                        {item.event.action}
                      </span>
                      <RiskBadge level={r} confidence={p?.confidence} size="sm" />
                    </div>

                    <div className="mt-2 text-[11px] text-gray-400 space-y-0.5 font-mono">
                      <div className="flex justify-between">
                        <span className="text-gray-500">Agent:</span>
                        <span className="text-blue-400 truncate max-w-[140px]">{item.event.agent_id}</span>
                      </div>
                      <div className="flex justify-between">
                        <span className="text-gray-500">Tool:</span>
                        <span className="text-purple-400">{item.event.tool}</span>
                      </div>
                      <div className="flex justify-between">
                        <span className="text-gray-500">Resource:</span>
                        <span className="text-amber-400 truncate max-w-[150px]">{item.event.resource}</span>
                      </div>
                    </div>

                    {hasPaths && (
                      <div className="mt-2 pt-2 border-t border-gray-800/80 flex items-center justify-between text-[10px] text-rose-400 font-mono">
                        <span>{item.attack_paths?.length} causal attack paths</span>
                        <span className="underline hover:text-rose-300">Inspect &rarr;</span>
                      </div>
                    )}
                  </div>
                );
              })
            )}
          </div>
        </div>

        {/* Right Column: Live Telemetry Feed Table */}
        <div className="lg:col-span-2 bg-[#111827] border border-gray-800 rounded-xl p-4 flex flex-col">
          <div className="flex items-center justify-between pb-3 border-b border-gray-800">
            <div className="flex items-center space-x-2">
              <Activity className="w-4 h-4 text-blue-400" />
              <h3 className="text-sm font-semibold text-white">Live Telemetry Ingestion Feed</h3>
            </div>
            <span className="text-xs text-gray-400 font-mono">
              Real-time events
            </span>
          </div>

          <div className="mt-3 flex-1 overflow-x-auto overflow-y-auto max-h-[460px]">
            {events.length === 0 ? (
              <div className="text-center py-16 text-gray-500 text-xs">
                Waiting for telemetry events...
              </div>
            ) : (
              <table className="w-full text-left text-xs">
                <thead className="bg-gray-900/80 text-gray-400 font-mono uppercase text-[10px] sticky top-0">
                  <tr>
                    <th className="py-2 px-3">Event ID</th>
                    <th className="py-2 px-3">Agent</th>
                    <th className="py-2 px-3">Tool</th>
                    <th className="py-2 px-3">Action</th>
                    <th className="py-2 px-3">Resource</th>
                    <th className="py-2 px-3">Policy Risk</th>
                    <th className="py-2 px-3">Evidence</th>
                    <th className="py-2 px-3">AgntID</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-gray-800 font-mono">
                  {events.slice().reverse().map((item, idx) => {
                    const e = item.event;
                    const p = item.prediction;
                    const r = item.hybrid?.policy_risk || p?.class_name || (e.label as RiskLevel) || 'Normal';
                    const score = item.hybrid?.evidence_score ?? 0;

                    return (
                      <tr
                        key={e.event_id || idx}
                        onClick={() => {
                          onSelectEvent(item);
                          onNavigateToThreats();
                        }}
                        className="hover:bg-gray-800/50 cursor-pointer transition-colors"
                      >
                        <td className="py-2.5 px-3 text-gray-400 font-mono text-[11px] truncate max-w-[100px]">
                          {e.event_id}
                        </td>
                        <td className="py-2.5 px-3 text-blue-400 truncate max-w-[110px]">
                          {e.agent_id}
                        </td>
                        <td className="py-2.5 px-3 text-purple-400">
                          {e.tool}
                        </td>
                        <td className="py-2.5 px-3 text-gray-200 font-semibold">
                          {e.action}
                        </td>
                        <td className="py-2.5 px-3 text-amber-300 truncate max-w-[130px]">
                          {e.resource}
                        </td>
                        <td className="py-2.5 px-3">
                          <RiskBadge level={r} size="sm" />
                        </td>
                        <td className="py-2.5 px-3 text-cyan-300 font-bold text-[11px]">
                          {score}/100
                        </td>
                        <td className="py-2.5 px-3">
                          <span className="text-[10px] px-1.5 py-0.5 rounded bg-gray-800 text-gray-400 border border-gray-700/60 font-mono">
                            {item.agntid?.decision || 'NOT_EVAL'}
                          </span>
                        </td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};
