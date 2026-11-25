#include "telemetry_collector.hpp"
#include "graph_engine.hpp"
#include "rgcn.hpp"
#include "nexus_model.hpp"
#include "attack_path.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <chrono>
#include <filesystem>
#include <optional>
#include <nlohmann/json.hpp>
#include <torch/torch.h>

namespace {

std::string nodeTypeName(nexus::graph::NodeType t) {
    switch (t) {
        case nexus::graph::NodeType::Agent: return "Agent";
        case nexus::graph::NodeType::Tool: return "Tool";
        case nexus::graph::NodeType::Resource: return "Resource";
        case nexus::graph::NodeType::Action: return "Action";
        case nexus::graph::NodeType::Permission: return "Permission";
        case nexus::graph::NodeType::Identity: return "Identity";
        case nexus::graph::NodeType::Destination: return "Destination";
    }
    return "Unknown";
}

std::string relationTypeName(nexus::graph::RelationType r) {
    switch (r) {
        case nexus::graph::RelationType::Uses: return "USES";
        case nexus::graph::RelationType::Accesses: return "ACCESSES";
        case nexus::graph::RelationType::Performs: return "PERFORMS";
        case nexus::graph::RelationType::AuthorizedBy: return "AUTHORIZED_BY";
        case nexus::graph::RelationType::Modifies: return "MODIFIES";
        case nexus::graph::RelationType::Follows: return "FOLLOWS";
        case nexus::graph::RelationType::CommunicatesWith: return "COMMUNICATES_WITH";
    }
    return "UNKNOWN";
}

} // namespace

