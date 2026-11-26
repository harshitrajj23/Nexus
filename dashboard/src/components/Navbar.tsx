import React from 'react';
import {
  ShieldAlert,
  Activity,
  Network,
  Cpu,
  FlaskConical,
  Radio,
  CheckCircle2,
  AlertTriangle,
  RotateCcw,
  SlidersHorizontal
} from 'lucide-react';
import { HealthResponse } from '../types';

export type TabType = 'overview' | 'graph' | 'threats' | 'model' | 'lab' | 'intent';

interface NavbarProps {
  activeTab: TabType;
  setActiveTab: (tab: TabType) => void;
  health: HealthResponse | null;
  onResetGraph: () => void;
}

export const Navbar: React.FC<NavbarProps> = ({
  activeTab,
  setActiveTab,
  health,
  onResetGraph
}) => {
  const isHealthy = health?.status === 'healthy';
  const modelLoaded = health?.cpp_engine?.model_loaded;
  const agntidActive = health?.agntid_integration?.enforcement_active;

  const tabs: { id: TabType; label: string; icon: React.ReactNode }[] = [
    { id: 'overview', label: 'Overview', icon: <Activity className="w-4 h-4" /> },
    { id: 'graph', label: 'Live Graph', icon: <Network className="w-4 h-4" /> },
    { id: 'threats', label: 'Threat Intel', icon: <ShieldAlert className="w-4 h-4" /> },
    { id: 'model', label: 'Model Intel', icon: <Cpu className="w-4 h-4" /> },
    { id: 'lab', label: 'Experiment Lab', icon: <FlaskConical className="w-4 h-4" /> },
    { id: 'intent', label: 'Intent & AgntID', icon: <SlidersHorizontal className="w-4 h-4" /> },
  ];

  return (
    <header className="bg-[#111827] border-b border-gray-800 sticky top-0 z-50">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="flex items-center justify-between h-16">
          {/* Logo & Platform Name */}
          <div className="flex items-center space-x-3">
            <div className="w-9 h-9 rounded-lg bg-gradient-to-br from-blue-500 to-indigo-600 flex items-center justify-center shadow-lg shadow-blue-500/20">
              <ShieldAlert className="w-5 h-5 text-white" />
            </div>
            <div>
              <div className="flex items-center space-x-2">
                <span className="text-xl font-bold tracking-wider text-white">NEXUS</span>
                <span className="text-xs px-2 py-0.5 rounded-full bg-blue-500/10 text-blue-400 border border-blue-500/20 font-mono">
                  v1.0-RC
                </span>
              </div>
              <p className="text-[11px] text-gray-400 hidden sm:block">
                C++17 Dynamic Graph & R-GCN+GRU Security Intelligence
              </p>
            </div>
          </div>

          {/* Navigation Tabs */}
          <nav className="flex items-center space-x-1 sm:space-x-2">
            {tabs.map((tab) => {
              const isActive = activeTab === tab.id;
              return (
                <button
                  key={tab.id}
                  onClick={() => setActiveTab(tab.id)}
                  className={`flex items-center space-x-2 px-3 py-2 rounded-lg text-sm font-medium transition-all ${
                    isActive
                      ? 'bg-blue-600/20 text-blue-400 border border-blue-500/30 shadow-sm'
                      : 'text-gray-400 hover:text-gray-200 hover:bg-gray-800/60'
                  }`}
                >
                  {tab.icon}
                  <span className="hidden md:inline">{tab.label}</span>
                </button>
              );
            })}
          </nav>

          {/* System & AgntID Health Badges */}
          <div className="flex items-center space-x-3">
            <button
              onClick={onResetGraph}
              title="Reset graph state and temporal memory"
              className="p-1.5 rounded-lg text-gray-400 hover:text-gray-200 hover:bg-gray-800 border border-gray-700/50 transition-colors"
            >
              <RotateCcw className="w-4 h-4" />
            </button>

            {/* C++ LibTorch Engine Status */}
            <div className="flex items-center space-x-1.5 px-2.5 py-1 rounded-md bg-gray-900 border border-gray-800 text-xs">
              <span className={`w-2 h-2 rounded-full ${modelLoaded ? 'bg-emerald-400 animate-pulse' : 'bg-amber-400'}`} />
              <span className="text-gray-300 font-mono hidden sm:inline">C++ LibTorch</span>
              <span className={`font-semibold ${modelLoaded ? 'text-emerald-400' : 'text-amber-400'}`}>
                {modelLoaded ? 'READY' : 'INIT'}
              </span>
            </div>

            {/* Independent AgntID Status */}
            <div className="flex items-center space-x-1.5 px-2.5 py-1 rounded-md bg-gray-900 border border-gray-800 text-xs" title="AgntID Runtime Security Integration (Independent)">
              <span className={`w-2 h-2 rounded-full ${agntidActive ? 'bg-emerald-400' : 'bg-gray-500'}`} />
              <span className="text-gray-400 font-mono hidden sm:inline">AgntID</span>
              <span className="text-gray-300 font-mono">
                {agntidActive ? 'ACTIVE' : 'UNCONFIGURED'}
              </span>
            </div>
          </div>
        </div>
      </div>
    </header>
  );
};
