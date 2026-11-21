#pragma once

#include "graph_engine.hpp"
#include "nexus_model.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace nexus::analysis {

struct PathStep {
    int node_id{-1};
    nexus::graph::NodeType node_type{nexus::graph::NodeType::Agent};
    std::string node_key;
    int edge_id{-1};
    nexus::graph::RelationType relation{nexus::graph::RelationType::Uses};
    int64_t timestamp{0};
    double edge_score{0.0};

    bool operator==(const PathStep& o) const {
        return node_id == o.node_id &&
               node_type == o.node_type &&
               node_key == o.node_key &&
               edge_id == o.edge_id &&
               relation == o.relation &&
               timestamp == o.timestamp;
    }
};

struct AttackPath {
    std::vector<int> node_ids;
    std::vector<int> edge_ids;
    std::vector<PathStep> steps;
    double total_score{0.0};
    double confidence{0.0};
    std::string summary;

    bool operator==(const AttackPath& o) const {
        return node_ids == o.node_ids &&
               edge_ids == o.edge_ids &&
               steps == o.steps &&
               summary == o.summary;
    }
};

void to_json(nlohmann::json& j, const PathStep& s);
void from_json(const nlohmann::json& j, PathStep& s);

void to_json(nlohmann::json& j, const AttackPath& p);
void from_json(const nlohmann::json& j, AttackPath& p);

class AttackPathAnalyzer {
public:
    AttackPathAnalyzer() = default;
    ~AttackPathAnalyzer() = default;

    // Reconstruct and rank top-k suspicious multi-hop paths combining attribution, causality & risk chains
    [[nodiscard]] std::vector<AttackPath> findSuspiciousPaths(
        const nexus::graph::DynamicGraphEngine& graph,
        const nexus::nn::GraphReadout& readout,
        const nexus::nn::RiskPrediction& prediction,
        size_t top_k = 5,
        int max_depth = 6) const;

private:
    [[nodiscard]] double getEdgeAttribution(int edge_id,
                                            const nexus::graph::Edge& edge,
                                            const nexus::nn::GraphReadout& readout) const;

    [[nodiscard]] static bool isHighRiskResource(const std::string& key);
    [[nodiscard]] static bool isExternalDestination(const std::string& key);

    [[nodiscard]] std::string generateSummary(
        const AttackPath& path,
        const nexus::nn::RiskPrediction& prediction) const;
};

} // namespace nexus::analysis
