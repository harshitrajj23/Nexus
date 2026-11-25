import React, { useState, useMemo, useEffect, useCallback } from 'react';
import {
  ReactFlow,
  Background,
  Controls,
  MiniMap,
  Node,
  Edge,
  MarkerType,
  useNodesState,
  useEdgesState,
  Panel,
} from '@xyflow/react';
import {
  GraphNode,
  GraphEdge,
  AttackPath,
  NodeType,
  RelationType
} from '../types';
import {
  Bot,
  Wrench,
  Database,
  Play,
  Key,
  Shield,
  Globe,
  Sliders,
  PlayCircle,
  PauseCircle,
  SkipForward,
  RotateCcw,
  X,
  Info,
  Maximize2
} from 'lucide-react';

interface LiveGraphViewProps {
  nodes: GraphNode[];
  edges: GraphEdge[];
  activeAttackPath?: AttackPath | null;
  onRefreshGraph: () => void;
}

const nodeTypeColors: Record<NodeType, { border: string; bg: string; text: string; icon: React.ReactNode }> = {
  Agent: {
    border: 'border-blue-500',
    bg: 'bg-blue-950/90',
    text: 'text-blue-300',
    icon: <Bot className="w-4 h-4 text-blue-400" />
  },
  Tool: {
    border: 'border-purple-500',
    bg: 'bg-purple-950/90',
    text: 'text-purple-300',
    icon: <Wrench className="w-4 h-4 text-purple-400" />
  },
  Resource: {
    border: 'border-amber-500',
    bg: 'bg-amber-950/90',
    text: 'text-amber-300',
    icon: <Database className="w-4 h-4 text-amber-400" />
  },
  Action: {
    border: 'border-emerald-500',
    bg: 'bg-emerald-950/90',
    text: 'text-emerald-300',
    icon: <Play className="w-4 h-4 text-emerald-400" />
  },
  Permission: {
    border: 'border-rose-500',
    bg: 'bg-rose-950/90',
    text: 'text-rose-300',
    icon: <Key className="w-4 h-4 text-rose-400" />
  },
  Identity: {
    border: 'border-indigo-500',
    bg: 'bg-indigo-950/90',
    text: 'text-indigo-300',
    icon: <Shield className="w-4 h-4 text-indigo-400" />
  },
  Destination: {
    border: 'border-red-500',
    bg: 'bg-red-950/90',
    text: 'text-red-300',
    icon: <Globe className="w-4 h-4 text-red-400" />
  }
};

const relationColors: Record<RelationType, string> = {
  USES: '#3B82F6',
  ACCESSES: '#F59E0B',
  PERFORMS: '#10B981',
  AUTHORIZED_BY: '#F43F5E',
  MODIFIES: '#F97316',
  FOLLOWS: '#6366F1',
  COMMUNICATES_WITH: '#06B6D4'
};

