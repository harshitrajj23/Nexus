import React, { useState } from 'react';
import {
  FlaskConical,
  Play,
  RotateCcw,
  CheckCircle2,
  ShieldAlert,
  ArrowRight,
  Zap,
  Activity,
  FileCode,
  Shield,
  Layers,
  Clock,
  Sparkles
} from 'lucide-react';
import { runExperiment, resetGraph } from '../services/api';
import { ExperimentResponse, RiskLevel } from '../types';
import { RiskBadge } from './RiskBadge';

interface ExperimentLabViewProps {
  onExperimentComplete: (result: ExperimentResponse) => void;
  onResetGraph: () => void;
  onNavigateToGraph: () => void;
  onNavigateToThreats: () => void;
}

export const ExperimentLabView: React.FC<ExperimentLabViewProps> = ({
  onExperimentComplete,
  onResetGraph,
  onNavigateToGraph,
  onNavigateToThreats,
}) => {
  const [runningScenario, setRunningScenario] = useState<string | null>(null);
  const [result, setResult] = useState<ExperimentResponse | null>(null);
  const [error, setError] = useState<string | null>(null);

  const handleRun = async (scenario: 'normal' | 'adversarial_exfil' | 'unseen_attack') => {
    setRunningScenario(scenario);
    setError(null);
    try {
      const resp = await runExperiment(scenario);
      setResult(resp);
      onExperimentComplete(resp);
    } catch (err: any) {
      setError(err.message || 'Failed to run experiment scenario');
    } finally {
      setRunningScenario(null);
    }
  };

  const handleReset = async () => {
    try {
      await resetGraph();
      setResult(null);
      onResetGraph();
    } catch (err: any) {
      setError(err.message || 'Failed to reset graph');
    }
  };

  return (
    <div className="space-y-6">
      {/* Top Banner */}
      <div className="bg-gradient-to-r from-purple-900/40 via-blue-900/20 to-gray-900 border border-purple-500/30 rounded-xl p-5 shadow-md">
        <div className="flex flex-col md:flex-row md:items-center justify-between gap-4">
          <div className="flex items-center space-x-3">
            <div className="w-10 h-10 rounded-lg bg-purple-500/20 border border-purple-500/40 flex items-center justify-center text-purple-400">
              <FlaskConical className="w-5 h-5" />
            </div>
            <div>
              <h2 className="text-base font-semibold text-white">System 05 & 06: Competition Experiment Lab</h2>
              <p className="text-xs text-gray-300 mt-0.5">
                Executes sandboxed agent workflows in isolated temporary environments and streams live events through NEXUS C++17 LibTorch inference and AgntID evaluation.
              </p>
            </div>
          </div>

          <div className="flex items-center space-x-2">
            <button
              onClick={handleReset}
              className="px-3.5 py-1.5 rounded-lg bg-gray-800 hover:bg-gray-700 text-gray-300 hover:text-white text-xs font-mono border border-gray-700 transition-colors flex items-center space-x-1.5"
            >
              <RotateCcw className="w-3.5 h-3.5" />
              <span>Reset Graph</span>
            </button>
          </div>
        </div>
      </div>

      {/* Scenario Triggers Grid */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
        {/* Scenario 1: Experiment A (Normal) */}
        <div className="bg-[#111827] border border-gray-800 hover:border-emerald-500/40 rounded-xl p-4 flex flex-col justify-between transition-all">
          <div>
            <div className="flex items-center justify-between">
              <span className="text-xs font-mono font-bold px-2 py-0.5 rounded bg-emerald-500/10 text-emerald-400 border border-emerald-500/30">
                Experiment A
              </span>
              <span className="text-[10px] text-gray-500 font-mono">4 Benign Steps</span>
            </div>
            <h3 className="text-sm font-semibold text-white mt-2">Normal Agent Behaviour</h3>
            <p className="text-xs text-gray-400 mt-1 leading-relaxed">
              Benign repository source code inspection, database catalog query, audit report writing, and unit test execution.
            </p>
          </div>

          <button
            onClick={() => handleRun('normal')}
            disabled={runningScenario !== null}
            className="mt-4 w-full py-2 px-3 rounded-lg bg-emerald-600/80 hover:bg-emerald-500 text-white text-xs font-semibold font-mono disabled:opacity-40 transition-all flex items-center justify-center space-x-1.5 shadow-md shadow-emerald-600/20"
          >
            {runningScenario === 'normal' ? (
              <span className="flex items-center space-x-1.5">
                <span className="w-3 h-3 rounded-full border-2 border-white border-t-transparent animate-spin" />
                <span>Executing in Sandbox...</span>
              </span>
            ) : (
              <span className="flex items-center space-x-1.5">
                <Play className="w-3.5 h-3.5 fill-current" />
                <span>Run Normal Scenario</span>
              </span>
            )}
          </button>
        </div>

        {/* Scenario 2: Experiment B (Adversarial Exfiltration) */}
        <div className="bg-[#111827] border border-gray-800 hover:border-rose-500/40 rounded-xl p-4 flex flex-col justify-between transition-all">
          <div>
            <div className="flex items-center justify-between">
              <span className="text-xs font-mono font-bold px-2 py-0.5 rounded bg-rose-500/10 text-rose-400 border border-rose-500/30">
                Experiment B
              </span>
              <span className="text-[10px] text-gray-500 font-mono">4 Adversarial Steps</span>
            </div>
            <h3 className="text-sm font-semibold text-white mt-2">Multi-Step Attack Vector</h3>
            <p className="text-xs text-gray-400 mt-1 leading-relaxed">
              Benign recon pivoting to secret reading (<code className="text-amber-300">config.env</code>), IAM privilege escalation (<code className="text-rose-300">ADMIN</code>), and mock exfiltration.
            </p>
          </div>

          <button
            onClick={() => handleRun('adversarial_exfil')}
            disabled={runningScenario !== null}
            className="mt-4 w-full py-2 px-3 rounded-lg bg-rose-600/80 hover:bg-rose-500 text-white text-xs font-semibold font-mono disabled:opacity-40 transition-all flex items-center justify-center space-x-1.5 shadow-md shadow-rose-600/20"
          >
            {runningScenario === 'adversarial_exfil' ? (
              <span className="flex items-center space-x-1.5">
                <span className="w-3 h-3 rounded-full border-2 border-white border-t-transparent animate-spin" />
                <span>Executing Attack in Sandbox...</span>
              </span>
            ) : (
              <span className="flex items-center space-x-1.5">
                <Play className="w-3.5 h-3.5 fill-current" />
                <span>Run Attack Scenario</span>
              </span>
            )}
          </button>
        </div>

        {/* Scenario 3: Experiment C (Unseen Attack Combination) */}
        <div className="bg-[#111827] border border-gray-800 hover:border-purple-500/40 rounded-xl p-4 flex flex-col justify-between transition-all">
          <div>
            <div className="flex items-center justify-between">
              <span className="text-xs font-mono font-bold px-2 py-0.5 rounded bg-purple-500/10 text-purple-400 border border-purple-500/30">
                Experiment C
              </span>
              <span className="text-[10px] text-gray-500 font-mono">Held-Out Template</span>
            </div>
            <h3 className="text-sm font-semibold text-white mt-2">Unseen Attack Combination</h3>
            <p className="text-xs text-gray-400 mt-1 leading-relaxed">
              Withheld attack template testing out-of-distribution generalization: indirect prompt injection pivoting to cloud IMDS metadata and supply-chain backdoor.
            </p>
          </div>

          <button
            onClick={() => handleRun('unseen_attack')}
            disabled={runningScenario !== null}
            className="mt-4 w-full py-2 px-3 rounded-lg bg-purple-600/80 hover:bg-purple-500 text-white text-xs font-semibold font-mono disabled:opacity-40 transition-all flex items-center justify-center space-x-1.5 shadow-md shadow-purple-600/20"
          >
            {runningScenario === 'unseen_attack' ? (
              <span className="flex items-center space-x-1.5">
                <span className="w-3 h-3 rounded-full border-2 border-white border-t-transparent animate-spin" />
                <span>Evaluating Generalization...</span>
              </span>
            ) : (
              <span className="flex items-center space-x-1.5">
                <Sparkles className="w-3.5 h-3.5 fill-current" />
                <span>Run Unseen Experiment</span>
              </span>
            )}
          </button>
        </div>
      </div>

      {error && (
        <div className="p-4 rounded-xl bg-rose-950/60 border border-rose-500/50 text-rose-300 text-xs font-mono">
          Error executing experiment: {error}
        </div>
      )}

      {/* Experiment Results Container */}
      {result && (
        <div className="bg-[#111827] border border-gray-800 rounded-xl p-5 shadow-lg space-y-4">
          <div className="flex flex-col sm:flex-row sm:items-center justify-between pb-4 border-b border-gray-800 gap-3">
            <div>
              <div className="flex items-center space-x-2">
                <CheckCircle2 className="w-5 h-5 text-emerald-400" />
                <h3 className="text-sm font-semibold text-white font-mono uppercase">
                  Experiment Result: {result.scenario} ({result.total_steps} Steps Executed)
                </h3>
              </div>
              <p className="text-xs text-gray-400 mt-0.5 font-mono">
                Agent ID: <span className="text-blue-400">{result.agent_id}</span> &bull; Peak Evidence Score: <span className="text-white font-bold">{result.summary.peak_evidence_score ?? 0}/100</span> &bull; Neural Model: <span className="text-cyan-400 font-bold">{result.summary.highest_risk_detected}</span>
              </p>
            </div>

            <div className="flex items-center space-x-2">
              <RiskBadge level={result.summary.policy_risk_detected || result.summary.highest_risk_detected} size="md" />

              <button
                onClick={onNavigateToGraph}
                className="px-3 py-1.5 rounded-lg bg-blue-600/30 hover:bg-blue-600 text-blue-300 hover:text-white text-xs font-mono border border-blue-500/40 transition-colors flex items-center space-x-1.5"
              >
                <span>View in Live Graph</span>
                <ArrowRight className="w-3.5 h-3.5" />
              </button>

              <button
                onClick={onNavigateToThreats}
                className="px-3 py-1.5 rounded-lg bg-red-600/30 hover:bg-red-600 text-red-300 hover:text-white text-xs font-mono border border-red-500/40 transition-colors flex items-center space-x-1.5"
              >
                <span>Threat Intel Details</span>
                <ArrowRight className="w-3.5 h-3.5" />
              </button>
            </div>
          </div>

          {/* Policy Recommendation & Narrative Summary */}
          {result.summary.recommended_action && (
            <div className="bg-gray-900/90 border border-red-500/30 rounded-lg p-3.5 font-mono text-xs">
              <div className="flex items-center justify-between">
                <span className="text-[10px] uppercase font-bold text-red-400 tracking-wider">
                  Recommended Policy Action
                </span>
                <span className="text-[10px] text-gray-500">
                  Enforcement: {result.summary.actual_enforcement_status || 'NOT_ENFORCED'}
                </span>
              </div>
              <div className="mt-1 font-bold text-white text-sm">
                {result.summary.recommended_action}
              </div>
              {(result.summary as any).score_headline && (
                <div className="mt-1.5 text-[11px] text-amber-300 font-semibold">
                  {(result.summary as any).score_headline} ({(result.summary as any).timestamp_label || 'replay timestamp (pinned)'})
                </div>
              )}
              {result.summary.attack_explanation?.narrative && (
                <p className="mt-1.5 text-xs text-gray-300 font-sans leading-relaxed">
                  {result.summary.attack_explanation.narrative}
                </p>
              )}
            </div>
          )}

          {/* Step-by-Step Execution Cards */}
          <div className="space-y-3">
            {result.steps.map((step, idx) => {
              const e = step.event;
              const p = step.nexus_prediction;
              const agnt = step.agntid_decision;
              const hybrid = step.hybrid_assessment;
              const signals = hybrid?.evidence_signals || step.evidence_signals || [];

              return (
                <div
                  key={idx}
                  className="bg-gray-900/90 border border-gray-800 rounded-lg p-3.5 font-mono text-xs hover:border-gray-700 transition-colors"
                >
                  <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-2 pb-2 border-b border-gray-800">
                    <div className="flex items-center space-x-2">
                      <span className="w-6 h-6 rounded-full bg-gray-800 flex items-center justify-center text-gray-400 font-bold text-[11px]">
                        {idx + 1}
                      </span>
                      <div>
                        <span className="font-bold text-gray-200 block">
                          [{e.tool}] {e.action} &rarr; <span className="text-amber-300">{e.resource}</span>
                        </span>
                        <span className="text-[10px] text-gray-500 block">
                          {(step as any).timestamp_label || 'replay timestamp (pinned)'}: {e.timestamp}
                        </span>
                      </div>
                    </div>
                    <div className="flex items-center space-x-2">
                      {hybrid && (
                        <div className="flex items-center space-x-1 px-2 py-0.5 rounded bg-gray-800 border border-gray-750">
                          <span className="text-[10px] text-gray-400">Score:</span>
                          <span className="text-[11px] font-bold text-white">{hybrid.evidence_score}/100</span>
                        </div>
                      )}
                      <RiskBadge level={hybrid?.policy_risk || p.class_name} size="sm" />
                      <span className="text-[10px] text-cyan-400 hidden sm:inline">Model: {p.class_name} ({(p.confidence * 100).toFixed(0)}%)</span>
                      <span className="text-[11px] text-gray-400">{step.latency_ms.toFixed(1)}ms</span>
                    </div>
                  </div>

                  {/* Signals pills */}
                  {signals.length > 0 && (
                    <div className="mt-2 flex flex-wrap gap-1.5">
                      {signals.map((sig) => (
                        <span key={sig.id} className="text-[10px] font-mono px-2 py-0.5 rounded bg-amber-500/15 text-amber-300 border border-amber-500/30">
                          +{sig.points} {sig.name}
                        </span>
                      ))}
                    </div>
                  )}

                  <div className="mt-2.5 grid grid-cols-1 sm:grid-cols-3 gap-2 text-[11px]">
                    <div className="bg-gray-950 p-2 rounded border border-gray-850">
                      <span className="text-gray-500 block text-[10px] uppercase">Permission & Intent</span>
                      <span className="text-rose-400 font-semibold">{e.permission}</span>
                      <span className="text-gray-600 block text-[9px] truncate">{e.intent_id}</span>
                    </div>

                    <div className="bg-gray-950 p-2 rounded border border-gray-850">
                      <span className="text-gray-500 block text-[10px] uppercase">Recommended Action</span>
                      <span className="text-amber-300 font-semibold truncate block">
                        {hybrid?.recommended_action || step.recommended_action || 'Continue monitoring'}
                      </span>
                      <span className="text-gray-500 block text-[9px]">Status: NOT_ENFORCED</span>
                    </div>

                    <div className="bg-gray-950 p-2 rounded border border-gray-850">
                      <span className="text-gray-500 block text-[10px] uppercase">AgntID Decision</span>
                      <span className="text-gray-300 font-semibold">{agnt.decision}</span>
                      <span className="text-gray-500 block text-[9px]">(Enforced: {agnt.enforced ? 'true' : 'false'})</span>
                    </div>
                  </div>

                  {step.attack_paths.length > 0 && (
                    <div className="mt-2 pt-2 border-t border-gray-800 flex items-center justify-between text-[11px] text-rose-400">
                      <span>&bull; {step.attack_paths.length} causal attack paths reconstructed</span>
                      <span className="text-gray-400 text-[10px]">
                        Top Score: {step.attack_paths[0].total_score.toFixed(3)}
                      </span>
                    </div>
                  )}
                </div>
              );
            })}
          </div>
        </div>
      )}
    </div>
  );
};
