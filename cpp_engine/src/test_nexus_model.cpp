#include "attack_path.hpp"
#include "graph_engine.hpp"
#include "nexus_model.hpp"
#include "telemetry_collector.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace nexus::graph;
using namespace nexus::nn;
using namespace nexus::analysis;
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
                                std::string label = "NORMAL") {
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
    ev.is_synthetic = true;
    return ev;
}

static void test_temporal_risk_model_sequence() {
    const std::string test_name = "TemporalRiskModel Sequence Processing & 4-Class Softmax";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    // Build multi-step temporal graph
    DynamicGraphEngine engine;
    engine.ingestEvent(makeEvent("EVT_01", "AGENT_RECON", 1000, "READ_CONFIG", "GITHUB", "repository/config.env", "READ"));
    engine.ingestEvent(makeEvent("EVT_02", "AGENT_RECON", 1020, "EXTRACT_TOKEN", "VAULT", "secret/aws_prod.key", "READ"));
    engine.ingestEvent(makeEvent("EVT_03", "AGENT_RECON", 1050, "CONNECT_CLOUD", "BASH", "https://api.external-cloud.com/v1", "WRITE"));
    engine.ingestEvent(makeEvent("EVT_04", "AGENT_RECON", 1100, "DATA_EXPORT", "CURL", "https://api.external-cloud.com/v1/dump", "WRITE"));

    // Extract 4 chronological snapshots
    std::vector<GraphTensorBatch> snapshots;
    snapshots.push_back(engine.getSnapshot(1010).toTensorBatch());
    snapshots.push_back(engine.getSnapshot(1030).toTensorBatch());
    snapshots.push_back(engine.getSnapshot(1060).toTensorBatch());
    snapshots.push_back(engine.getSnapshot(1120).toTensorBatch());

    TEST_ASSERT(snapshots.size() == 4, "Should have 4 chronological snapshots");

    // Instantiate TemporalRiskModel
    TemporalRiskModel model(kNodeFeatureDim, /*graph_hidden_dim=*/32, /*gru_hidden_dim=*/32,
                            /*num_rgcn_layers=*/2, /*num_gru_layers=*/1, /*dropout=*/0.0);

    model->eval();
    RiskPrediction prediction = model->forward(snapshots);

    // Assertions on 4-class output
    TEST_ASSERT(prediction.logits.dim() == 1, "Logits must be 1D tensor");
    TEST_ASSERT(prediction.logits.size(0) == 4, "Logits must have size 4");

    TEST_ASSERT(prediction.probabilities.dim() == 1, "Probabilities must be 1D tensor");
    TEST_ASSERT(prediction.probabilities.size(0) == 4, "Probabilities must have size 4");

    // Softmax probabilities sum to 1.0
    const float prob_sum = prediction.probabilities.sum().item<float>();
    TEST_ASSERT(std::abs(prob_sum - 1.0f) < 1e-4, "Softmax probabilities must sum to 1.0");

    // Confidence and predicted class
    TEST_ASSERT(prediction.confidence >= 0.0 && prediction.confidence <= 1.0, "Confidence must be in [0, 1]");
    TEST_ASSERT(prediction.class_index >= 0 && prediction.class_index <= 3, "Class index must be in [0, 3]");
    TEST_ASSERT(prediction.predicted_class == static_cast<RiskLevel>(prediction.class_index), "Predicted class must match class index");

    // Temporal hidden state z_t
    TEST_ASSERT(prediction.temporal_state.dim() == 1, "Temporal state z_t must be 1D tensor");
    TEST_ASSERT(prediction.temporal_state.size(0) == 32, "Temporal state z_t size must match gru_hidden_dim");

    // Latest readout presence
    TEST_ASSERT(prediction.latest_readout.graph_embedding.defined(), "Latest readout graph embedding must be defined");
    TEST_ASSERT(prediction.latest_readout.node_embeddings.defined(), "Latest readout node embeddings must be defined");

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_checkpoint_persistence() {
    const std::string test_name = "Model Checkpoint Persistence (saveCheckpoint & loadCheckpoint)";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    const std::filesystem::path ckpt_path = "test_output/temporal_risk_model.pt";

    TemporalRiskModel model1(kNodeFeatureDim, 32, 32, 2, 1, 0.0);
    model1->eval();

    torch::Tensor dummy_input = torch::randn({1, 3, 32}); // [batch=1, seq=3, dim=32]
    RiskPrediction pred1 = model1->forward(dummy_input);

    // Save checkpoint
    model1->saveCheckpoint(ckpt_path);
    TEST_ASSERT(std::filesystem::exists(ckpt_path), "Checkpoint file must exist on disk");

    // Load into fresh model
    TemporalRiskModel model2(kNodeFeatureDim, 32, 32, 2, 1, 0.0);
    model2->loadCheckpoint(ckpt_path);
    model2->eval();

    RiskPrediction pred2 = model2->forward(dummy_input);

    // Logits from restored model must match exactly
    TEST_ASSERT(torch::allclose(pred1.logits, pred2.logits, 1e-5, 1e-5), "Restored model logits must match original exactly");
    TEST_ASSERT(torch::allclose(pred1.probabilities, pred2.probabilities, 1e-5, 1e-5), "Restored probabilities must match");
    TEST_ASSERT(pred1.class_index == pred2.class_index, "Restored predicted class index must match");

    // Clean up
    std::filesystem::remove(ckpt_path);

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_attack_path_reconstruction() {
    const std::string test_name = "System 04: AttackPathAnalyzer Multi-Hop Path Reconstruction";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    // Multi-step exfiltration attack sequence
    DynamicGraphEngine graph;
    graph.ingestEvent(makeEvent("EVT_A1", "MAL_AGENT", 1000, "READ_CONFIG", "GITHUB", "repository/config.env", "READ"));
    graph.ingestEvent(makeEvent("EVT_A2", "MAL_AGENT", 1020, "EXTRACT_TOKEN", "BASH", "credentials/aws.key", "READ"));
    graph.ingestEvent(makeEvent("EVT_A3", "MAL_AGENT", 1050, "EXFILTRATE", "CURL", "https://api.adversary-c2.org/leak", "WRITE"));

    GraphTensorBatch batch = graph.toTensorBatch();

    TemporalRiskModel model(kNodeFeatureDim, 32, 32, 2, 1, 0.0);
    model->eval();

    std::vector<GraphTensorBatch> seq = {batch};
    RiskPrediction prediction = model->forward(seq);

    // Force high confidence for testing analyzer
    prediction.predicted_class = RiskLevel::Critical;
    prediction.confidence = 0.95;

    AttackPathAnalyzer analyzer;
    std::vector<AttackPath> paths = analyzer.findSuspiciousPaths(graph, prediction.latest_readout, prediction, /*top_k=*/3);

    TEST_ASSERT(!paths.empty(), "Analyzer should reconstruct suspicious paths");
    TEST_ASSERT(paths.size() <= 3, "Returned paths must not exceed top_k=3");

    // Verify paths are sorted descending by score
    for (size_t i = 1; i < paths.size(); ++i) {
        TEST_ASSERT(paths[i - 1].total_score >= paths[i].total_score, "Paths must be sorted by total_score descending");
    }

    // Inspect the top path
    const auto& top_path = paths[0];
    TEST_ASSERT(!top_path.steps.empty(), "Top path must have steps");
    TEST_ASSERT(top_path.total_score > 0.0, "Top path total_score must be positive");
    TEST_ASSERT(top_path.confidence > 0.0, "Top path confidence must be positive");
    TEST_ASSERT(!top_path.summary.empty(), "Top path summary must not be empty");

    // Verify JSON serialization
    nlohmann::json path_json;
    to_json(path_json, top_path);
    TEST_ASSERT(path_json.contains("steps"), "JSON must contain 'steps'");
    TEST_ASSERT(path_json.contains("total_score"), "JSON must contain 'total_score'");
    TEST_ASSERT(path_json.contains("summary"), "JSON must contain 'summary'");

    AttackPath restored_path;
    from_json(path_json, restored_path);
    TEST_ASSERT(restored_path.node_ids == top_path.node_ids, "Restored node_ids must match");
    TEST_ASSERT(restored_path.edge_ids == top_path.edge_ids, "Restored edge_ids must match");

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "NEXUS Day 4: Temporal Model & Attack Path Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    test_temporal_risk_model_sequence();
    test_checkpoint_persistence();
    test_attack_path_reconstruction();

    std::cout << "\n========================================" << std::endl;
    if (g_failures == 0) {
        std::cout << "ALL DAY 4 TESTS PASSED SUCCESSFULLY!" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    } else {
        std::cerr << "FAILURES DETECTED: " << g_failures << " test(s) failed." << std::endl;
        std::cout << "========================================" << std::endl;
        return 1;
    }
}
