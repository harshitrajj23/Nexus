import React, { useState } from 'react';
import {
  ShieldAlert,
  AlertTriangle,
  ArrowRight,
  Clock,
  Compass,
  Zap,
  Layers,
  Database,
  ExternalLink,
  Target
} from 'lucide-react';
import { LiveEventItem } from './OverviewView';
import { AttackPath, RiskLevel } from '../types';
import { RiskBadge } from './RiskBadge';

interface ThreatIntelViewProps {
  selectedEvent: LiveEventItem | null;
  allEvents: LiveEventItem[];
  onSelectPath: (path: AttackPath) => void;
  onNavigateToGraph: () => void;
}

export const ThreatIntelView: React.FC<ThreatIntelViewProps> = ({
  selectedEvent,
  allEvents,
  onSelectPath,
  onNavigateToGraph
}) => {
  // Use selected event or fallback to the latest suspicious/critical event, or latest event
  const currentItem = selectedEvent || allEvents.find(e => {
    const r = e.prediction?.class_name || e.event.label;
    return r === 'Critical' || r === 'HighRisk' || r === 'Suspicious';
  }) || allEvents[allEvents.length - 1] || null;

  const [activePathIndex, setActivePathIndex] = useState<number>(0);

  if (!currentItem) {
    return (
      <div className="bg-[#111827] border border-gray-800 rounded-xl p-12 text-center">
        <ShieldAlert className="w-12 h-12 text-gray-600 mx-auto mb-3" />
        <h3 className="text-base font-semibold text-gray-300">No Security Events Available</h3>
        <p className="text-xs text-gray-500 mt-1 max-w-md mx-auto">
          Run a scenario in the Experiment Lab or wait for live telemetry events to populate the threat intelligence feed.
        </p>
      </div>
    );
  }

  const { event, prediction, latency_ms, agntid, attack_paths } = currentItem;
  const hybrid = currentItem.hybrid;
  const paths = attack_paths || [];
  const selectedPath: AttackPath | null = paths[activePathIndex] || paths[0] || null;

  const classNames: RiskLevel[] = ['Normal', 'Suspicious', 'HighRisk', 'Critical'];
  const probabilities = prediction?.probabilities || [0.7, 0.2, 0.08, 0.02];

  return (
    <div className="space-y-6">
      {hybrid && (
        <div className="bg-[#111827] border border-red-500/50 rounded-xl p-5 shadow-xl space-y-4">
          <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-3 pb-3 border-b border-gray-800">
            <div>
              <div className="flex items-center space-x-2">
                <span className="text-[11px] font-mono uppercase tracking-widest text-red-400 font-bold">
                  Active Policy Recommendation
                </span>
                <span className="text-[10px] font-mono px-2 py-0.5 rounded bg-gray-850 text-gray-400 border border-gray-750">
                  Evidence-Driven
                </span>
              </div>
              <h3 className="text-base font-bold text-white mt-1">
                {hybrid.recommended_action}
              </h3>
            </div>
            <div className="flex items-center space-x-3">
              <div className="text-right">
                <span className="text-[10px] font-mono text-gray-500 uppercase block">Evidence Score</span>
                <span className="text-lg font-bold font-mono text-white">{hybrid.evidence_score}<span className="text-xs text-gray-500">/100</span></span>
              </div>
              <RiskBadge level={hybrid.policy_risk} size="lg" />
            </div>
          </div>

          {/* Narrative Explanation */}
          <div className="bg-gray-900/90 rounded-lg p-3.5 border border-gray-800">
            <span className="text-[10px] font-mono uppercase text-gray-400 font-semibold block mb-1">
              Incident Narrative
            </span>
            <p className="text-xs leading-relaxed text-gray-200">
              {hybrid.attack_explanation.narrative}
            </p>
          </div>

          {/* Structured Evidence Matrix */}
          <div className="grid grid-cols-2 sm:grid-cols-4 gap-3 text-xs font-mono">
            <div className="bg-gray-900/70 p-2.5 rounded border border-gray-800">
              <span className="text-gray-500 block text-[10px] uppercase">Agent Identity</span>
              <span className="text-blue-400 font-bold">{hybrid.attack_explanation.affected_agent || 'N/A'}</span>
            </div>
            <div className="bg-gray-900/70 p-2.5 rounded border border-gray-800">
              <span className="text-gray-500 block text-[10px] uppercase">Sensitive Target</span>
              <span className="text-amber-300 font-bold truncate block">{hybrid.attack_explanation.sensitive_resource || 'None observed'}</span>
            </div>
            <div className="bg-gray-900/70 p-2.5 rounded border border-gray-800">
              <span className="text-gray-500 block text-[10px] uppercase">Privilege Level</span>
              <span className="text-rose-400 font-bold">{hybrid.attack_explanation.privilege_or_role || 'DEFAULT'}</span>
            </div>
            <div className="bg-gray-900/70 p-2.5 rounded border border-gray-800">
              <span className="text-gray-500 block text-[10px] uppercase">External Egress</span>
              <span className="text-red-400 font-bold truncate block">{hybrid.attack_explanation.external_destination || 'None observed'}</span>
            </div>
          </div>

          {/* Deterministic Signal Attribution Breakdown */}
          <div className="bg-gray-900/80 rounded-lg p-3.5 border border-gray-800">
            <div className="flex items-center justify-between pb-2 border-b border-gray-800">
              <span className="text-[11px] font-mono uppercase font-semibold text-gray-300">
                Deterministic Signal Attribution (+25 / +25 / +35 / +10 / +10)
              </span>
              <span className="text-[11px] font-mono text-cyan-400 font-semibold">
                Model: {hybrid.model_prediction} ({(hybrid.model_confidence * 100).toFixed(1)}%)
              </span>
            </div>

            {hybrid.evidence_signals.length === 0 ? (
              <p className="text-xs text-gray-500 font-mono mt-2">
                No high-risk structural signals triggered. Baseline safe activity.
              </p>
            ) : (
              <div className="mt-2.5 space-y-2">
                {hybrid.evidence_signals.map((sig) => (
                  <div key={sig.id} className="flex items-center justify-between text-xs font-mono bg-gray-950 p-2 rounded border border-gray-850">
                    <div className="flex items-center space-x-2">
                      <span className="w-10 text-center font-bold px-1.5 py-0.5 rounded bg-amber-500/20 text-amber-300 border border-amber-500/30">
                        +{sig.points}
                      </span>
                      <span className="text-gray-200 font-semibold">{sig.name}</span>
                      <span className="text-gray-400 text-[11px] hidden sm:inline">&bull; {sig.reason}</span>
                    </div>
                    <span className="text-[10px] text-gray-500">{sig.id}</span>
                  </div>
                ))}
              </div>
            )}
          </div>

          {/* Enforcement Transparency Disclosure */}
          <div className="flex items-center justify-between p-3 rounded-lg bg-gray-950 border border-gray-800 text-[11px] font-mono">
            <div className="flex items-center space-x-2">
              <span className="text-gray-500 uppercase">Actual Enforcement Status:</span>
              <span className="px-2 py-0.5 rounded bg-gray-800 text-amber-300 font-bold">
                {hybrid.actual_enforcement_status}
              </span>
            </div>
            <span className="text-gray-500 text-[10px] hidden sm:inline">
              Policy recommendations advise human operators; no inline interception without active AgntID hook.
            </span>
          </div>
        </div>
      )}
      {/* Top Threat Alert Header */}
      <div className="bg-[#111827] border border-gray-800 rounded-xl p-5 shadow-md">
        <div className="flex flex-col md:flex-row md:items-center justify-between gap-4 pb-4 border-b border-gray-800">
          <div className="flex items-center space-x-3">
            <div className="w-10 h-10 rounded-lg bg-rose-500/10 border border-rose-500/30 flex items-center justify-center text-rose-400">
              <ShieldAlert className="w-5 h-5" />
            </div>
            <div>
              <div className="flex items-center space-x-2">
                <span className="text-base font-bold text-white font-mono">{event.event_id}</span>
                <RiskBadge
                  level={hybrid?.policy_risk || prediction?.class_name || (event.label as RiskLevel)}
                  confidence={prediction?.confidence}
                  size="md"
                />
                <span className="text-xs px-2 py-0.5 rounded bg-gray-800 text-gray-400 font-mono">
                  {event.is_synthetic ? 'SYNTHETIC' : 'LIVE AGENT'}
                </span>
              </div>
              <p className="text-xs text-gray-400 mt-0.5">
                Timestamp: <span className="font-mono text-gray-300">{event.timestamp}</span> &bull; Agent: <span className="font-mono text-blue-400">{event.agent_id}</span>
              </p>
            </div>
          </div>

          <div className="flex items-center space-x-3">
            {selectedPath && (
              <button
                onClick={() => {
                  onSelectPath(selectedPath);
                  onNavigateToGraph();
                }}
                className="px-3.5 py-1.5 rounded-lg bg-red-600 hover:bg-red-500 text-white text-xs font-semibold shadow-md shadow-red-500/20 transition-all flex items-center space-x-1.5"
              >
                <span>View Attack Path in Graph</span>
                <ArrowRight className="w-3.5 h-3.5" />
              </button>
            )}
          </div>
        </div>

        {/* Event Detail Tiles */}
        <div className="mt-4 grid grid-cols-2 sm:grid-cols-4 gap-3 text-xs font-mono">
          <div className="bg-gray-900/80 p-2.5 rounded-lg border border-gray-800">
            <span className="text-gray-500 block text-[10px] uppercase">Action</span>
            <span className="text-gray-200 font-bold">{event.action}</span>
          </div>
          <div className="bg-gray-900/80 p-2.5 rounded-lg border border-gray-800">
            <span className="text-gray-500 block text-[10px] uppercase">Tool Invocated</span>
            <span className="text-purple-400 font-bold">{event.tool}</span>
          </div>
          <div className="bg-gray-900/80 p-2.5 rounded-lg border border-gray-800">
            <span className="text-gray-500 block text-[10px] uppercase">Resource Target</span>
            <span className="text-amber-300 font-bold truncate block">{event.resource}</span>
          </div>
          <div className="bg-gray-900/80 p-2.5 rounded-lg border border-gray-800">
            <span className="text-gray-500 block text-[10px] uppercase">Permission Level</span>
            <span className="text-rose-400 font-bold">{event.permission}</span>
          </div>
        </div>

        {/* Softmax Probability Distribution & Independent AgntID Record */}
        <div className="mt-4 pt-4 border-t border-gray-800 grid grid-cols-1 md:grid-cols-3 gap-4 items-center">
          <div className="md:col-span-2 space-y-1.5">
            <span className="text-xs font-medium text-gray-400 block">
              TemporalRiskModel Softmax Probability Distribution
            </span>
            <div className="grid grid-cols-4 gap-2 text-[11px] font-mono">
              {classNames.map((cName, idx) => {
                const prob = probabilities[idx] || 0;
                const isSelected = prediction?.class_name === cName;
                return (
                  <div key={cName} className={`p-2 rounded border ${isSelected ? 'bg-blue-950/60 border-blue-500/50' : 'bg-gray-900/50 border-gray-800'}`}>
                    <div className="flex justify-between text-gray-400 text-[10px]">
                      <span>{cName}</span>
                      <span className="text-gray-300">{(prob * 100).toFixed(1)}%</span>
                    </div>
                    <div className="w-full bg-gray-800 h-1.5 rounded-full mt-1.5 overflow-hidden">
                      <div
                        className={`h-full rounded-full ${idx === 3 ? 'bg-rose-500' : idx === 2 ? 'bg-orange-500' : idx === 1 ? 'bg-amber-400' : 'bg-emerald-400'}`}
                        style={{ width: `${Math.max(4, prob * 100)}%` }}
                      />
                    </div>
                  </div>
                );
              })}
            </div>
          </div>

          <div className="md:col-span-1 bg-gray-900/80 p-3 rounded-lg border border-gray-800 font-mono text-xs">
            <div className="text-gray-400 font-semibold text-[11px] flex items-center justify-between">
              <span>AgntID Runtime Check</span>
              <span className="text-[10px] text-gray-500">Independent</span>
            </div>
            <div className="mt-1.5 flex items-center space-x-2">
              <span className="px-2 py-0.5 rounded bg-gray-800 text-gray-300 font-bold text-[11px]">
                {agntid?.decision || 'NOT_EVALUATED'}
              </span>
              <span className="text-[10px] text-gray-500">
                (Enforced: {agntid?.enforced ? 'true' : 'false'})
              </span>
            </div>
            <p className="text-[10px] text-gray-500 mt-1 leading-tight">
              {agntid?.reason || 'AgntID SDK unconfigured; decision recorded independently.'}
            </p>
          </div>
        </div>
      </div>

      {/* Multi-Hop Causal Attack Path Reconstruction */}
      <div className="bg-[#111827] border border-gray-800 rounded-xl p-5 shadow-md">
        <div className="flex items-center justify-between pb-4 border-b border-gray-800">
          <div>
            <div className="flex items-center space-x-2">
              <Compass className="w-4 h-4 text-cyan-400" />
              <h3 className="text-sm font-semibold text-white">Reconstructed Causal Attack Paths (System 04)</h3>
            </div>
            <p className="text-xs text-gray-400 mt-0.5">
              Multi-hop DFS heuristic traversing R-GCN attribution, causality (t_k &ge; t_k-1), and structural risk multipliers.
            </p>
          </div>
          <span className="text-xs font-mono px-2.5 py-1 rounded bg-blue-500/10 text-blue-400 border border-blue-500/20">
            {paths.length} {paths.length === 1 ? 'path' : 'paths'} reconstructed
          </span>
        </div>

        {paths.length === 0 ? (
          <div className="py-12 text-center text-gray-500 text-xs">
            <AlertTriangle className="w-8 h-8 text-gray-600 mx-auto mb-2" />
            No causal attack paths reconstructed for this specific event step.
            <p className="text-[11px] text-gray-600 mt-1">
              Select an adversarial scenario or multi-step exfiltration sequence in Overview to inspect reconstructed paths.
            </p>
          </div>
        ) : (
          <div className="mt-4 grid grid-cols-1 lg:grid-cols-3 gap-6">
            {/* Path Selector Tabs */}
            <div className="lg:col-span-1 space-y-2">
              <span className="text-xs font-medium text-gray-400 block font-mono">Ranked Suspicious Trajectories</span>
              {paths.map((p, idx) => {
                const isSelected = activePathIndex === idx;
                return (
                  <div
                    key={idx}
                    onClick={() => setActivePathIndex(idx)}
                    className={`p-3 rounded-lg border cursor-pointer transition-all ${
                      isSelected
                        ? 'bg-red-950/40 border-red-500/60 ring-1 ring-red-500 shadow-md'
                        : 'bg-gray-900 border-gray-800 hover:border-gray-700'
                    }`}
                  >
                    <div className="flex items-center justify-between">
                      <span className="font-mono font-bold text-xs text-white">Path #{idx + 1}</span>
                      <span className="font-mono text-xs px-2 py-0.5 rounded bg-red-500/20 text-red-400 border border-red-500/30">
                        Score: {p.total_score.toFixed(3)}
                      </span>
                    </div>
                    <p className="text-[11px] text-gray-300 mt-1.5 font-mono truncate">{p.summary}</p>
                    <div className="mt-2 text-[10px] text-gray-500 font-mono flex items-center justify-between">
                      <span>{p.steps.length} causal hops</span>
                      <span>Conf: {(p.confidence * 100).toFixed(1)}%</span>
                    </div>
                  </div>
                );
              })}
            </div>

            {/* Path Step Sequence Table */}
            <div className="lg:col-span-2 bg-gray-900/60 rounded-xl border border-gray-800 p-4 flex flex-col">
              {selectedPath && (
                <>
                  <div className="flex items-center justify-between pb-3 border-b border-gray-800">
                    <div>
                      <h4 className="text-xs font-semibold text-white font-mono uppercase">
                        Causal Step Sequence ({selectedPath.steps.length} Hops)
                      </h4>
                      <p className="text-[11px] text-gray-400 mt-0.5 font-mono">{selectedPath.summary}</p>
                    </div>
                    <button
                      onClick={() => {
                        onSelectPath(selectedPath);
                        onNavigateToGraph();
                      }}
                      className="px-2.5 py-1 rounded bg-blue-600/30 hover:bg-blue-600 text-blue-300 hover:text-white text-[11px] font-mono border border-blue-500/40 transition-colors"
                    >
                      Highlight Path &rarr;
                    </button>
                  </div>

                  <div className="mt-3 flex-1 overflow-x-auto">
                    <table className="w-full text-left text-xs font-mono">
                      <thead className="bg-gray-900 text-gray-400 uppercase text-[10px]">
                        <tr>
                          <th className="py-2 px-2.5">Hop</th>
                          <th className="py-2 px-2.5">Source Entity</th>
                          <th className="py-2 px-2.5">Relation</th>
                          <th className="py-2 px-2.5">Target Entity</th>
                          <th className="py-2 px-2.5">Attribution (a_e)</th>
                          <th className="py-2 px-2.5">Multiplier</th>
                        </tr>
                      </thead>
                      <tbody className="divide-y divide-gray-800">
                        {selectedPath.steps.map((step, idx) => (
                          <tr key={idx} className="hover:bg-gray-800/40">
                            <td className="py-2 px-2.5 text-gray-500">#{idx + 1}</td>
                            <td className="py-2 px-2.5 text-blue-400 font-semibold">{step.source_key}</td>
                            <td className="py-2 px-2.5 text-purple-400">{step.relation_name}</td>
                            <td className="py-2 px-2.5 text-amber-300 font-semibold">{step.destination_key}</td>
                            <td className="py-2 px-2.5 text-gray-300">{step.attribution_score.toFixed(4)}</td>
                            <td className="py-2 px-2.5 text-rose-400 font-bold">{step.structural_multiplier}x</td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                </>
              )}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
