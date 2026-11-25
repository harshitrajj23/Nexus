# NEXUS: Temporal Graph Neural Network for Autonomous AI Agent Security

> **Product Thesis:** Autonomous AI agents perform sequences of individually legitimate tool invocations that collectively construct unauthorized attack trajectories. NEXUS dynamically constructs heterogeneous temporal interaction graphs and applies Relational Graph Convolutional Networks (R-GCN) paired with Gated Recurrent Units (GRU) to detect multi-step attack vectors in real time, reconstruct causal attack paths, and provide independent runtime security evaluation.

---

## System Architecture

NEXUS comprises six production-grade subsystems engineered in C++17, LibTorch, Python FastAPI, and React + TypeScript:

```
                                  +------------------------------------+
                                  |     Tool-Using Autonomous Agent    |
                                  +-----------------+------------------+
                                                    | (Tool Invocations)
                                                    v
+---------------------------------------------------------------------------------------------------------+
| [System 01] Agent Telemetry Collector (C++17)                                                          |
|   Thread-safe, append-only JSONL ingestion with deterministic timestamp & event_id ordering             |
+---------------------------------------------------+-----------------------------------------------------+
                                                    |
                                                    v
+---------------------------------------------------------------------------------------------------------+
| [System 02] Dynamic Graph Construction Engine (C++17)                                                   |
|   Heterogeneous graph G_t = (V, E_t, X_v, X_e)                                                          |
|   - 7 Node Types: Agent, Tool, Resource, Action, Permission, Identity, Destination                     |
|   - 7 Edge Types: USES, ACCESSES, PERFORMS, AUTHORIZED_BY, MODIFIES, FOLLOWS, COMMUNICATES_WITH        |
|   - Deterministic 10D feature encoders & LibTorch GraphTensorBatch export                              |
+-------------------------+-----------------------------------------+-------------------------------------+
                          |                                         |
                          v                                         v
+----------------------------------------------+  +-------------------------------------------------------+
| [System 03] Neural Graph Intelligence        |  | [System 04] Attack Path Intelligence                 |
|   - Multi-layer R-GCN with target in-degree  |  |   - Causal multi-hop DFS heuristic (t_k >= t_{k-1})   |
|     normalization: c_{i,r} = max(deg_r(i),1) |  |   - Attribution score weighting (a_e)                 |
|   - Temporal GRU sequence encoder:           |  |   - Structural risk multipliers:                      |
|     z_t = GRU(g_t, z_{t-1})                  |  |     Credential (2.5x), Admin (2.0x), Exfil (3.0x)     |
|   - 4-Class Softmax Risk Head:               |  +---------------------------+---------------------------+
|     Normal, Suspicious, HighRisk, Critical   |                              |
+-------------------------+--------------------+                              |
                          |                                                   |
                          +-------------------------+-------------------------+
                                                    |
                                                    v
+---------------------------------------------------------------------------------------------------------+
| [FastAPI Backend Bridge] (agent_orchestrator/api.py)                                                    |
|   Communicates with persistent C++17 LibTorch nexus_engine_bridge subprocess over JSONL stdio pipes     |
|   - POST /api/events/ingest        - GET /api/graph/snapshot                                            |
|   - GET  /api/metrics              - POST /api/experiments/run                                          |
|   - GET  /health                   - POST /api/graph/reset                                              |
+-------------------------+-----------------------------------------+-------------------------------------+
                          |                                         |
                          v                                         v
+----------------------------------------------+  +-------------------------------------------------------+
| [System 05] AgntID Integration               |  | [System 06] Security Intelligence Dashboard           |
|   - Dynamic SDK/CLI/API environment checks   |  |   React, TypeScript, Tailwind CSS, @xyflow/react      |
|   - Independent decision recording           |  |   - Overview: Live feed, metrics & peak risk badges   |
|   - Strict PRD honesty: no fake blocks       |  |   - Live Graph: Interactive canvas with step slider   |
|   - Sandboxed agent harness in /tmp sandbox  |  |   - Threat Intel: Multi-hop attack path inspector     |
|                                              |  |   - Model Intel: Confusion matrix & ablation table   |
|                                              |  |   - Experiment Lab: Live runner for Experiments A,B,C |
+----------------------------------------------+  +-------------------------------------------------------+
```

---

## Directory Structure

