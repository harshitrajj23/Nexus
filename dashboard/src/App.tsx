import React, { useState, useEffect, useCallback } from 'react';
import { Navbar, TabType } from './components/Navbar';
import { OverviewView, LiveEventItem } from './components/OverviewView';
import { LiveGraphView } from './components/LiveGraphView';
import { ThreatIntelView } from './components/ThreatIntelView';
import { ModelIntelView } from './components/ModelIntelView';
import { ExperimentLabView } from './components/ExperimentLabView';
import { IntentDecisionComparisonView } from './components/IntentDecisionComparisonView';
import {
  getHealth,
  getGraphSnapshot,
  resetGraph
} from './services/api';
import {
  HealthResponse,
  GraphSnapshot,
  AttackPath,
  ExperimentResponse
} from './types';

export const App: React.FC = () => {
  const [activeTab, setActiveTab] = useState<TabType>('overview');
  const [health, setHealth] = useState<HealthResponse | null>(null);
  const [snapshot, setSnapshot] = useState<GraphSnapshot>({
    status: 'ok',
    nodes: [],
    edges: []
  });
  const [events, setEvents] = useState<LiveEventItem[]>([]);
  const [selectedEvent, setSelectedEvent] = useState<LiveEventItem | null>(null);
  const [activeAttackPath, setActiveAttackPath] = useState<AttackPath | null>(null);

  // Refresh health & graph state
  const refreshData = useCallback(async () => {
    try {
      const h = await getHealth();
      setHealth(h);
    } catch (e) {
      // Backend starting or offline
    }

    try {
      const snap = await getGraphSnapshot();
      setSnapshot(snap);
      if (snap.active_attack_path && !activeAttackPath) {
        setActiveAttackPath(snap.active_attack_path);
      }
      setEvents((currentEvents) => {
        if (currentEvents.length === 0 && snap.latest_hybrid_assessment) {
          const initialEvent: LiveEventItem = {
            event: {
              event_id: snap.latest_hybrid_assessment.attack_explanation?.chronological_event_ids?.[0] || 'EVT-INIT',
              agent_id: snap.latest_hybrid_assessment.attack_explanation?.affected_agent || 'agent-01',
              timestamp: Math.floor(Date.now() / 1000),
              action: 'SECURITY_STATE_RESTORED',
              tool: 'nexus_engine',
              resource: snap.latest_hybrid_assessment.attack_explanation?.sensitive_resource || 'runtime_state',
              permission: snap.latest_hybrid_assessment.attack_explanation?.privilege_or_role || 'ADMIN',
              intent_id: 'SYSTEM_STATE',
              status: 'SUCCESS',
              label: snap.latest_hybrid_assessment.policy_risk,
              is_synthetic: true,
            },
            prediction: {
              class_name: snap.latest_hybrid_assessment.model_prediction,
              confidence: snap.latest_hybrid_assessment.model_confidence,
              class_index: 3,
              probabilities: [0.05, 0.1, 0.15, 0.7],
              logits: [-1.2, -0.5, 0.2, 2.1],
            },
            latency_ms: 1.2,
            agntid: {
              agntid_available: false,
              decision: 'NOT_EVALUATED',
              action_blocked: false,
              enforced: false,
              reason: 'Restored from C++ graph engine snapshot',
            },
            attack_paths: snap.top_paths || (snap.active_attack_path ? [snap.active_attack_path] : []),
            hybrid: snap.latest_hybrid_assessment,
          };
          setSelectedEvent(initialEvent);
          return [initialEvent];
        }
        return currentEvents;
      });
    } catch (e) {
      // Fallback
    }
  }, [activeAttackPath]);

  useEffect(() => {
    refreshData();
    const interval = setInterval(refreshData, 4000);
    return () => clearInterval(interval);
  }, [refreshData]);

  // Handle experiment completion
  const handleExperimentComplete = (resp: ExperimentResponse) => {
    const newItems: LiveEventItem[] = resp.steps.map(s => ({
      event: s.event,
      prediction: s.nexus_prediction,
      latency_ms: s.latency_ms,
      agntid: s.agntid_decision,
      attack_paths: s.attack_paths,
      hybrid: s.hybrid_assessment
    }));

    setEvents(prev => [...prev, ...newItems]);

    if (resp.summary.active_attack_path) {
      setActiveAttackPath(resp.summary.active_attack_path);
    }

    if (newItems.length > 0) {
      setSelectedEvent(newItems[newItems.length - 1]);
    }

    refreshData();
  };

  const handleResetGraph = async () => {
    try {
      await resetGraph();
      setSnapshot({ status: 'ok', nodes: [], edges: [] });
      setEvents([]);
      setSelectedEvent(null);
      setActiveAttackPath(null);
      refreshData();
    } catch (e) {
      console.error('Reset failed:', e);
    }
  };

  return (
    <div className="min-h-screen bg-[#0B0F19] text-gray-100 flex flex-col font-sans">
      <Navbar
        activeTab={activeTab}
        setActiveTab={setActiveTab}
        health={health}
        onResetGraph={handleResetGraph}
      />

      <main className="flex-1 max-w-7xl w-full mx-auto px-4 sm:px-6 lg:px-8 py-6">
        {activeTab === 'overview' && (
          <OverviewView
            events={events}
            nodeCount={snapshot.nodes?.length || 0}
            edgeCount={snapshot.edges?.length || 0}
            onSelectEvent={setSelectedEvent}
            onNavigateToGraph={() => setActiveTab('graph')}
            onNavigateToThreats={() => setActiveTab('threats')}
            onNavigateToLab={() => setActiveTab('lab')}
            onNavigateToIntent={() => setActiveTab('intent')}
          />
        )}

        {activeTab === 'graph' && (
          <LiveGraphView
            nodes={snapshot.nodes || []}
            edges={snapshot.edges || []}
            activeAttackPath={activeAttackPath}
            onRefreshGraph={refreshData}
          />
        )}

        {activeTab === 'threats' && (
          <ThreatIntelView
            selectedEvent={selectedEvent}
            allEvents={events}
            onSelectPath={(path) => setActiveAttackPath(path)}
            onNavigateToGraph={() => setActiveTab('graph')}
          />
        )}

        {activeTab === 'model' && <ModelIntelView />}

        {activeTab === 'lab' && (
          <ExperimentLabView
            onExperimentComplete={handleExperimentComplete}
            onResetGraph={handleResetGraph}
            onNavigateToGraph={() => setActiveTab('graph')}
            onNavigateToThreats={() => setActiveTab('threats')}
          />
        )}

        {activeTab === 'intent' && (
          <IntentDecisionComparisonView
            onNavigateToGraph={() => setActiveTab('graph')}
            onNavigateToThreats={() => setActiveTab('threats')}
          />
        )}
      </main>

      <footer className="bg-[#111827] border-t border-gray-800/80 py-4 text-center text-xs text-gray-500 font-mono">
        NEXUS &bull; Graph-Neural Threat Intelligence &amp; Multi-Hop Attack Path Reconstruction &bull; C++17 LibTorch Core
      </footer>
    </div>
  );
};
