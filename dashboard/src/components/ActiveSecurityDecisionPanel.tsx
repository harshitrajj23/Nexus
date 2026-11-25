import React from 'react';
import {
  ShieldAlert,
  ShieldCheck,
  AlertTriangle,
  Lock,
  Key,
  Globe,
  Bot,
  ArrowRight,
  Info,
  CheckCircle2,
  AlertOctagon,
  Eye,
  SlidersHorizontal
} from 'lucide-react';
import { HybridAssessment, RiskLevel } from '../types';
import { RiskBadge } from './RiskBadge';

interface ActiveSecurityDecisionPanelProps {
  assessment: HybridAssessment | null;
  onNavigateToGraph: () => void;
  onNavigateToThreats: () => void;
}

export const ActiveSecurityDecisionPanel: React.FC<ActiveSecurityDecisionPanelProps> = ({
  assessment,
  onNavigateToGraph,
  onNavigateToThreats,
}) => {
  // Fallback benign state if no events/assessments exist yet
  const policyRisk: RiskLevel = assessment?.policy_risk || 'Normal';
  const evidenceScore: number = assessment?.evidence_score ?? 0;
  const recommendedAction: string = assessment?.recommended_action || 'Continue monitoring';
  const actualEnforcement: string = assessment?.actual_enforcement_status || 'NOT_ENFORCED (No active blocking engine attached; policy is advisory)';
  const modelPred: string = assessment?.model_prediction || 'Normal';
  const modelConf: number = assessment?.model_confidence ?? 0.95;

  const expl = assessment?.attack_explanation;
  const agentName = expl?.affected_agent || 'agent_analyst_01';
  const sensitiveRes = expl?.sensitive_resource || 'None (Benign)';
  const privilegeRole = expl?.privilege_or_role || 'Standard (User)';
  const externalDest = expl?.external_destination || 'Internal repo / DB';
  const narrative = expl?.narrative || 'Agent operating within normal security boundaries. No credential access, privilege escalation, or unauthorized external egress detected.';

  const actionColors: Record<RiskLevel, { bg: string; border: string; text: string; icon: React.ReactNode }> = {
    Normal: {
      bg: 'bg-emerald-950/40',
      border: 'border-emerald-500/40',
      text: 'text-emerald-300',
      icon: <CheckCircle2 className="w-5 h-5 text-emerald-400" />
    },
    Suspicious: {
      bg: 'bg-amber-950/40',
      border: 'border-amber-500/40',
      text: 'text-amber-300',
      icon: <AlertTriangle className="w-5 h-5 text-amber-400" />
    },
    HighRisk: {
      bg: 'bg-orange-950/40',
      border: 'border-orange-500/40',
      text: 'text-orange-300',
      icon: <AlertOctagon className="w-5 h-5 text-orange-400" />
    },
    Critical: {
      bg: 'bg-rose-950/50',
      border: 'border-rose-500/60 ring-1 ring-rose-500/50',
      text: 'text-rose-200',
      icon: <ShieldAlert className="w-5 h-5 text-rose-400 animate-pulse" />
    }
  };

  const actionStyle = actionColors[policyRisk] || actionColors.Normal;

  return (
    <div className={`rounded-xl border ${actionStyle.border} ${actionStyle.bg} p-5 shadow-xl transition-all`}>
      {/* Header: Badge, Title & Recommended Action */}
      <div className="flex flex-col lg:flex-row lg:items-center justify-between gap-4 pb-4 border-b border-gray-800">
        <div>
          <div className="flex items-center space-x-2.5">
            <span className="text-[11px] font-mono uppercase tracking-wider px-2 py-0.5 rounded bg-blue-500/10 text-cyan-400 border border-cyan-500/30 font-bold">
              Active Security Decision
            </span>
            <RiskBadge level={policyRisk} size="lg" />
            <span className="text-xs font-mono px-2 py-0.5 rounded bg-gray-900 text-gray-400 border border-gray-800">
              Score: <strong className="text-white">{evidenceScore}</strong>/100
            </span>
          </div>
          <h2 className="text-lg font-bold text-white mt-1.5 flex items-center space-x-2">
            <span>Policy Recommendation:</span>
            <span className={`underline decoration-2 ${actionStyle.text}`}>{recommendedAction}</span>
          </h2>
        </div>

        {/* Quick Inspector Actions */}
        <div className="flex items-center space-x-2.5">
          <button
            onClick={onNavigateToThreats}
            className="px-3 py-1.5 rounded-lg bg-gray-900 hover:bg-gray-800 text-gray-200 text-xs font-mono border border-gray-700 transition-colors flex items-center space-x-1.5"
          >
            <Eye className="w-3.5 h-3.5" />
            <span>Threat Intel</span>
          </button>
          <button
            onClick={onNavigateToGraph}
            className="px-3.5 py-1.5 rounded-lg bg-blue-600 hover:bg-blue-500 text-white text-xs font-semibold font-mono shadow-md shadow-blue-500/20 transition-all flex items-center space-x-1.5"
          >
            <span>Live Graph</span>
            <ArrowRight className="w-3.5 h-3.5" />
          </button>
        </div>
      </div>

      {/* Grid: 4 Core Security Evidence Signals */}
      <div className="mt-4 grid grid-cols-2 md:grid-cols-4 gap-3 text-xs font-mono">
        {/* Agent */}
        <div className="bg-[#111827]/90 p-3 rounded-lg border border-gray-800">
          <div className="flex items-center space-x-1.5 text-gray-400 text-[11px] mb-1">
            <Bot className="w-3.5 h-3.5 text-blue-400" />
            <span className="uppercase">Affected Agent</span>
          </div>
          <span className="text-blue-300 font-bold text-sm truncate block" title={agentName}>
            {agentName}
          </span>
        </div>

        {/* Sensitive Resource */}
        <div className={`p-3 rounded-lg border ${sensitiveRes.includes('None') ? 'bg-[#111827]/90 border-gray-800' : 'bg-amber-950/40 border-amber-500/40 text-amber-200'}`}>
          <div className="flex items-center space-x-1.5 text-gray-400 text-[11px] mb-1">
            <Lock className="w-3.5 h-3.5 text-amber-400" />
            <span className="uppercase">Sensitive Resource</span>
          </div>
          <span className={`font-bold text-sm truncate block ${sensitiveRes.includes('None') ? 'text-gray-400 font-normal' : 'text-amber-300'}`} title={sensitiveRes}>
            {sensitiveRes}
          </span>
        </div>

        {/* Privilege / Role */}
        <div className={`p-3 rounded-lg border ${privilegeRole.includes('Standard') ? 'bg-[#111827]/90 border-gray-800' : 'bg-rose-950/40 border-rose-500/40 text-rose-200'}`}>
          <div className="flex items-center space-x-1.5 text-gray-400 text-[11px] mb-1">
            <Key className="w-3.5 h-3.5 text-rose-400" />
            <span className="uppercase">Privilege / Role</span>
          </div>
          <span className={`font-bold text-sm truncate block ${privilegeRole.includes('Standard') ? 'text-gray-400 font-normal' : 'text-rose-300'}`} title={privilegeRole}>
            {privilegeRole}
          </span>
        </div>

        {/* External Destination */}
        <div className={`p-3 rounded-lg border ${externalDest.includes('Internal') ? 'bg-[#111827]/90 border-gray-800' : 'bg-red-950/50 border-red-500/50 text-red-200 ring-1 ring-red-500/40'}`}>
          <div className="flex items-center space-x-1.5 text-gray-400 text-[11px] mb-1">
            <Globe className="w-3.5 h-3.5 text-red-400" />
            <span className="uppercase">External Egress</span>
          </div>
          <span className={`font-bold text-sm truncate block ${externalDest.includes('Internal') ? 'text-gray-400 font-normal' : 'text-red-300'}`} title={externalDest}>
            {externalDest}
          </span>
        </div>
      </div>

      {/* Narrative Explanation & Model vs Evidence Breakdown */}
      <div className="mt-4 grid grid-cols-1 lg:grid-cols-3 gap-4 font-mono text-xs">
        {/* Narrative Box */}
        <div className="lg:col-span-2 bg-[#111827]/95 p-3.5 rounded-lg border border-gray-800 flex flex-col justify-between">
          <div>
            <div className="flex items-center space-x-1.5 text-cyan-400 text-[11px] font-bold mb-1">
              <Info className="w-3.5 h-3.5" />
              <span>WHY THIS WAS FLAGGED (EXPLAINABLE ATTACK NARRATIVE)</span>
            </div>
            <p className="text-gray-200 leading-relaxed text-xs mt-1">
              {narrative}
            </p>
          </div>

          {expl?.evidence_signals && expl.evidence_signals.length > 0 && (
            <div className="mt-2.5 pt-2 border-t border-gray-800/80 flex flex-wrap gap-1.5">
              {expl.evidence_signals.map((sig, idx) => (
                <span
                  key={idx}
                  className="px-2 py-0.5 rounded text-[10px] bg-gray-900 border border-gray-700 text-gray-300"
                  title={sig.reason}
                >
                  +{sig.points} {sig.name}
                </span>
              ))}
            </div>
          )}
        </div>

        {/* Model vs Evidence Comparison */}
        <div className="lg:col-span-1 bg-[#111827]/95 p-3.5 rounded-lg border border-gray-800 flex flex-col justify-between space-y-2">
          <div>
            <span className="text-gray-400 text-[11px] font-bold block mb-1">
              NEURAL MODEL VS EVIDENCE
            </span>
            <div className="space-y-1 text-[11px]">
              <div className="flex justify-between">
                <span className="text-gray-500">Neural Model Output:</span>
                <span className="text-purple-300 font-bold">{modelPred} ({(modelConf * 100).toFixed(1)}%)</span>
              </div>
              <div className="flex justify-between">
                <span className="text-gray-500">Deterministic Evidence:</span>
                <span className="text-cyan-300 font-bold">{evidenceScore}/100</span>
              </div>
              <div className="flex justify-between">
                <span className="text-gray-500">Final Policy Decision:</span>
                <span className="text-amber-300 font-bold">{policyRisk}</span>
              </div>
            </div>
          </div>

          {/* Honest Enforcement Notice */}
          <div className="pt-2 border-t border-gray-800 text-[10px] text-gray-400">
            <span className="text-gray-500 block">Actual Enforcement Status:</span>
            <span className="text-gray-300 font-semibold">{actualEnforcement}</span>
          </div>
        </div>
      </div>
    </div>
  );
};