int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    // Hyperparameters conforming to trained model
    constexpr int64_t kNodeFeatureDim = 10;
    constexpr int64_t kHiddenDim = 32;
    constexpr int64_t kNumRgcnLayers = 2;
    constexpr int64_t kNumGruLayers = 1;
    constexpr double kDropout = 0.0;

    std::string model_path = "data/nexus_best.pt";
    bool run_self_test = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--model" && i + 1 < argc) {
            model_path = argv[++i];
        } else if (arg == "--test") {
            run_self_test = true;
        }
    }

    // Auto-discover checkpoint path if default not found
    if (!std::filesystem::exists(model_path)) {
        if (std::filesystem::exists("../data/nexus_best.pt")) {
            model_path = "../data/nexus_best.pt";
        } else if (std::filesystem::exists("../../data/nexus_best.pt")) {
            model_path = "../../data/nexus_best.pt";
        }
    }

    // Initialize model architecture
    nexus::nn::TemporalRiskModel model(kNodeFeatureDim, kHiddenDim, kHiddenDim, kNumRgcnLayers, kNumGruLayers, kDropout);
    bool model_loaded = false;

    if (std::filesystem::exists(model_path)) {
        try {
            model->loadCheckpoint(model_path);
            model->eval();
            model_loaded = true;
            std::cerr << "[nexus_engine_bridge] Successfully loaded checkpoint: " << model_path << "\n";
        } catch (const std::exception& ex) {
            std::cerr << "[nexus_engine_bridge] Warning: Could not load checkpoint (" << ex.what() << "). Using initialized weights.\n";
            model->eval();
        }
    } else {
        std::cerr << "[nexus_engine_bridge] Notice: Checkpoint " << model_path << " not found. Using initialized weights.\n";
        model->eval();
    }

    // State maintained across streaming requests
    nexus::graph::DynamicGraphEngine engine;
    std::vector<nexus::graph::GraphTensorBatch> sequence_window;
    std::optional<torch::Tensor> running_hidden = std::nullopt;
    std::vector<nexus::analysis::AttackPath> last_paths;
    nlohmann::json last_paths_json = nlohmann::json::array();

    // Self-test mode
    if (run_self_test) {
        std::cout << "[nexus_engine_bridge] Running internal self-test...\n";
        nexus::telemetry::TelemetryEvent evt1;
        evt1.event_id = "EVT_TEST_01";
        evt1.agent_id = "agent_test";
        evt1.timestamp = 1795330000;
        evt1.action = "READ_CONFIG";
        evt1.tool = "BASH";
        evt1.resource = "credentials/config.env";
        evt1.permission = "READ";
        evt1.intent_id = "INT_TEST";
        evt1.status = "SUCCESS";
        evt1.label = "Suspicious";
        evt1.is_synthetic = false;

        engine.ingestEvent(evt1);
        auto batch = engine.toTensorBatch();
        sequence_window.push_back(batch);

        torch::NoGradGuard no_grad;
        auto pred = model->forward(sequence_window, running_hidden);
        nexus::analysis::AttackPathAnalyzer analyzer;
        auto paths = analyzer.findSuspiciousPaths(engine, pred.latest_readout, pred, 3, 5);

        std::cout << "Self-test result: class=" << nexus::nn::riskLevelToString(pred.predicted_class)
                  << ", confidence=" << pred.confidence
                  << ", reconstructed paths=" << paths.size() << "\n";
        std::cout << "SELF_TEST_PASS\n";
        return 0;
    }

    // Main JSONL stdio command loop
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        try {
            nlohmann::json req = nlohmann::json::parse(line);
            std::string cmd = req.value("cmd", "");

            if (cmd == "ping" || cmd == "health") {
                nlohmann::json resp = {
                    {"status", "ok"},
                    {"message", "nexus_engine_bridge ready"},
                    {"model_loaded", model_loaded},
                    {"model_path", model_path},
                    {"nodes_count", engine.numNodes()},
                    {"edges_count", engine.numEdges()},
                    {"sequence_length", sequence_window.size()}
                };
                std::cout << resp.dump() << "\n";
                std::cout.flush();

            } else if (cmd == "reset") {
                engine = nexus::graph::DynamicGraphEngine();
                sequence_window.clear();
                running_hidden = std::nullopt;
                last_paths.clear();
                last_paths_json = nlohmann::json::array();

                nlohmann::json resp = {
                    {"status", "ok"},
                    {"message", "Engine state reset successfully"}
                };
                std::cout << resp.dump() << "\n";
                std::cout.flush();

            } else if (cmd == "snapshot") {
                nlohmann::json nodes_json = nlohmann::json::array();
                for (const auto& node : engine.allNodes()) {
                    nodes_json.push_back({
                        {"id", node.id},
                        {"type", static_cast<int>(node.type)},
                        {"type_name", nodeTypeName(node.type)},
                        {"key", node.key},
                        {"created_at", node.created_at}
                    });
                }

                nlohmann::json edges_json = nlohmann::json::array();
                for (const auto& edge : engine.allEdges()) {
                    edges_json.push_back({
                        {"id", edge.id},
                        {"source", edge.source},
                        {"destination", edge.destination},
                        {"relation", static_cast<int>(edge.relation)},
                        {"relation_name", relationTypeName(edge.relation)},
                        {"timestamp", edge.timestamp}
                    });
                }

                nlohmann::json active_path = nullptr;
                if (!last_paths.empty()) {
                    nexus::analysis::to_json(active_path, last_paths[0]);
                }

                nlohmann::json resp = {
                    {"status", "ok"},
                    {"nodes", nodes_json},
                    {"edges", edges_json},
                    {"top_paths", last_paths_json},
                    {"active_attack_path", active_path},
                    {"num_nodes", engine.numNodes()},
                    {"num_edges", engine.numEdges()}
                };
                std::cout << resp.dump() << "\n";
                std::cout.flush();

            } else if (cmd == "ingest") {
                if (!req.contains("event")) {
                    nlohmann::json err_resp = {{"status", "error"}, {"error", "Missing 'event' object in request"}};
                    std::cout << err_resp.dump() << "\n";
                    std::cout.flush();
                    continue;
                }

                nexus::telemetry::TelemetryEvent evt = req["event"].get<nexus::telemetry::TelemetryEvent>();

                auto t_start = std::chrono::high_resolution_clock::now();

                // 1. Ingest event into DynamicGraphEngine
                engine.ingestEvent(evt);

                // 2. Export LibTorch tensor snapshot
                auto batch = engine.toTensorBatch();
                sequence_window.push_back(batch);
                if (sequence_window.size() > 8) {
                    sequence_window.erase(sequence_window.begin());
                }

                // 3. TemporalRiskModel forward inference
                torch::NoGradGuard no_grad;
                nexus::nn::RiskPrediction prediction = model->forward(sequence_window, running_hidden);
                if (prediction.temporal_state.defined()) {
                    running_hidden = prediction.temporal_state.detach();
                }

                // 4. AttackPathAnalyzer path reconstruction
                nexus::analysis::AttackPathAnalyzer analyzer;
                last_paths = analyzer.findSuspiciousPaths(engine, prediction.latest_readout, prediction, 5, 5);

                last_paths_json = nlohmann::json::array();
                for (const auto& p : last_paths) {
                    nlohmann::json pj;
                    nexus::analysis::to_json(pj, p);
                    last_paths_json.push_back(pj);
                }

                auto t_end = std::chrono::high_resolution_clock::now();
                double latency_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

                // Format probabilities
                std::vector<double> probs;
                if (prediction.probabilities.defined() && prediction.probabilities.numel() == 4) {
                    auto probs_cpu = prediction.probabilities.to(torch::kCPU).to(torch::kFloat32);
                    auto ptr = probs_cpu.data_ptr<float>();
                    for (int i = 0; i < 4; ++i) probs.push_back(static_cast<double>(ptr[i]));
                }

                // Format logits
                std::vector<double> logits;
                if (prediction.logits.defined() && prediction.logits.numel() == 4) {
                    auto logits_cpu = prediction.logits.to(torch::kCPU).to(torch::kFloat32);
                    auto ptr = logits_cpu.data_ptr<float>();
                    for (int i = 0; i < 4; ++i) logits.push_back(static_cast<double>(ptr[i]));
                }

                nlohmann::json resp = {
                    {"status", "ok"},
                    {"event_id", evt.event_id},
                    {"latency_ms", latency_ms},
                    {"prediction", {
                        {"class_name", nexus::nn::riskLevelToString(prediction.predicted_class)},
                        {"class_index", prediction.class_index},
                        {"confidence", prediction.confidence},
                        {"probabilities", probs},
                        {"logits", logits}
                    }},
                    {"attack_paths", last_paths_json},
                    {"graph_stats", {
                        {"nodes", engine.numNodes()},
                        {"edges", engine.numEdges()}
                    }}
                };
                std::cout << resp.dump() << "\n";
                std::cout.flush();

            } else {
                nlohmann::json err_resp = {{"status", "error"}, {"error", "Unknown command: " + cmd}};
                std::cout << err_resp.dump() << "\n";
                std::cout.flush();
            }

        } catch (const std::exception& ex) {
            nlohmann::json err_resp = {{"status", "error"}, {"error", std::string("Exception: ") + ex.what()}};
            std::cout << err_resp.dump() << "\n";
            std::cout.flush();
        }
    }

    return 0;
}