```
Nexus/
├── CMakeLists.txt                      # Root CMake build configuration
├── README.md                           # Comprehensive platform documentation
├── SUBMISSION_REPORT.md                # Competition submission report with honest metrics
├── cpp_engine/                         # Native C++17 LibTorch Core Engine
│   ├── CMakeLists.txt                  # LibTorch, nlohmann_json, target definitions
│   ├── include/
│   │   ├── telemetry_collector.hpp     # System 01: Telemetry event structs & thread-safe collector
│   │   ├── graph_engine.hpp            # System 02: Dynamic heterogeneous graph engine
│   │   ├── rgcn.hpp                    # System 03: R-GCN layer, readout pooling, attribution
│   │   ├── nexus_model.hpp             # System 03: TemporalRiskModel (R-GCN + GRU + MLP head)
│   │   └── attack_path.hpp             # System 04: Causal DFS multi-hop attack path analyzer
│   └── src/
│       ├── main.cpp                    # Bootstrap verification executable
│       ├── telemetry_collector.cpp     # Telemetry parsing, sorting, validation
│       ├── graph_engine.cpp            # Adjacency lists, FOLLOWS chains, tensor batches
│       ├── rgcn.cpp                    # LibTorch R-GCN forward & in-degree normalization
│       ├── nexus_model.cpp             # LibTorch GRU temporal modeling & checkpoints
│       ├── attack_path.cpp             # Multi-hop attack path search & structural multipliers
│       ├── engine_bridge.cpp           # System 06 C++ stdio bridge for FastAPI & CLI
│       ├── train_nexus.cpp             # Day 5 training & ablation evaluation pipeline
│       ├── test_telemetry.cpp          # System 01 test suite
│       ├── test_graph.cpp              # System 02 test suite
│       ├── test_rgcn.cpp               # System 03 R-GCN unit test suite
│       └── test_nexus_model.cpp        # System 03/04 Temporal model & path reconstruction test
├── agent_orchestrator/                 # Python Orchestration & FastAPI Bridge
│   ├── agntid_bridge.py                # System 05: AgntID integration & environment inspector
│   ├── agent_runner.py                 # System 05: Sandboxed tool-using agent harness
│   ├── cpp_bridge.py                   # Subprocess JSONL bridge client to nexus_engine_bridge
│   ├── api.py                          # FastAPI service exposing endpoints on port 8000
│   ├── test_api.py                     # Automated end-to-end FastAPI test suite
│   ├── generate_dataset.py             # Scenario-based synthetic telemetry generator
│   └── run_competition_demos.py        # Automated competition demonstration runner (A, B, C)
├── dashboard/                          # System 06: React + TypeScript Security Intelligence UI
│   ├── package.json                    # Dependencies: React 18, @xyflow/react, Lucide, Tailwind
│   ├── vite.config.ts                  # Vite configuration with proxy to FastAPI backend
│   ├── tsconfig.json                   # Strict TypeScript compiler options
│   ├── index.html                      # HTML entry with dark canvas theme
│   └── src/
│       ├── types.ts                    # TypeScript interfaces for graph, predictions & threats
│       ├── main.tsx                    # React DOM root entry
│       ├── App.tsx                     # Core state coordinator & tab navigation
│       ├── index.css                   # Tailwind directives & animated attack-path edge styles
│       ├── services/api.ts             # API client connecting to http://localhost:8000
│       └── components/
│           ├── Navbar.tsx              # Navigation bar with C++ engine & AgntID health badges
│           ├── RiskBadge.tsx           # 4-class risk badges (Normal, Suspicious, HighRisk, Critical)
│           ├── OverviewView.tsx        # View 1: Live telemetry feed & KPI metrics
│           ├── LiveGraphView.tsx       # View 2: Interactive React Flow canvas with step scrubber
│           ├── ThreatIntelView.tsx     # View 3: Selected alert & reconstructed attack path inspector
│           ├── ModelIntelView.tsx      # View 4: Loss curves, confusion matrix & ablation table
│           └── ExperimentLabView.tsx   # View 5: Interactive runner for Experiments A, B, C
└── data/                               # Benchmarks, Checkpoints & Demo Runs
    ├── train.jsonl                     # Training split (154 scenarios, 631 events)
    ├── val.jsonl                       # Validation split (33 scenarios, 137 events)
    ├── test.jsonl                      # Test split (33 scenarios, 140 events)
    ├── test_unseen.jsonl               # Completely held-out attack templates (40 scenarios, 220 events)
    ├── dataset_manifest.json           # Comprehensive dataset summary and template metadata
    ├── nexus_best.pt                   # Trained LibTorch model checkpoint
    ├── metrics.json                    # Honest training loss, confusion matrices & ablation metrics
    └── demo_runs/                      # Saved competition demo run artifacts
        ├── experiment_a_normal.json    # Experiment A execution trace
        ├── experiment_b_adversarial.json# Experiment B execution trace
        ├── experiment_c_unseen.json    # Experiment C execution trace
        └── demo_manifest.json          # Demonstration index manifest
```

