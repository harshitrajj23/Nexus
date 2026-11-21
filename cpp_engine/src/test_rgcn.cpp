#include "graph_engine.hpp"
#include "rgcn.hpp"
#include "telemetry_collector.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace nexus::graph;
using namespace nexus::nn;
using namespace nexus::telemetry;

static int g_failures = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  Assertion failed: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            test_passed = false; \
        } \
    } while (0)

static TelemetryEvent makeEvent(std::string event_id,
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

static void test_rgcn_layer_forward_and_empty_edges() {
    const std::string test_name = "RGCNLayer Forward & Empty Edge Tensors Handling";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    constexpr int64_t in_dim = 10;
    constexpr int64_t out_dim = 32;
    RGCNLayer layer(in_dim, out_dim, 7, /*dropout=*/0.0);

    // 1. Normal Multi-Relation Graph
    constexpr int64_t num_nodes = 5;
    torch::Tensor node_x = torch::randn({num_nodes, in_dim});

    std::map<RelationType, torch::Tensor> rel_edges;
    // Uses: node 0 -> node 1, node 2 -> node 3
    rel_edges[RelationType::Uses] = torch::tensor({{0, 2}, {1, 3}}, torch::kInt64);
    // Performs: node 0 -> node 4
    rel_edges[RelationType::Performs] = torch::tensor({{0}, {4}}, torch::kInt64);
    // Follows: empty [2, 0]
    rel_edges[RelationType::Follows] = torch::empty({2, 0}, torch::kInt64);
    // Accesses: empty [2, 0]
    rel_edges[RelationType::Accesses] = torch::empty({2, 0}, torch::kInt64);

    torch::Tensor out = layer->forward(node_x, rel_edges);

    TEST_ASSERT(out.dim() == 2, "Output must be 2D tensor");
    TEST_ASSERT(out.size(0) == num_nodes, "Output rows must equal num_nodes");
    TEST_ASSERT(out.size(1) == out_dim, "Output columns must equal out_dim");
    TEST_ASSERT(!torch::isnan(out).any().item<bool>(), "Output must not contain NaNs");

    // 2. All-Empty Edge Tensors
    std::map<RelationType, torch::Tensor> empty_rel_edges;
    for (auto rel : {RelationType::Uses, RelationType::Accesses, RelationType::Performs,
                     RelationType::AuthorizedBy, RelationType::Modifies, RelationType::Follows,
                     RelationType::CommunicatesWith}) {
        empty_rel_edges[rel] = torch::empty({2, 0}, torch::kInt64);
    }

    torch::Tensor out_empty = layer->forward(node_x, empty_rel_edges);
    TEST_ASSERT(out_empty.size(0) == num_nodes && out_empty.size(1) == out_dim,
                "Forward with all-empty edge tensors should compute self-loop cleanly");
    TEST_ASSERT(!torch::isnan(out_empty).any().item<bool>(), "Output with empty edges must not contain NaNs");

    // 3. Zero-Node Graph
    torch::Tensor zero_nodes = torch::empty({0, in_dim});
    torch::Tensor out_zero = layer->forward(zero_nodes, empty_rel_edges);
    TEST_ASSERT(out_zero.size(0) == 0 && out_zero.size(1) == out_dim, "Zero-node graph must return [0, out_dim]");

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_rgcn_basis_decomposition() {
    const std::string test_name = "RGCNLayer Basis Decomposition";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    constexpr int64_t in_dim = 10;
    constexpr int64_t out_dim = 24;
    constexpr int64_t num_relations = 7;
    constexpr int64_t num_bases = 3;

    RGCNLayer layer_basis(in_dim, out_dim, num_relations, /*dropout=*/0.0,
                          /*use_basis_decomp=*/true, num_bases);

    torch::Tensor node_x = torch::randn({4, in_dim});
    std::map<RelationType, torch::Tensor> edges;
    edges[RelationType::Uses] = torch::tensor({{0, 1}, {1, 2}}, torch::kInt64);
    edges[RelationType::Follows] = torch::tensor({{2}, {3}}, torch::kInt64);

    torch::Tensor out = layer_basis->forward(node_x, edges);

    TEST_ASSERT(out.size(0) == 4, "Basis decomp output rows must equal 4");
    TEST_ASSERT(out.size(1) == out_dim, "Basis decomp output cols must equal out_dim");
    TEST_ASSERT(!torch::isnan(out).any().item<bool>(), "Basis decomp output must not contain NaNs");

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_rgcn_encoder_and_readout() {
    const std::string test_name = "Multi-Layer RGCNEncoder & GraphReadout Extraction";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    // 1. Build a realistic dynamic agent graph using DynamicGraphEngine
    DynamicGraphEngine engine;
    engine.ingestEvent(makeEvent("EVT_01", "AGENT_CODER", 1000, "READ", "GITHUB", "repo/main.cpp", "READ"));
    engine.ingestEvent(makeEvent("EVT_02", "AGENT_CODER", 1020, "MODIFY", "SHELL", "/etc/hosts", "WRITE"));
    engine.ingestEvent(makeEvent("EVT_03", "AGENT_CODER", 1050, "CONNECT", "CURL", "https://api.exfil.org/v1", "WRITE"));

    GraphTensorBatch batch = engine.toTensorBatch();
    TEST_ASSERT(batch.num_nodes > 0, "GraphTensorBatch must contain nodes");
    TEST_ASSERT(batch.num_edges > 0, "GraphTensorBatch must contain edges");

    // 2. Instantiate 2-layer RGCNEncoder
    constexpr int64_t hidden_dim = 48;
    RGCNEncoder encoder(kNodeFeatureDim, hidden_dim, /*num_layers=*/2, /*dropout=*/0.1);

    // 3. Forward pass
    GraphReadout readout = encoder->forward(batch);

    // Assert graph embedding shape [hidden_dim]
    TEST_ASSERT(readout.graph_embedding.dim() == 1, "graph_embedding must be 1D vector");
    TEST_ASSERT(readout.graph_embedding.size(0) == hidden_dim, "graph_embedding dimension must match hidden_dim");

    // Assert node embeddings shape [num_nodes, hidden_dim]
    TEST_ASSERT(readout.node_embeddings.dim() == 2, "node_embeddings must be 2D tensor");
    TEST_ASSERT(readout.node_embeddings.size(0) == static_cast<int64_t>(batch.num_nodes), "node_embeddings count must match num_nodes");
    TEST_ASSERT(readout.node_embeddings.size(1) == hidden_dim, "node_embeddings columns must match hidden_dim");

    // Assert node importance shape [num_nodes] and non-negativity
    TEST_ASSERT(readout.node_importance.dim() == 1, "node_importance must be 1D tensor");
    TEST_ASSERT(readout.node_importance.size(0) == static_cast<int64_t>(batch.num_nodes), "node_importance length must match num_nodes");
    TEST_ASSERT((readout.node_importance >= 0.0f).all().item<bool>(), "node_importance scores must be non-negative");

    // Assert edge importance per relation
    for (const auto& [rel, edge_tensor] : batch.relation_edge_index) {
        TEST_ASSERT(readout.edge_importance.count(rel) == 1, "Relation must exist in edge_importance map");
        const auto& imp_tensor = readout.edge_importance.at(rel);
        const int64_t edge_count = edge_tensor.size(1);
        TEST_ASSERT(imp_tensor.size(0) == edge_count, "Edge importance count must match relation edge count");
        if (edge_count > 0) {
            TEST_ASSERT((imp_tensor >= 0.0f).all().item<bool>(), "Edge importance scores must be non-negative");
        }
    }

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_target_node_pooling() {
    const std::string test_name = "Target Node Pooling (Action & Agent Indices)";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    DynamicGraphEngine engine;
    engine.ingestEvent(makeEvent("EVT_P1", "AGENT_A", 100, "READ", "CLI", "resA"));
    engine.ingestEvent(makeEvent("EVT_P2", "AGENT_A", 150, "EXEC", "CLI", "resB"));

    GraphTensorBatch batch = engine.toTensorBatch();

    RGCNEncoder encoder(kNodeFeatureDim, /*hidden_dim=*/32, /*num_layers=*/2, /*dropout=*/0.0);

    // Target specific Action node indices
    std::vector<int64_t> target_actions;
    for (const auto& node : engine.allNodes()) {
        if (node.type == NodeType::Action) {
            target_actions.push_back(node.id);
        }
    }
    TEST_ASSERT(!target_actions.empty(), "Should have Action nodes for targeting");

    GraphReadout global_readout = encoder->forward(batch);
    GraphReadout targeted_readout = encoder->forward(batch, target_actions);

    TEST_ASSERT(targeted_readout.graph_embedding.size(0) == 32, "Targeted embedding dimension must be 32");
    TEST_ASSERT(!torch::isnan(targeted_readout.graph_embedding).any().item<bool>(), "Targeted embedding must not contain NaNs");

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_train_vs_eval_mode() {
    const std::string test_name = "Train vs Eval Mode (Dropout Determinism)";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    constexpr int64_t hidden_dim = 32;
    RGCNEncoder encoder(10, hidden_dim, /*num_layers=*/2, /*dropout=*/0.5);

    torch::Tensor node_x = torch::randn({4, 10});
    std::map<RelationType, torch::Tensor> edges;
    edges[RelationType::Uses] = torch::tensor({{0, 1}, {1, 2}}, torch::kInt64);

    // 1. Eval Mode: deterministic (same input produces exact same output)
    encoder->eval();
    torch::NoGradGuard no_grad;
    GraphReadout eval_out_1 = encoder->forward(node_x, edges);
    GraphReadout eval_out_2 = encoder->forward(node_x, edges);

    TEST_ASSERT(torch::allclose(eval_out_1.graph_embedding, eval_out_2.graph_embedding),
                "Eval mode forward must be strictly deterministic");

    // 2. Train Mode: stochastic dropout produces variance
    encoder->train();
    bool saw_difference = false;
    for (int attempt = 0; attempt < 5; ++attempt) {
        GraphReadout train_out_1 = encoder->forward(node_x, edges);
        GraphReadout train_out_2 = encoder->forward(node_x, edges);
        if (!torch::allclose(train_out_1.graph_embedding, train_out_2.graph_embedding)) {
            saw_difference = true;
            break;
        }
    }
    TEST_ASSERT(saw_difference, "Train mode with dropout=0.5 must produce different outputs across passes");

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_backward_gradient_flow() {
    const std::string test_name = "End-to-End Backward Gradient Flow & Weight Update";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    constexpr int64_t hidden_dim = 32;
    RGCNEncoder encoder(kNodeFeatureDim, hidden_dim, /*num_layers=*/2, /*dropout=*/0.0);
    torch::optim::Adam optimizer(encoder->parameters(), torch::optim::AdamOptions(0.01));

    // Dynamic graph with events
    DynamicGraphEngine engine;
    engine.ingestEvent(makeEvent("EVT_G1", "AGENT_SEC", 1000, "PROBE", "NMAP", "10.0.0.1"));
    engine.ingestEvent(makeEvent("EVT_G2", "AGENT_SEC", 1020, "LOGIN", "SSH", "10.0.0.1"));
    GraphTensorBatch batch = engine.toTensorBatch();

    encoder->train();
    optimizer.zero_grad();

    GraphReadout readout = encoder->forward(batch);
    torch::Tensor loss = readout.graph_embedding.sum() + readout.node_embeddings.sum();

    loss.backward();

    // Check all registered parameters have gradients
    int param_count = 0;
    for (const auto& param : encoder->parameters()) {
        ++param_count;
        TEST_ASSERT(param.grad().defined(), "Parameter gradient must be defined");
        TEST_ASSERT(param.grad().numel() > 0, "Parameter gradient must not be empty");
        const float grad_norm = param.grad().norm().item<float>();
        TEST_ASSERT(grad_norm >= 0.0f && !std::isnan(grad_norm), "Gradient norm must be non-negative and finite");
    }
    TEST_ASSERT(param_count > 0, "Encoder must have registered parameters");

    // Execute step
    optimizer.step();

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "NEXUS System 03: R-GCN Layer & Encoder Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    test_rgcn_layer_forward_and_empty_edges();
    test_rgcn_basis_decomposition();
    test_rgcn_encoder_and_readout();
    test_target_node_pooling();
    test_train_vs_eval_mode();
    test_backward_gradient_flow();

    std::cout << "\n========================================" << std::endl;
    if (g_failures == 0) {
        std::cout << "ALL R-GCN TESTS PASSED SUCCESSFULLY!" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    } else {
        std::cerr << "FAILURES DETECTED: " << g_failures << " test(s) failed." << std::endl;
        std::cout << "========================================" << std::endl;
        return 1;
    }
}
