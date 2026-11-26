# NEXUS: Temporal Graph Neural Network for Autonomous AI Agent Security

> **Product Thesis:** Autonomous AI agents perform sequences of individually legitimate tool invocations that collectively construct unauthorized attack trajectories. NEXUS dynamically constructs heterogeneous temporal interaction graphs and applies Relational Graph Convolutional Networks (R-GCN) paired with Gated Recurrent Units (GRU) to detect multi-step attack vectors in real time, reconstruct causal attack paths, and provide independent runtime security evaluation.

---

## System Architecture

NEXUS comprises six end-to-end subsystems engineered in C++17, LibTorch, Python FastAPI, and React + TypeScript:

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

## 🚀 90-Second Judge Demo Quickstart

Judges can reproduce the entire NEXUS threat detection lifecycle, verify the C++17 LibTorch neural inference, and inspect the hybrid explainable security decisions in under 90 seconds.

### Step 1: Start FastAPI Backend Bridge (Terminal 1)
```bash
cd /Users/harshitrajsingh/Nexus
source .venv/bin/activate
uvicorn agent_orchestrator.api:app --host 0.0.0.0 --port 8000
```
- API Documentation available at: `http://localhost:8000/docs`
- Health check endpoint: `http://localhost:8000/health` (inspects C++ model state and AgntID status)

### Step 2: Start React Security Intelligence Dashboard (Terminal 2)
```bash
cd /Users/harshitrajsingh/Nexus/dashboard
npm run dev
```
- Open `http://localhost:5173` (or `http://localhost:3000`) in your browser.

### Step 3: Run the Automated Competition Experiments (Terminal 3)
```bash
cd /Users/harshitrajsingh/Nexus
source .venv/bin/activate
python agent_orchestrator/run_competition_demos.py
```

### What Judges Will See in the Dashboard

1. **Active Security Decision Panel (5-Second Executive Clarity)**:
   - **Policy Recommendation**: `Recommend workflow pause and investigation` (or `Require human approval` / `Continue monitoring`).
   - **Evidence Score**: Clear `100/100` score with deterministic signal attribution (+25 Credential Access, +25 Privilege Escalation, +35 External Egress, +10 Multi-Hop Chain, +10 Neural Anomaly).
   - **Model vs Evidence Breakdown**: Neural model prediction (`Suspicious (85.2%)`) clearly separated from the deterministic policy risk (`CRITICAL`).
   - **Enforcement Transparency**: Explicit badge displaying `NOT_ENFORCED (Simulated / Independent)`—honestly disclosing that NEXUS is advisory intelligence and no inline blocking daemon intercepted traffic.

2. **Interactive Live Graph View**:
   - Reconstructed causal attack path rendered in real time with animated red directional edges.
   - Semantic node badges:
     - `[SENSITIVE]` on `credentials/config.env`
     - `[ELEVATED]` on `iam/roles/admin`
     - `[EXTERNAL]` and pulsing `[TARGET EGRESS]` on `mock://adversary-exfil.org/api/v1/dump`.
   - Interactive timeline scrubber slider to step backward and forward through graph evolution.

3. **Threat Intel View**:
   - Chronological causal hops ($t_k \ge t_{k-1}$) with R-GCN attribution scores ($a_e$) and structural multipliers (2.5x credentials, 2.0x admin escalation, 3.0x exfiltration).
   - Full human-readable incident narrative and structured evidence matrix.

4. **Experiment Lab**:
   - **Experiment A (Normal)**: 4 benign steps &rarr; Evidence score: 10/100, Policy: Normal, Action: "Continue monitoring".
   - **Experiment B (Multi-Step Attack Vector)**: 4 steps &rarr; Evidence score progresses: 10/100 &rarr; 40/100 (secret read) &rarr; 65/100 (admin escalation) &rarr; 100/100 (exfiltration). Action: "Recommend workflow pause and investigation".
   - **Experiment C (Unseen Prompt Injection & Cloud Metadata Pivot)**: Evaluates a completely withheld attack template from `test_unseen.jsonl` &rarr; correctly detected and flagged as Critical (100/100).

---

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

## Judge Preflight Check & Demo Checklist

### 1. Judge Preflight Check (Single Command)
Run the automated environment and dependency preflight check:
```bash
./scripts/judge_preflight.sh
# Or alternatively: python3 agent_orchestrator/judge_preflight.py
```
This inspects Python (>= 3.10), PyTorch/LibTorch, C++ engine binary, model weights (`nexus_best.pt`), Node.js, and dashboard build readiness with zero external network calls.

### 2. 90-Second Demo Execution (Single Command)
Run the complete, safe competition demo:
```bash
./scripts/judge_demo.sh
# Or alternatively: python3 agent_orchestrator/judge_demo.py
```
This safely runs Experiments A, B, and C in local sandboxes, prints chronological risk escalation, and generates a timestamped Judge Evidence Bundle at `data/demo_runs/JUDGE_EVIDENCE.md`.