---

## Prerequisites & Installation

### 1. System Requirements
- **OS**: macOS (Apple Silicon or Intel) or Linux
- **C++**: Clang or GCC with C++17 support
- **CMake**: Version $\ge$ 3.16
- **Python**: Version $\ge$ 3.10 with `virtualenv`
- **Node.js**: Version $\ge$ 18.0 with `npm`

### 2. Environment Setup

```bash
# Clone the repository
git clone https://github.com/harshitrajj23/Nexus.git
cd Nexus

# Create and activate Python virtual environment
python3 -m venv .venv
source .venv/bin/activate

# Install Python dependencies (PyTorch, FastAPI, Uvicorn, HTTPX)
pip install torch fastapi uvicorn httpx pydantic
```

---

## Compilation & Testing Instructions

### 1. Compile C++ Engine and Bridge
```bash
cd cpp_engine
cmake -B build -DCMAKE_PREFIX_PATH=$(python3 -c 'import torch; print(torch.utils.cmake_prefix_path)')
cmake --build build -j4
```

### 2. Run C++ Test Suites (100% Pass)
```bash
# Run all unit tests via CTest
ctest --test-dir build --output-on-failure

# Or run individual test targets:
./build/nexus_telemetry_test  # System 01: Telemetry validation & sorting
./build/nexus_graph_test      # System 02: Dynamic graph & FOLLOWS chains
./build/nexus_rgcn_test       # System 03: R-GCN normalization & pooling
./build/nexus_model_test      # System 03/04: Temporal GRU & AttackPathAnalyzer
./build/nexus_engine_bridge --test # Bridge self-test
```

### 3. Build React Security Dashboard
```bash
cd ../dashboard
npm install
npm run build
```

---

## Running the Platform

### Option A: Automated Competition Demonstration Runner (CLI)
Executes all three core PRD experiments in sandboxed isolation, passes events through the native C++ LibTorch engine, records independent AgntID decisions, and saves JSON artifacts:
```bash
python3 agent_orchestrator/run_competition_demos.py
```

### Option B: Interactive Full-Stack Experience (FastAPI + React Dashboard)

**Terminal 1: Start FastAPI Backend Bridge**
```bash
source .venv/bin/activate
uvicorn agent_orchestrator.api:app --host 0.0.0.0 --port 8000
```
- API Documentation available at: `http://localhost:8000/docs`
- Health check endpoint: `http://localhost:8000/health`

**Terminal 2: Start React Security Intelligence Dashboard**
```bash
cd dashboard
npm run dev
```
- Open `http://localhost:3000` in your browser.

---

## Core PRD Competition Experiments

| Experiment | Description | Expected Outcome | Output Artifact |
|------------|-------------|------------------|-----------------|
| **Experiment A (Normal Behaviour)** | Benign repository inspection, SQL database query, quarterly report generation, and unit tests. | Normal graph evolution, benign telemetry, zero unauthorized external operations. | `data/demo_runs/experiment_a_normal.json` |
| **Experiment B (Multi-Step Attack)** | Recon pivoting to secret access (`config.env`), role escalation (`ADMIN`), and unauthorized external exfiltration. | Reconstructs 5 multi-hop causal attack paths, identifies compromised agent, records independent AgntID evaluation. | `data/demo_runs/experiment_b_adversarial.json` |
| **Experiment C (Held-Out Unseen Attack)** | Out-of-distribution generalization against withheld indirect prompt injection and cloud IMDS metadata pivot. | Successfully processes unseen attack template from `test_unseen.jsonl`, reconstructing causal vector. | `data/demo_runs/experiment_c_unseen.json` |

---

## PRD Integrity & Scientific Methodology Disclosures

1. **Synthetic Telemetry Transparency**:
   Benchmark training and validation splits were generated synthetically (`agent_orchestrator/generate_dataset.py`) to model complex multi-step adversarial progressions safely. All generated records are explicitly marked with `is_synthetic: true`. Live executions inside `agent_runner.py` are explicitly marked with `is_synthetic: false`.
2. **Honest AgntID Evaluation**:
   System checks confirm that no official AgntID SDK or daemon is currently installed in the host environment. Decisions are recorded independently as `NOT_EVALUATED` (`enforced: false`). **No fake enforcement claims or fabricated blocks are reported.**
3. **Evidence-Backed Metrics**:
   Ablation comparisons reveal that static graph models suffer from a **100.0% False Negative Rate** on multi-step attacks. Integrating temporal recurrence via GRU resolves this blindspot, reducing FNR to **20.0%**, as documented in `data/metrics.json`.
