#include "graph_engine.hpp"
#include "telemetry_collector.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace nexus::graph;
using namespace nexus::telemetry;

static int g_failures = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  Assertion failed: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            test_passed = false; \
        } \
    } while (0)

static TelemetryEvent createTestEvent(
    std::string event_id,
    std::string agent_id,
    int64_t timestamp,
    std::string action,
    std::string tool,
    std::string resource = "",
    std::string permission = "",
    std::string intent_id = "",
    std::string status = "SUCCESS",
    std::string label = "NORMAL",
    bool is_synthetic = false) {
    TelemetryEvent ev;
    ev.event_id = std::move(event_id);
    ev.agent_id = std::move(agent_id);
    ev.timestamp = timestamp;
    ev.action = std::move(action);
    ev.tool = std::move(tool);
    ev.resource = std::move(resource);
    ev.permission = std::move(permission);
    ev.intent_id = std::move(intent_id);
    ev.status = std::move(status);
    ev.label = std::move(label);
    ev.is_synthetic = is_synthetic;
    return ev;
}

static void test_node_and_edge_crud() {
    const std::string test_name = "Node & Edge CRUD and Adjacency Bounds Validation";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    DynamicGraphEngine engine;

    // 1. Upsert unique nodes
    int ag1 = engine.upsertNode(NodeType::Agent, "AGENT_007", 1700000000);
    int tl1 = engine.upsertNode(NodeType::Tool, "SHELL_EXEC", 1700000000);
    int res1 = engine.upsertNode(NodeType::Resource, "/etc/passwd", 1700000000);

    TEST_ASSERT(ag1 == 0, "First node must have ID 0");
    TEST_ASSERT(tl1 == 1, "Second node must have ID 1");
    TEST_ASSERT(res1 == 2, "Third node must have ID 2");
    TEST_ASSERT(engine.numNodes() == 3, "Node count should be 3");

    // 2. Idempotency of upsertNode
    int ag1_again = engine.upsertNode(NodeType::Agent, "AGENT_007", 1700000050);
    TEST_ASSERT(ag1 == ag1_again, "Upserting existing (NodeType, key) must return existing ID");
    TEST_ASSERT(engine.numNodes() == 3, "Node count must not increase on duplicate upsert");

    // 3. Add Edges
    int e1 = engine.addEdge(ag1, tl1, RelationType::Uses, 1700000005);
    int e2 = engine.addEdge(tl1, res1, RelationType::Accesses, 1700000010);

    TEST_ASSERT(e1 == 0, "First edge must have ID 0");
    TEST_ASSERT(e2 == 1, "Second edge must have ID 1");
    TEST_ASSERT(engine.numEdges() == 2, "Edge count should be 2");

    // 4. Adjacency lists
    const auto& out_ag1 = engine.outgoingEdges(ag1);
    TEST_ASSERT(out_ag1.size() == 1 && out_ag1[0] == e1, "Agent should have outgoing edge e1");

    const auto& in_tl1 = engine.incomingEdges(tl1);
    TEST_ASSERT(in_tl1.size() == 1 && in_tl1[0] == e1, "Tool should have incoming edge e1");

    const auto& out_tl1 = engine.outgoingEdges(tl1);
    TEST_ASSERT(out_tl1.size() == 1 && out_tl1[0] == e2, "Tool should have outgoing edge e2");

    // 5. Bounds validation exceptions
    bool caught_invalid_src = false;
    try {
        engine.addEdge(999, tl1, RelationType::Uses, 1700000000);
    } catch (const std::out_of_range&) {
        caught_invalid_src = true;
    }
    TEST_ASSERT(caught_invalid_src, "addEdge must throw std::out_of_range on out-of-bounds source");

    bool caught_invalid_node = false;
    try {
        (void)engine.getNode(100);
    } catch (const std::out_of_range&) {
        caught_invalid_node = true;
    }
    TEST_ASSERT(caught_invalid_node, "getNode must throw std::out_of_range on invalid ID");

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_deterministic_feature_encoders() {
    const std::string test_name = "Deterministic Feature Encoders (Node & Edge)";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    // 1. Node features: one-hot + temporal
    auto feat_agent = encodeNodeFeatures(NodeType::Agent, 1700000000);
    auto feat_tool = encodeNodeFeatures(NodeType::Tool, 1700000000);

    TEST_ASSERT(feat_agent.size() == kNodeFeatureDim, "Node feature vector must equal kNodeFeatureDim");
    TEST_ASSERT(feat_agent[0] == 1.0f && feat_agent[1] == 0.0f, "Agent one-hot index 0 must be 1.0");
    TEST_ASSERT(feat_tool[0] == 0.0f && feat_tool[1] == 1.0f, "Tool one-hot index 1 must be 1.0");

    // Determinism check
    auto feat_agent_2 = encodeNodeFeatures(NodeType::Agent, 1700000000);
    TEST_ASSERT(feat_agent == feat_agent_2, "Identical inputs must yield identical node features");

    // Clamping & normalization checks
    TEST_ASSERT(feat_agent[7] >= 0.0f && feat_agent[7] <= 1.0f, "Timestamp feature must be clamped in [0, 1]");
    TEST_ASSERT(feat_agent[8] >= -1.0f && feat_agent[8] <= 1.0f, "Sin diurnal feature in [-1, 1]");
    TEST_ASSERT(feat_agent[9] >= -1.0f && feat_agent[9] <= 1.0f, "Cos diurnal feature in [-1, 1]");

    // 2. Edge features: one-hot + dt normalized + log dt
    auto feat_follows = encodeEdgeFeatures(RelationType::Follows, 1700000050, 50);
    TEST_ASSERT(feat_follows.size() == kEdgeFeatureDim, "Edge feature vector must equal kEdgeFeatureDim");
    TEST_ASSERT(feat_follows[static_cast<size_t>(RelationType::Follows)] == 1.0f, "Follows one-hot must be 1.0");
    TEST_ASSERT(feat_follows[7] == 50.0f / 3600.0f, "Normalized dt index 7 verification");
    TEST_ASSERT(feat_follows[8] > 0.0f && feat_follows[8] <= 1.0f, "Log dt index 8 in (0, 1]");

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_telemetry_event_ingestion() {
    const std::string test_name = "Telemetry Ingestion & Heterogeneous Topology";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    DynamicGraphEngine engine;

    TelemetryEvent event;
    event.event_id = "EVT_COMP_01";
    event.agent_id = "AGENT_RED_TEAM";
    event.timestamp = 1795330100;
    event.action = "MODIFY_FILE";
    event.tool = "REST_CLIENT";
    event.resource = "https://external-api.exfil.org/v1/leak";
    event.permission = "ADMIN";
    event.intent_id = "INTENT_EXFIL_01";
    event.status = "SUCCESS";

    bool ok = engine.ingestEvent(event);
    TEST_ASSERT(ok, "ingestEvent must return true for valid event");

    // Verify created nodes: Agent, Action, Tool, Resource, Destination, Permission
    TEST_ASSERT(engine.hasNode(NodeType::Agent, "AGENT_RED_TEAM"), "Agent node missing");
    TEST_ASSERT(engine.hasNode(NodeType::Action, "EVT_COMP_01"), "Action node missing");
    TEST_ASSERT(engine.hasNode(NodeType::Tool, "REST_CLIENT"), "Tool node missing");
    TEST_ASSERT(engine.hasNode(NodeType::Destination, "https://external-api.exfil.org/v1/leak"), "Destination node missing");
    TEST_ASSERT(engine.hasNode(NodeType::Resource, "https://external-api.exfil.org/v1/leak"), "Resource node missing");
    TEST_ASSERT(engine.hasNode(NodeType::Permission, "ADMIN"), "Permission node missing");

    // Verify typed edges
    bool has_performs = false;
    bool has_uses = false;
    bool has_accesses = false;
    bool has_modifies = false;
    bool has_auth_by = false;
    bool has_comm_with = false;

    for (const auto& edge : engine.allEdges()) {
        if (edge.relation == RelationType::Performs) has_performs = true;
        if (edge.relation == RelationType::Uses) has_uses = true;
        if (edge.relation == RelationType::Accesses) has_accesses = true;
        if (edge.relation == RelationType::Modifies) has_modifies = true;
        if (edge.relation == RelationType::AuthorizedBy) has_auth_by = true;
        if (edge.relation == RelationType::CommunicatesWith) has_comm_with = true;
    }

    TEST_ASSERT(has_performs, "PERFORMS edge missing");
    TEST_ASSERT(has_uses, "USES edge missing");
    TEST_ASSERT(has_accesses, "ACCESSES edge missing");
    TEST_ASSERT(has_modifies, "MODIFIES edge missing for MODIFY_FILE action");
    TEST_ASSERT(has_auth_by, "AUTHORIZED_BY edge missing");
    TEST_ASSERT(has_comm_with, "COMMUNICATES_WITH edge missing for external URL resource");

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_per_agent_follows_chaining() {
    const std::string test_name = "Per-Agent FOLLOWS Edge Temporal Chaining";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    DynamicGraphEngine engine;

    // Agent Alpha events
    TelemetryEvent a1 = createTestEvent("EVT_A1", "AGENT_ALPHA", 1000, "READ", "CLI", "/tmp/file1", "READ", "", "SUCCESS");
    TelemetryEvent a2 = createTestEvent("EVT_A2", "AGENT_ALPHA", 1040, "READ", "CLI", "/tmp/file2", "READ", "", "SUCCESS");
    TelemetryEvent a3 = createTestEvent("EVT_A3", "AGENT_ALPHA", 1100, "WRITE", "CLI", "/tmp/file3", "WRITE", "", "SUCCESS");

    // Agent Beta events interleaved
    TelemetryEvent b1 = createTestEvent("EVT_B1", "AGENT_BETA", 1010, "SCAN", "NMAP", "192.168.1.1", "ADMIN", "", "SUCCESS");
    TelemetryEvent b2 = createTestEvent("EVT_B2", "AGENT_BETA", 1070, "EXPLOIT", "METASPLOIT", "192.168.1.1", "ADMIN", "", "SUCCESS");

    engine.ingestEvent(a1);
    engine.ingestEvent(b1);
    engine.ingestEvent(a2);
    engine.ingestEvent(b2);
    engine.ingestEvent(a3);

    // Verify FOLLOWS edges
    int follows_count = 0;
    int a1_to_a2_found = 0;
    int a2_to_a3_found = 0;
    int b1_to_b2_found = 0;

    int act_a1 = engine.findNodeId(NodeType::Action, "EVT_A1");
    int act_a2 = engine.findNodeId(NodeType::Action, "EVT_A2");
    int act_a3 = engine.findNodeId(NodeType::Action, "EVT_A3");
    int act_b1 = engine.findNodeId(NodeType::Action, "EVT_B1");
    int act_b2 = engine.findNodeId(NodeType::Action, "EVT_B2");

    for (const auto& edge : engine.allEdges()) {
        if (edge.relation == RelationType::Follows) {
            ++follows_count;
            if (edge.source == act_a1 && edge.destination == act_a2) {
                ++a1_to_a2_found;
                TEST_ASSERT(edge.timestamp == 1040, "FOLLOWS timestamp must match current event");
            }
            if (edge.source == act_a2 && edge.destination == act_a3) {
                ++a2_to_a3_found;
                TEST_ASSERT(edge.timestamp == 1100, "FOLLOWS timestamp must match current event");
            }
            if (edge.source == act_b1 && edge.destination == act_b2) {
                ++b1_to_b2_found;
                TEST_ASSERT(edge.timestamp == 1070, "FOLLOWS timestamp must match current event");
            }
        }
    }

    TEST_ASSERT(follows_count == 3, "Total FOLLOWS edges must be exactly 3 (2 for Alpha, 1 for Beta)");
    TEST_ASSERT(a1_to_a2_found == 1, "Alpha A1 -> A2 FOLLOWS link missing");
    TEST_ASSERT(a2_to_a3_found == 1, "Alpha A2 -> A3 FOLLOWS link missing");
    TEST_ASSERT(b1_to_b2_found == 1, "Beta B1 -> B2 FOLLOWS link missing");

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_temporal_snapshots() {
    const std::string test_name = "Temporal Snapshot Filtering (getSnapshot)";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    DynamicGraphEngine engine;

    engine.ingestEvent(createTestEvent("EVT_1", "AG_1", 100, "READ", "TOOL", "res1", "READ", "", "SUCCESS"));
    engine.ingestEvent(createTestEvent("EVT_2", "AG_1", 200, "READ", "TOOL", "res2", "READ", "", "SUCCESS"));
    engine.ingestEvent(createTestEvent("EVT_3", "AG_1", 300, "WRITE", "TOOL", "res3", "WRITE", "", "SUCCESS"));
    engine.ingestEvent(createTestEvent("EVT_4", "AG_1", 400, "EXIT", "TOOL", "res4", "EXEC", "", "SUCCESS"));

    // 1. Snapshot up to timestamp 250 (should contain EVT_1 and EVT_2, but not EVT_3 or EVT_4)
    auto snap250 = engine.getSnapshot(250);
    TEST_ASSERT(snap250.hasNode(NodeType::Action, "EVT_1"), "EVT_1 Action should be in snapshot 250");
    TEST_ASSERT(snap250.hasNode(NodeType::Action, "EVT_2"), "EVT_2 Action should be in snapshot 250");
    TEST_ASSERT(!snap250.hasNode(NodeType::Action, "EVT_3"), "EVT_3 Action must NOT be in snapshot 250");
    TEST_ASSERT(!snap250.hasNode(NodeType::Action, "EVT_4"), "EVT_4 Action must NOT be in snapshot 250");

    // Verify all edges in snap250 have timestamp <= 250
    for (const auto& edge : snap250.allEdges()) {
        TEST_ASSERT(edge.timestamp <= 250, "Snapshot edge timestamp exceeds upper limit 250");
    }

    // 2. Snapshot up to timestamp 50 (empty)
    auto snap50 = engine.getSnapshot(50);
    TEST_ASSERT(snap50.numNodes() == 0, "Snapshot 50 should have 0 nodes");
    TEST_ASSERT(snap50.numEdges() == 0, "Snapshot 50 should have 0 edges");

    // 3. Snapshot up to 1000 (all included)
    auto snap1000 = engine.getSnapshot(1000);
    TEST_ASSERT(snap1000.numNodes() == engine.numNodes(), "Snapshot 1000 should include all nodes");
    TEST_ASSERT(snap1000.numEdges() == engine.numEdges(), "Snapshot 1000 should include all edges");

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_tensor_batch_export() {
    const std::string test_name = "LibTorch Tensor Batch Export (toTensorBatch)";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    DynamicGraphEngine engine;

    engine.ingestEvent(createTestEvent("EVT_T1", "AGENT_X", 1000, "READ", "CLI", "repo/file", "READ", "", "SUCCESS"));
    engine.ingestEvent(createTestEvent("EVT_T2", "AGENT_X", 1020, "MODIFY", "CLI", "https://api.cloud.com/v1", "WRITE", "", "SUCCESS"));

    auto batch = engine.toTensorBatch();

    // Verify structural counts
    TEST_ASSERT(batch.num_nodes == engine.numNodes(), "Batch node count matches engine");
    TEST_ASSERT(batch.num_edges == engine.numEdges(), "Batch edge count matches engine");
    TEST_ASSERT(batch.raw_node_features.size() == engine.numNodes(), "Raw node feature count matches");

    // Verify all 7 relations are represented
    const std::vector<RelationType> rels = {
        RelationType::Uses,
        RelationType::Accesses,
        RelationType::Performs,
        RelationType::AuthorizedBy,
        RelationType::Modifies,
        RelationType::Follows,
        RelationType::CommunicatesWith
    };

    for (auto rel : rels) {
        TEST_ASSERT(batch.raw_relation_edge_index.count(rel) == 1, "Relation index missing in raw map");
        TEST_ASSERT(batch.raw_relation_edge_attr.count(rel) == 1, "Relation attr missing in raw map");
    }

#if NEXUS_HAS_TORCH
    std::cout << "  [INFO] LibTorch detected: asserting torch::Tensor shapes & dtypes..." << std::endl;

    // Node Features: [num_nodes, feat_dim]
    TEST_ASSERT(batch.node_features.dim() == 2, "node_features must be 2D tensor");
    TEST_ASSERT(batch.node_features.size(0) == static_cast<int64_t>(engine.numNodes()), "node_features shape[0] mismatch");
    TEST_ASSERT(batch.node_features.size(1) == static_cast<int64_t>(kNodeFeatureDim), "node_features shape[1] mismatch");
    TEST_ASSERT(batch.node_features.dtype() == torch::kFloat32, "node_features dtype must be Float32");

    // Relations: edge_index [2, E_rel] (Int64), edge_attr [E_rel, feat_dim] (Float32)
    for (auto rel : rels) {
        const auto& idx_tensor = batch.relation_edge_index.at(rel);
        const auto& attr_tensor = batch.relation_edge_attr.at(rel);

        TEST_ASSERT(idx_tensor.dim() == 2, "relation_edge_index must be 2D tensor");
        TEST_ASSERT(idx_tensor.size(0) == 2, "relation_edge_index shape[0] must be 2 (source, dest)");
        TEST_ASSERT(idx_tensor.dtype() == torch::kInt64, "relation_edge_index dtype must be Int64");

        TEST_ASSERT(attr_tensor.dim() == 2, "relation_edge_attr must be 2D tensor");
        TEST_ASSERT(attr_tensor.size(0) == idx_tensor.size(1), "Edge attribute count must match edge index count");
        TEST_ASSERT(attr_tensor.size(1) == static_cast<int64_t>(kEdgeFeatureDim), "Edge attribute dim must equal kEdgeFeatureDim");
        TEST_ASSERT(attr_tensor.dtype() == torch::kFloat32, "relation_edge_attr dtype must be Float32");
    }
#else
    std::cout << "  [INFO] LibTorch not present at compile-time: verified raw representation." << std::endl;
#endif

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "NEXUS System 02: Dynamic Graph Engine Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    test_node_and_edge_crud();
    test_deterministic_feature_encoders();
    test_telemetry_event_ingestion();
    test_per_agent_follows_chaining();
    test_temporal_snapshots();
    test_tensor_batch_export();

    std::cout << "\n========================================" << std::endl;
    if (g_failures == 0) {
        std::cout << "ALL GRAPH ENGINE TESTS PASSED SUCCESSFULLY!" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    } else {
        std::cerr << "FAILURES DETECTED: " << g_failures << " test(s) failed." << std::endl;
        std::cout << "========================================" << std::endl;
        return 1;
    }
}