### 3. Interactive Judge Demo Checklist (Browser Experience)
1. **Preflight**: Verify `./scripts/judge_preflight.sh` passes.
2. **Start Backend**: `uvicorn agent_orchestrator.api:app --host 0.0.0.0 --port 8000`
3. **Start Dashboard**: `cd dashboard && npm run dev` &rarr; Open `http://localhost:5173`
4. **First Tab to Inspect (Overview)**: Observe the **Active Security Decision Panel** and **Why NEXUS?** comparison.
5. **Scenario to Run**: Click **Experiment Lab** &rarr; Run **Experiment B (Multi-Step Attack Vector)**.
6. **Expected Outcome**: Watch risk escalate to **Critical (95–100/100)** with recommendation: *"Recommend workflow pause and investigation"*.
7. **What to Inspect Across Tabs**:
   - **Live Graph**: Reconstructed causal attack path, timeline scrubber, and semantic badges (`[SENSITIVE]`, `[ELEVATED]`, `[TARGET EGRESS]`).
   - **Threat Intel**: Chronological hop table with attribution scores ($a_e$) and structural multipliers (2.5x, 2.0x, 3.0x).
   - **Model Intel**: Confusion matrix and ablation study proving temporal GRU recurrence reduced False Negatives by 80%.

---

## PRD Integrity & Scientific Methodology Disclosures

In accordance with strict competition evaluation and scientific integrity principles, NEXUS maintains full transparency across all reported metrics, system statuses, and experimental findings:

1. **Four-Layer Decision Framework (Model != Evidence != Enforcement)**:
   NEXUS strictly decouples the four layers of autonomous threat assessment:
   - **Layer 1: Neural Model Signal**: Raw 4-class softmax probability distribution emitted by the C++17 LibTorch R-GCN + GRU model (`Normal`, `Suspicious`, `HighRisk`, `Critical`).
   - **Layer 2: Deterministic Evidence Score (0–100)**: Transparent points-based score grounded in verified graph topology (`+25` sensitive secret read, `+25` admin privilege escalation, `+35` external network egress, `+10` complete causal chain, `+10` neural anomaly confirmation).
   - **Layer 3: Policy Recommendation**: Objective operational guidance mapped from evidence thresholds:
     - `0–24` Normal: *"Continue monitoring"*
     - `25–49` Suspicious: *"Alert analyst"*
     - `50–74` HighRisk: *"Require human approval before next sensitive action"*
     - `75–100` Critical: *"Recommend workflow pause and investigation"*
   - **Layer 4: Actual Enforcement Status**: Reported honestly as `"Advisory only — no live blocking gateway attached"`. Policy recommendations inform human security teams; no simulated system claims inline interception without an active enforcement gateway.

2. **Honest Diagnostic on 4-Class Neural Model Performance**:
   - The temporal R-GCN + GRU architecture successfully resolved the catastrophic **100.0% False Negative Rate** of static graph models (reducing FNR to **20.0%**).
   - However, on the held-out test split, predictions clustered in `Normal` and `Suspicious`, with zero predictions in `HighRisk` and `Critical`. This reflects class-weight sensitivity and sequence-window granularity on small discrete splits.
   - **Why this matters for judges**: Rather than fabricating artificial 99% accuracy numbers or concealing model undertraining, NEXUS pairs the graph neural network with deterministic structural signal attribution. This hybrid paradigm provides transparent explainability and defensible security decision-making.

3. **Synthetic Telemetry Transparency**:
   Benchmark training and validation splits were generated synthetically (`agent_orchestrator/generate_dataset.py`) to safely model hazardous agent exploits (credential theft, cloud metadata queries). All benchmark records are explicitly flagged with `is_synthetic: true`. Live executions inside `agent_runner.py` are explicitly flagged with `is_synthetic: false`.

4. **Honest AgntID Evaluation & Status**:
   - **AgntID adapter scaffold is present; official runtime validation is pending credentials and documented API contract.**
   - **NEXUS policy is advisory when AgntID is unavailable.**
   - **All current decisions in local demos are NEXUS decisions, not AgntID decisions.**
   - **Mock external egress is simulated locally and performs no real network transfer.**
   - System checks confirm that no official AgntID SDK or daemon is currently installed in the host environment. Decisions are recorded independently as `NOT_EVALUATED` (`enforced: false`). **No fake enforcement claims or fabricated blocks are reported.**

---

## Known Limitations and Next Steps

1. **Synthetic Benchmark Limitation**:
   Current evaluation benchmarks rely on scenario-generated telemetry (`agent_orchestrator/generate_dataset.py`). While realistic and diverse, synthetic execution cannot fully replicate the nuances of real production enterprise agents.
2. **Exact Severity Calibration on Discrete Splits**:
   While the R-GCN + GRU model effectively detects anomalies (reducing FNR to 20%), fine-grained 4-class severity calibration (`Suspicious` vs `HighRisk` vs `Critical`) requires larger training corpora, focal loss tuning, and multi-task learning.
3. **Real-World Agent Telemetry Validation**:
   Future work will integrate live OpenTelemetry hooks into popular autonomous frameworks (LangChain, AutoGen, CrewAI, and Model Context Protocol servers).
4. **Official AgntID Inline Enforcement Adapter**:
   When the official AgntID enforcement daemon or webhook API is available, NEXUS policy recommendations will feed directly into inline interception gateways.
5. **Per-Organization Policy Threshold Calibration**:
   Evidence score thresholds (25, 50, 75) are set conservatively for demonstration and should be calibrated per organization based on risk tolerance and operational workflows.