export const LiveGraphView: React.FC<LiveGraphViewProps> = ({
  nodes: rawNodes,
  edges: rawEdges,
  activeAttackPath,
  onRefreshGraph
}) => {
  // Selected node for inspection
  const [selectedNode, setSelectedNode] = useState<GraphNode | null>(null);

  // Timeline Scrubbing State
  const [maxStep, setMaxStep] = useState<number>(0);
  const [currentStep, setCurrentStep] = useState<number>(0);
  const [isPlaying, setIsPlaying] = useState<boolean>(false);

  // Update maxStep whenever rawEdges change
  useEffect(() => {
    const totalSteps = rawEdges.length > 0 ? rawEdges.length : rawNodes.length;
    setMaxStep(totalSteps);
    setCurrentStep(totalSteps);
  }, [rawEdges.length, rawNodes.length]);

  // Auto-play timeline step slider
  useEffect(() => {
    let timer: any;
    if (isPlaying) {
      timer = setInterval(() => {
        setCurrentStep((prev) => {
          if (prev >= maxStep) {
            setIsPlaying(false);
            return prev;
          }
          return prev + 1;
        });
      }, 800);
    }
    return () => clearInterval(timer);
  }, [isPlaying, maxStep]);

  // Set of edge IDs and node IDs involved in the active attack path
  const attackPathEdgeSet = useMemo(() => {
    const set = new Set<string>();
    if (activeAttackPath?.steps) {
      activeAttackPath.steps.forEach((step) => {
        set.add(`e-${step.source_id}-${step.destination_id}`);
        set.add(`edge-${step.edge_id}`);
        set.add(`${step.source_id}->${step.destination_id}`);
      });
    }
    return set;
  }, [activeAttackPath]);

  const attackPathNodeSet = useMemo(() => {
    const set = new Set<number>();
    if (activeAttackPath?.steps) {
      activeAttackPath.steps.forEach((step) => {
        set.add(step.source_id);
        set.add(step.destination_id);
      });
    }
    return set;
  }, [activeAttackPath]);

  // Filter edges based on timeline slider
  const visibleEdges = useMemo(() => {
    if (rawEdges.length === 0) return [];
    return rawEdges.slice(0, currentStep);
  }, [rawEdges, currentStep]);

  // Visible nodes: nodes that have edges or all nodes if slider is full
  const visibleNodes = useMemo(() => {
    if (rawNodes.length === 0) return [];
    if (visibleEdges.length === 0 && currentStep > 0) return rawNodes.slice(0, 3);
    const activeNodeIds = new Set<number>();
    visibleEdges.forEach((e) => {
      activeNodeIds.add(e.source);
      activeNodeIds.add(e.destination);
    });
    return rawNodes.filter((n) => activeNodeIds.has(n.id) || currentStep >= maxStep);
  }, [rawNodes, visibleEdges, currentStep, maxStep]);

  // Layout nodes radially / hierarchically by type
  const flowNodes: Node[] = useMemo(() => {
    const typeOrder: NodeType[] = ['Agent', 'Tool', 'Action', 'Permission', 'Resource', 'Destination', 'Identity'];
    const columns: Record<string, GraphNode[]> = {};
    typeOrder.forEach(t => { columns[t] = []; });

    visibleNodes.forEach(n => {
      if (!columns[n.type_name]) columns[n.type_name] = [];
      columns[n.type_name].push(n);
    });

    const result: Node[] = [];
    const colSpacing = 220;
    const rowSpacing = 95;

    typeOrder.forEach((t, colIndex) => {
      const list = columns[t] || [];
      const totalHeight = list.length * rowSpacing;
      const startY = Math.max(80, 260 - totalHeight / 2);

      list.forEach((n, rowIndex) => {
        const isAttackNode = attackPathNodeSet.has(n.id);
        const style = nodeTypeColors[n.type_name] || nodeTypeColors.Resource;

        result.push({
          id: String(n.id),
          position: {
            x: 60 + colIndex * colSpacing,
            y: startY + rowIndex * rowSpacing,
          },
          data: {
            node: n,
            label: (
              <div
                className={`px-3 py-2 rounded-lg border shadow-lg cursor-pointer transition-all ${style.bg} ${
                  isAttackNode
                    ? 'border-red-500 shadow-red-500/50 ring-2 ring-red-500 animate-pulse'
                    : selectedNode?.id === n.id
                    ? 'border-cyan-400 ring-2 ring-cyan-400'
                    : `${style.border} hover:border-gray-300`
                }`}
                onClick={() => setSelectedNode(n)}
              >
                <div className="flex items-center space-x-2">
                  {style.icon}
                  <span className="text-[10px] uppercase font-mono font-bold tracking-wider text-gray-400">
                    {n.type_name}
                  </span>
                  <span className="text-[9px] font-mono text-gray-500">#{n.id}</span>
                </div>
                <div className={`mt-1 font-mono text-xs font-semibold truncate max-w-[150px] ${style.text}`}>
                  {n.key}
                </div>
              </div>
            ),
          },
        });
      });
    });

    return result;
  }, [visibleNodes, attackPathNodeSet, selectedNode]);

  // Construct Flow Edges
  const flowEdges: Edge[] = useMemo(() => {
    return visibleEdges.map((e) => {
      const isAttackEdge =
        attackPathEdgeSet.has(`e-${e.source}-${e.destination}`) ||
        attackPathEdgeSet.has(`edge-${e.id}`) ||
        attackPathEdgeSet.has(`${e.source}->${e.destination}`);

      const color = relationColors[e.relation_name] || '#6B7280';

      return {
        id: `e-${e.source}-${e.destination}-${e.id}`,
        source: String(e.source),
        target: String(e.destination),
        label: e.relation_name,
        animated: isAttackEdge,
        className: isAttackEdge ? 'attack-path-edge' : '',
        style: {
          stroke: isAttackEdge ? '#EF4444' : color,
          strokeWidth: isAttackEdge ? 3.5 : 1.8,
        },
        labelStyle: {
          fill: isAttackEdge ? '#F87171' : '#9CA3AF',
          fontFamily: 'monospace',
          fontSize: 9,
          fontWeight: isAttackEdge ? 700 : 500,
        },
        labelBgStyle: {
          fill: '#111827',
          fillOpacity: 0.85,
        },
        markerEnd: {
          type: MarkerType.ArrowClosed,
          color: isAttackEdge ? '#EF4444' : color,
          width: 14,
          height: 14,
        },
      };
    });
  }, [visibleEdges, attackPathEdgeSet]);

  const [nodes, setNodes, onNodesChange] = useNodesState(flowNodes);
  const [edges, setEdges, onEdgesChange] = useEdgesState(flowEdges);

  useEffect(() => {
    setNodes(flowNodes);
  }, [flowNodes, setNodes]);

  useEffect(() => {
    setEdges(flowEdges);
  }, [flowEdges, setEdges]);

  return (
    <div className="relative w-full h-[720px] bg-[#0B0F19] rounded-xl border border-gray-800 overflow-hidden flex flex-col">
      {/* Top Controls Toolbar */}
      <div className="bg-[#111827]/90 backdrop-blur border-b border-gray-800 px-4 py-2.5 flex flex-wrap items-center justify-between gap-3 z-10">
        <div className="flex items-center space-x-3">
          <span className="text-xs font-semibold text-gray-200 uppercase tracking-wider font-mono flex items-center space-x-1.5">
            <span className="w-2 h-2 rounded-full bg-blue-400" />
            <span>Interactive Dynamic Graph</span>
          </span>
          <span className="text-xs font-mono px-2 py-0.5 rounded bg-gray-800 text-gray-400 border border-gray-700">
            {visibleNodes.length} / {rawNodes.length} nodes
          </span>
          <span className="text-xs font-mono px-2 py-0.5 rounded bg-gray-800 text-gray-400 border border-gray-700">
            {visibleEdges.length} / {rawEdges.length} edges
          </span>
          {activeAttackPath && (
            <span className="text-xs font-mono px-2.5 py-0.5 rounded-full bg-red-500/10 text-red-400 border border-red-500/30 font-semibold flex items-center space-x-1">
              <span className="w-1.5 h-1.5 rounded-full bg-red-400 animate-ping" />
              <span>Attack Path Highlight Active</span>
            </span>
          )}
        </div>

        {/* Timeline Scrubbing Controls */}
        <div className="flex items-center space-x-3 bg-gray-900 px-3 py-1.5 rounded-lg border border-gray-800">
          <button
            onClick={() => setIsPlaying(!isPlaying)}
            className="text-gray-300 hover:text-white transition-colors"
            title={isPlaying ? 'Pause timeline playback' : 'Play timeline'}
          >
            {isPlaying ? (
              <PauseCircle className="w-4 h-4 text-amber-400" />
            ) : (
              <PlayCircle className="w-4 h-4 text-emerald-400" />
            )}
          </button>

          <button
            onClick={() => setCurrentStep(prev => Math.min(prev + 1, maxStep))}
            disabled={currentStep >= maxStep}
            className="text-gray-400 hover:text-gray-200 disabled:opacity-30 transition-colors"
            title="Step forward"
          >
            <SkipForward className="w-3.5 h-3.5" />
          </button>

          <button
            onClick={() => {
              setIsPlaying(false);
              setCurrentStep(maxStep);
            }}
            className="text-gray-400 hover:text-gray-200 transition-colors"
            title="Reset to latest"
          >
            <RotateCcw className="w-3.5 h-3.5" />
          </button>

          <div className="flex items-center space-x-2">
            <Sliders className="w-3.5 h-3.5 text-gray-400" />
            <input
              type="range"
              min={0}
              max={maxStep}
              value={currentStep}
              onChange={(e) => {
                setIsPlaying(false);
                setCurrentStep(Number(e.target.value));
              }}
              className="w-24 sm:w-36 h-1.5 bg-gray-700 rounded-lg appearance-none cursor-pointer accent-blue-500"
            />
            <span className="text-[11px] font-mono text-gray-400 w-12 text-right">
              {currentStep}/{maxStep}
            </span>
          </div>
        </div>
      </div>

      {/* React Flow Canvas */}
      <div className="flex-1 w-full h-full relative">
        <ReactFlow
          nodes={nodes}
          edges={edges}
          onNodesChange={onNodesChange}
          onEdgesChange={onEdgesChange}
          fitView
          fitViewOptions={{ padding: 0.2 }}
          attributionPosition="bottom-left"
        >
          <Background color="#1F2937" gap={20} size={1} />
          <Controls />
          <MiniMap
            nodeColor={(n: any) => {
              const nodeData = visibleNodes.find(vn => String(vn.id) === n.id);
              if (!nodeData) return '#374151';
              if (attackPathNodeSet.has(nodeData.id)) return '#EF4444';
              return relationColors.ACCESSES;
            }}
            maskColor="rgba(11, 15, 25, 0.7)"
            style={{ backgroundColor: '#111827', border: '1px solid #1F2937' }}
          />

          {/* Graph Legend Panel */}
          <Panel position="bottom-left" className="bg-[#111827]/90 border border-gray-800 p-2.5 rounded-lg text-[10px] font-mono space-y-1 backdrop-blur shadow-md">
            <div className="font-semibold text-gray-300 pb-1 border-b border-gray-800">Node Types</div>
            <div className="grid grid-cols-2 gap-x-3 gap-y-0.5 text-gray-400">
              <span className="flex items-center space-x-1.5"><span className="w-2 h-2 rounded-full bg-blue-500" /><span>Agent</span></span>
              <span className="flex items-center space-x-1.5"><span className="w-2 h-2 rounded-full bg-purple-500" /><span>Tool</span></span>
              <span className="flex items-center space-x-1.5"><span className="w-2 h-2 rounded-full bg-emerald-500" /><span>Action</span></span>
              <span className="flex items-center space-x-1.5"><span className="w-2 h-2 rounded-full bg-rose-500" /><span>Permission</span></span>
              <span className="flex items-center space-x-1.5"><span className="w-2 h-2 rounded-full bg-amber-500" /><span>Resource</span></span>
              <span className="flex items-center space-x-1.5"><span className="w-2 h-2 rounded-full bg-red-500" /><span>Destination</span></span>
            </div>
          </Panel>
        </ReactFlow>

        {/* Selected Node Details Drawer */}
        {selectedNode && (
          <div className="absolute right-4 top-4 bottom-4 w-80 bg-[#111827]/95 border border-gray-700 rounded-xl p-4 shadow-2xl backdrop-blur flex flex-col z-20">
            <div className="flex items-center justify-between pb-3 border-b border-gray-800">
              <div className="flex items-center space-x-2">
                <Info className="w-4 h-4 text-cyan-400" />
                <h3 className="text-sm font-semibold text-white">Node Inspector</h3>
              </div>
              <button
                onClick={() => setSelectedNode(null)}
                className="text-gray-400 hover:text-white"
              >
                <X className="w-4 h-4" />
              </button>
            </div>

            <div className="mt-3 space-y-3 font-mono text-xs flex-1 overflow-y-auto">
              <div className="bg-gray-900 p-2.5 rounded border border-gray-800">
                <span className="text-gray-500 block text-[10px] uppercase">Node ID</span>
                <span className="text-gray-200 font-bold">{selectedNode.id}</span>
              </div>

              <div className="bg-gray-900 p-2.5 rounded border border-gray-800">
                <span className="text-gray-500 block text-[10px] uppercase">Entity Type</span>
                <span className="text-cyan-400 font-bold">{selectedNode.type_name}</span>
              </div>

              <div className="bg-gray-900 p-2.5 rounded border border-gray-800">
                <span className="text-gray-500 block text-[10px] uppercase">Key / Value</span>
                <span className="text-amber-300 font-semibold break-all">{selectedNode.key}</span>
              </div>

              <div className="bg-gray-900 p-2.5 rounded border border-gray-800">
                <span className="text-gray-500 block text-[10px] uppercase">Created At</span>
                <span className="text-gray-300">{selectedNode.created_at}</span>
              </div>

              {attackPathNodeSet.has(selectedNode.id) && (
                <div className="bg-red-950/60 p-2.5 rounded border border-red-500/50 text-red-300">
                  <span className="font-bold flex items-center space-x-1">
                    <span className="w-2 h-2 rounded-full bg-red-400 animate-ping inline-block" />
                    <span>Attack Vector Component</span>
                  </span>
                  <p className="text-[11px] mt-1 text-red-200/80">
                    This entity participates in a reconstructed multi-hop attack trajectory.
                  </p>
                </div>
              )}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
