# NEXUS: Temporal Graph Neural Network for Autonomous AI Agent Security

> **Product Thesis:** An AI agent can perform individually legitimate actions that collectively create an unauthorised attack path. NEXUS learns relationships between actions, tools, resources, and permissions over time to identify suspicious behavioural patterns.

---

## Architecture Overview

NEXUS consists of six core subsystems:
1. **Agent Telemetry Collector (System 01)**: Ingests structured JSON/JSONL tool executions, validates mandatory fields, enforces deterministic chronological ordering, and writes append-only event streams.
2. **Dynamic Graph Construction Engine (System 02)**: Models heterogeneous, time-dependent agent environments $G_t = (V, E_t, X_v, X_e)$ with 7 node types (`Agent`, `Tool`, `Resource`, `Action`, `Permission`, `Identity`, `Destination`) and 7 relationship types (`USES`, `ACCESSES`, `PERFORMS`, `AUTHORIZED_BY`, `MODIFIES`, `FOLLOWS`, `COMMUNICATES_WITH`). Provides temporal snapshot filtering and PyTorch Geometric compatible LibTorch tensor batch export.
3. **Neural Graph Intelligence Engine (System 03)**: Relational Graph Convolutional Network (R-GCN) + Temporal GRU encoder for behavioural sequence modeling and multi-class risk prediction (`Normal`, `Suspicious`, `High Risk`, `Critical`).
4. **Attack Path Intelligence (System 04)**: Multi-hop graph traversal and attack-path reconstruction.
5. **AgntID Runtime Security Integration (System 05)**: Independent runtime security evaluation and verification.
6. **Security Intelligence Dashboard (System 06)**: Interactive visualization of live graph evolution and threat analysis.

---

## Directory Structure

```
Nexus/
├── CMakeLists.txt                # Root CMake workspace configuration
├── .gitignore                    # Comprehensive security and build exclusion rules
├── cpp_engine/                   # High-performance C++17 core engine
│   ├── CMakeLists.txt            # Target definitions for nexus_core and test suites
│   ├── include/
│   │   ├── telemetry_collector.hpp # System 01: Telemetry event structs and thread-safe collector
│   │   └── graph_engine.hpp        # System 02: Dynamic heterogeneous graph engine & feature encoders
│   └── src/
│       ├── main.cpp              # Core engine bootstrap & LibTorch sanity check
│       ├── telemetry_collector.cpp # Event validation, sorting & JSONL serialization
│       ├── test_telemetry.cpp    # System 01 test suite (concurrency, ordering, validation)
│       ├── graph_engine.cpp      # Adjacency lists, event ingestion, snapshots, LibTorch export
│       └── test_graph.cpp        # System 02 test suite (topology, FOLLOWS chains, tensor export)
├── agent_orchestrator/           # Python agent instrumentation
├── dashboard/                    # Security intelligence dashboard
└── data/                         # Telemetry benchmark datasets
```

---

## Dependencies & Requirements

- **C++ Compiler**: Apple Clang or GCC with full C++17 support
- **Build System**: CMake $\ge$ 3.16
- **Deep Learning**: LibTorch / PyTorch $\ge$ 2.0 (C++ API)
- **JSON Serialization**: `nlohmann/json` $\ge$ 3.2.0

---

## Build & Test Instructions

### 1. Configure
```bash
cd cpp_engine
cmake -B build -DCMAKE_PREFIX_PATH=$(python3 -c 'import torch; print(torch.utils.cmake_prefix_path)' 2>/dev/null || echo "")
```

### 2. Compile All Targets
```bash
cmake --build build
```

### 3. Run Test Suites
```bash
# Telemetry Collector Suite
./build/nexus_telemetry_test

# Dynamic Graph Engine Suite
./build/nexus_graph_test

# Or run via CTest
ctest --test-dir build --output-on-failure
```

### 4. Run Core Engine
```bash
./build/nexus_core
```
