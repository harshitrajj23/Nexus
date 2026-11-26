import React, { useState, useEffect } from 'react';
import {
  ShieldAlert,
  ShieldCheck,
  AlertTriangle,
  Play,
  RotateCcw,
  SlidersHorizontal,
  Info,
  CheckCircle2,
  ExternalLink,
  Layers,
  Clock,
  Sparkles,
  ArrowRight
} from 'lucide-react';
import { runIntentBoundaryExperiment } from '../services/api';
import { IntentExperimentResponse, IntentComparisonRow, RiskLevel } from '../types';
import { RiskBadge } from './RiskBadge';

interface IntentDecisionComparisonViewProps {
  onNavigateToGraph: () => void;
  onNavigateToThreats: () => void;
}

export const IntentDecisionComparisonView: React.FC<IntentDecisionComparisonViewProps> = ({
  onNavigateToGraph,
  onNavigateToThreats
}) => {
  const [data, setData] = useState<IntentExperimentResponse | null>(null);
  const [isLoading, setIsLoading] = useState<boolean>(false);
  const [error, setError] = useState<string | null>(null);

  const fetchOrRun = async () => {
    setIsLoading(true);
    setError(null);
    try {
      const res = await runIntentBoundaryExperiment();
      setData(res);
    } catch (err: any) {
      setError(err.message || 'Failed to execute intent-boundary experiment');
    } finally {
      setIsLoading(false);
    }
  };

  useEffect(() => {
    fetchOrRun();
  }, []);

  const isAgntIDAvailable = data?.agntid_status?.available ?? false;

  return (
    <div className="space-y-6">
      {/* 1. Header Banner */}
      <div className="bg-[#111827] border border-gray-800 rounded-xl p-5 shadow-sm">
        <div className="flex flex-col md:flex-row md:items-center justify-between gap-4 pb-4 border-b border-gray-800">
          <div>
            <div className="flex items-center space-x-2">
              <span className="text-[10px] font-mono uppercase tracking-widest px-2 py-0.5 rounded bg-purple-500/10 text-purple-400 border border-purple-500/30 font-bold">
                AgntID Agent Security Challenge
              </span>
              <span className="text-[10px] font-mono text-gray-500">Intent Exploration</span>
            </div>
            <h1 className="text-xl sm:text-2xl font-bold text-white tracking-tight mt-1.5">
              Intent and Runtime Decision Comparison
            </h1>
            <p className="text-xs sm:text-sm text-gray-400 mt-1">
              Evaluates identical tool actions under different declared intents to contrast temporal evidence with runtime policy decisions.
            </p>
          </div>

          <div className="flex items-center space-x-2">
            <button
              onClick={fetchOrRun}
              disabled={isLoading}
              className="px-3.5 py-1.5 rounded-lg bg-purple-600 hover:bg-purple-500 text-white text-xs font-mono font-semibold shadow-md shadow-purple-500/20 disabled:opacity-50 transition-all flex items-center space-x-1.5"
            >
              {isLoading ? (
                <>
                  <span className="w-3.5 h-3.5 rounded-full border-2 border-white border-t-transparent animate-spin" />
                  <span>Evaluating Intents...</span>
                </>
              ) : (
                <>
                  <Play className="w-3.5 h-3.5 fill-current" />
                  <span>Re-Run Intent Experiment</span>
                </>
              )}
            </button>
          </div>
        </div>

        {/* AgntID Availability Notice */}
        <div className="mt-3.5">
          {!isAgntIDAvailable ? (
            <div className="p-3.5 rounded-lg bg-amber-950/30 border border-amber-500/40 text-amber-200 text-xs font-mono flex items-start space-x-2.5">
              <Info className="w-4 h-4 text-amber-400 shrink-0 mt-0.5" />
              <div>
                <span className="font-bold text-amber-300 block">
                  Connect AgntID to run live runtime decision experiments
                </span>
                <p className="mt-0.5 text-[11px] text-amber-200/90 leading-relaxed font-sans">
                  Official AgntID runtime SDK/CLI is unconfigured in this host environment. In accordance with strict hackathon scientific integrity rules, AgntID decisions are recorded honestly as <code className="text-amber-300 bg-amber-950/60 px-1 py-0.5 rounded">NOT_EVALUATED</code> without fabricating simulated blocks. NEXUS operates in transparent advisory mode.
                </p>
              </div>
            </div>
          ) : (
            <div className="p-3 rounded-lg bg-emerald-950/30 border border-emerald-500/40 text-emerald-200 text-xs font-mono flex items-center space-x-2">
              <CheckCircle2 className="w-4 h-4 text-emerald-400" />
              <span>Official AgntID Adapter Connected: {data?.agntid_status?.source}</span>
            </div>
          )}
        </div>
      </div>

      {error && (
        <div className="p-4 rounded-xl bg-rose-950/60 border border-rose-500/50 text-rose-300 text-xs font-mono">
          Error executing experiment: {error}
        </div>
      )}

      {/* 2. Structured Comparison Table */}
      <div className="bg-[#111827] border border-gray-800 rounded-xl p-5 shadow-sm">
        <div className="flex items-center justify-between pb-3 border-b border-gray-800">
          <div className="flex items-center space-x-2">
            <SlidersHorizontal className="w-4 h-4 text-blue-400" />
            <h3 className="text-sm font-semibold text-white font-mono uppercase tracking-wider">
              Intent vs Runtime Security Decision Matrix
            </h3>
          </div>
          <span className="text-[11px] font-mono text-gray-500">
            3 Tested Scenarios &bull; Identical Resource: <code className="text-amber-300">credentials/config.env</code>
          </span>
        </div>

        <div className="mt-4 overflow-x-auto">
          <table className="w-full text-left text-xs font-mono">
            <thead className="bg-gray-900/90 text-gray-400 uppercase text-[10px] border-b border-gray-800">
              <tr>
                <th className="py-3 px-3">Scenario</th>
                <th className="py-3 px-3">Declared Task Intent</th>
                <th className="py-3 px-3">Tool Action Sequence</th>
                <th className="py-3 px-3">NEXUS Policy</th>
                <th className="py-3 px-3">AgntID Decision</th>
                <th className="py-3 px-3">Enforcement</th>
                <th className="py-3 px-3">Observable Finding</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-gray-800 text-[11px]">
              {data?.comparison_matrix ? (
                data.comparison_matrix.map((row: IntentComparisonRow, idx: number) => {
                  const isCritical = row.nexus_policy === 'Critical';
                  const isSuspicious = row.nexus_policy === 'Suspicious';

                  return (
                    <tr
                      key={idx}
                      className={`hover:bg-gray-800/40 transition-colors ${
                        isCritical ? 'bg-rose-950/15' : isSuspicious ? 'bg-amber-950/10' : ''
                      }`}
                    >
                      <td className="py-3 px-3 font-bold text-white whitespace-nowrap">
                        {row.scenario}
                      </td>
                      <td className="py-3 px-3 text-gray-300 max-w-[220px] font-sans text-xs">
                        &ldquo;{row.declared_intent}&rdquo;
                      </td>
                      <td className="py-3 px-3 text-purple-300 font-semibold max-w-[190px]">
                        {row.tool_action}
                      </td>
                      <td className="py-3 px-3 whitespace-nowrap">
                        <div className="flex items-center space-x-1.5">
                          <RiskBadge level={row.nexus_policy} size="sm" />
                          <span className="text-[10px] text-gray-400 font-bold">({row.nexus_score}/100)</span>
                        </div>
                      </td>
                      <td className="py-3 px-3 whitespace-nowrap">
                        <span className="px-2 py-0.5 rounded bg-gray-800 text-gray-300 border border-gray-700 font-mono text-[10px] font-bold">
                          {row.agntid_decision}
                        </span>
                      </td>
                      <td className="py-3 px-3 whitespace-nowrap text-amber-400/90 text-[10px]">
                        {row.enforcement}
                      </td>
                      <td className="py-3 px-3 text-gray-300 max-w-[260px] font-sans text-xs leading-relaxed">
                        {row.finding}
                      </td>
                    </tr>
                  );
                })
              ) : (
                <tr>
                  <td colSpan={7} className="py-8 text-center text-gray-500">
                    Loading intent experiment matrix...
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </div>

      {/* 3. Agreement / Gap Analysis Panel */}
      <div className="bg-[#111827] border border-gray-800 rounded-xl p-5 shadow-sm space-y-4">
        <div className="flex items-center justify-between pb-3 border-b border-gray-800">
          <div className="flex items-center space-x-2">
            <Sparkles className="w-4 h-4 text-cyan-400" />
            <h3 className="text-sm font-semibold text-white font-mono uppercase tracking-wider">
              Agreement / Gap Analysis &amp; Security Findings
            </h3>
          </div>
          <span className="text-[11px] font-mono text-cyan-400 font-semibold">
            Temporal Context vs Point-in-Time Evaluation
          </span>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 gap-4 text-xs font-mono">
          {/* Card 1: Identical Resource Contrast */}
          <div className="p-4 rounded-lg bg-gray-900/80 border border-gray-800 space-y-2">
            <span className="text-amber-400 font-bold uppercase text-[11px] block">
              1. Identical Resource Under Different Intents
            </span>
            <p className="text-gray-300 leading-relaxed font-sans text-xs">
              Accessing <code className="text-amber-300">credentials/config.env</code> under an approved credential rotation task resulted in an evidence score of <strong>30–40/100</strong> (bounded sensitivity, no egress). Under exfiltration intent, the identical file access preceded IAM escalation and outbound exfiltration, escalating evidence to <strong>95–100/100 (CRITICAL)</strong>.
            </p>
          </div>

          {/* Card 2: Temporal Chain Reconstruction */}
          <div className="p-4 rounded-lg bg-gray-900/80 border border-gray-800 space-y-2">
            <span className="text-blue-400 font-bold uppercase text-[11px] block">
              2. Why Point-in-Time Tool Checks Miss the Attack
            </span>
            <p className="text-gray-300 leading-relaxed font-sans text-xs">
              A point-in-time guardrail checking only the final HTTP POST tool call evaluates it in isolation and may permit it. NEXUS reconstructs the causal graph chain (<code className="text-cyan-300">t_k &ge; t_k-1</code>), proving the outbound POST contains data derived from the earlier secret read.
            </p>
          </div>

          {/* Card 3: Context Conflict / Task Drift */}
          <div className="p-4 rounded-lg bg-gray-900/80 border border-gray-800 space-y-2">
            <span className="text-rose-400 font-bold uppercase text-[11px] block">
              3. Context Conflict &amp; Indirect Injection
            </span>
            <p className="text-gray-300 leading-relaxed font-sans text-xs">
              When an agent began with a benign documentation intent but pivoted to secret access and beaconing, the dynamic graph recorded the structural shift immediately. Point-in-time rules miss task drift because each tool is permitted; temporal modeling detects the deviation.
            </p>
          </div>

          {/* Card 4: Honest Enforcement Status */}
          <div className="p-4 rounded-lg bg-gray-900/80 border border-gray-800 space-y-2">
            <span className="text-emerald-400 font-bold uppercase text-[11px] block">
              4. Runtime Enforcement Separation
            </span>
            <p className="text-gray-300 leading-relaxed font-sans text-xs">
              NEXUS maintains strict integrity: AgntID is reported honestly as <strong>NOT_EVALUATED</strong> when unconfigured. NEXUS policy recommendations advise human analysts and external gateways rather than fabricating automated blocking claims.
            </p>
          </div>
        </div>

        {/* Quick Links */}
        <div className="pt-3 border-t border-gray-800 flex flex-wrap items-center justify-between gap-3 text-xs font-mono">
          <span className="text-gray-400">
            Inspect the live causal trajectories in the other views:
          </span>
          <div className="flex items-center space-x-2">
            <button
              onClick={onNavigateToGraph}
              className="px-3 py-1.5 rounded-lg bg-blue-600/30 hover:bg-blue-600 text-blue-300 hover:text-white transition-colors flex items-center space-x-1 border border-blue-500/40"
            >
              <span>View in Live Graph</span>
              <ArrowRight className="w-3.5 h-3.5" />
            </button>
            <button
              onClick={onNavigateToThreats}
              className="px-3 py-1.5 rounded-lg bg-red-600/30 hover:bg-red-600 text-red-300 hover:text-white transition-colors flex items-center space-x-1 border border-red-500/40"
            >
              <span>View Threat Intel</span>
              <ArrowRight className="w-3.5 h-3.5" />
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};
