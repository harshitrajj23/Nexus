#include "attack_path.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace nexus::analysis {

void to_json(nlohmann::json& j, const PathStep& s) {
    j = nlohmann::json{
        {"node_id", s.node_id},
        {"node_type", nexus::graph::nodeTypeToString(s.node_type)},
        {"node_key", s.node_key},
        {"edge_id", s.edge_id},
        {"relation", nexus::graph::relationTypeToString(s.relation)},
        {"timestamp", s.timestamp},
        {"edge_score", s.edge_score}
    };
}

void from_json(const nlohmann::json& j, PathStep& s) {
    j.at("node_id").get_to(s.node_id);
    j.at("node_key").get_to(s.node_key);
    j.at("edge_id").get_to(s.edge_id);
    j.at("timestamp").get_to(s.timestamp);
    if (j.contains("edge_score")) {
        j.at("edge_score").get_to(s.edge_score);
    }

    std::string rel_str;
    j.at("relation").get_to(rel_str);
    if (rel_str == "USES") s.relation = nexus::graph::RelationType::Uses;
    else if (rel_str == "ACCESSES") s.relation = nexus::graph::RelationType::Accesses;
    else if (rel_str == "PERFORMS") s.relation = nexus::graph::RelationType::Performs;
    else if (rel_str == "AUTHORIZED_BY") s.relation = nexus::graph::RelationType::AuthorizedBy;
    else if (rel_str == "MODIFIES") s.relation = nexus::graph::RelationType::Modifies;
    else if (rel_str == "FOLLOWS") s.relation = nexus::graph::RelationType::Follows;
    else if (rel_str == "COMMUNICATES_WITH") s.relation = nexus::graph::RelationType::CommunicatesWith;

    std::string type_str;
    j.at("node_type").get_to(type_str);
    if (type_str == "Agent") s.node_type = nexus::graph::NodeType::Agent;
    else if (type_str == "Tool") s.node_type = nexus::graph::NodeType::Tool;
    else if (type_str == "Resource") s.node_type = nexus::graph::NodeType::Resource;
    else if (type_str == "Action") s.node_type = nexus::graph::NodeType::Action;
    else if (type_str == "Permission") s.node_type = nexus::graph::NodeType::Permission;
    else if (type_str == "Identity") s.node_type = nexus::graph::NodeType::Identity;
    else if (type_str == "Destination") s.node_type = nexus::graph::NodeType::Destination;
}

void to_json(nlohmann::json& j, const AttackPath& p) {
    j = nlohmann::json{
        {"node_ids", p.node_ids},
        {"edge_ids", p.edge_ids},
        {"steps", p.steps},
        {"total_score", p.total_score},
        {"confidence", p.confidence},
        {"summary", p.summary}
    };
}

void from_json(const nlohmann::json& j, AttackPath& p) {
    j.at("node_ids").get_to(p.node_ids);
    j.at("edge_ids").get_to(p.edge_ids);
    j.at("steps").get_to(p.steps);
    j.at("total_score").get_to(p.total_score);
    j.at("confidence").get_to(p.confidence);
    j.at("summary").get_to(p.summary);
}

bool AttackPathAnalyzer::isHighRiskResource(const std::string& key) {
    std::string lower = key;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower.find("config") != std::string::npos ||
           lower.find("secret") != std::string::npos ||
           lower.find("token") != std::string::npos ||
           lower.find("credential") != std::string::npos ||
           lower.find("key") != std::string::npos ||
           lower.find("password") != std::string::npos ||
           lower.find("passwd") != std::string::npos ||
           lower.find(".env") != std::string::npos ||
           lower.find("shadow") != std::string::npos ||
           lower.find("auth") != std::string::npos;
}

bool AttackPathAnalyzer::isExternalDestination(const std::string& key) {
    return key.rfind("http://", 0) == 0 ||
           key.rfind("https://", 0) == 0 ||
           key.find("://") != std::string::npos ||
           key.rfind("api.", 0) == 0 ||
           key.find(".com") != std::string::npos ||
           key.find(".org") != std::string::npos ||
           key.find(".io") != std::string::npos ||
           key.rfind("external:", 0) == 0;
}

double AttackPathAnalyzer::getEdgeAttribution(
    int /*edge_id*/,
    const nexus::graph::Edge& edge,
    const nexus::nn::GraphReadout& readout) const {
    auto it = readout.edge_importance.find(edge.relation);
    if (it != readout.edge_importance.end() && it->second.defined() && it->second.numel() > 0) {
        // Find index of this edge among edges of the same relation
        // Attribution score from RGCN readout
        return std::max(0.1, it->second.mean().item<double>());
    }
    return 1.0;
}

std::string AttackPathAnalyzer::generateSummary(
    const AttackPath& path,
    const nexus::nn::RiskPrediction& prediction) const {
    std::ostringstream oss;
    oss << "[" << nexus::nn::riskLevelToString(prediction.predicted_class)
        << "] Attack path detected (" << std::fixed << std::setprecision(1)
        << (prediction.confidence * 100.0) << "% confidence): ";

    for (size_t i = 0; i < path.steps.size(); ++i) {
        const auto& s = path.steps[i];
        if (i == 0) {
            oss << nexus::graph::nodeTypeToString(s.node_type) << "(" << s.node_key << ")";
        } else {
            oss << " -> " << nexus::graph::relationTypeToString(s.relation)
                << " -> " << nexus::graph::nodeTypeToString(s.node_type)
                << "(" << s.node_key << ")";
        }
    }
    oss << " [Score: " << std::fixed << std::setprecision(2) << path.total_score << "]";
    return oss.str();
}

std::vector<AttackPath> AttackPathAnalyzer::findSuspiciousPaths(
    const nexus::graph::DynamicGraphEngine& graph,
    const nexus::nn::GraphReadout& readout,
    const nexus::nn::RiskPrediction& prediction,
    size_t top_k,
    int max_depth) const {
    std::vector<AttackPath> candidate_paths;

    if (graph.numNodes() == 0 || graph.numEdges() == 0) {
        return candidate_paths;
    }

    // Node importance tensor
    std::vector<double> node_scores(graph.numNodes(), 1.0);
    if (readout.node_importance.defined() && readout.node_importance.numel() == static_cast<int64_t>(graph.numNodes())) {
        auto node_imp_cpu = readout.node_importance.cpu();
        const float* ptr = node_imp_cpu.data_ptr<float>();
        for (size_t i = 0; i < graph.numNodes(); ++i) {
            node_scores[i] = std::max(0.1, static_cast<double>(ptr[i]));
        }
    }

    // Helper DFS state
    struct DFSState {
        int current_node;
        int64_t last_timestamp;
        std::vector<int> node_trail;
        std::vector<int> edge_trail;
        std::vector<PathStep> steps;
        double accumulated_score;
        bool has_credential_access;
        bool has_external_exfil;
        bool has_modifies;
    };

    std::vector<DFSState> stack;

    // Start DFS from Agent or Action nodes
    for (const auto& node : graph.allNodes()) {
        if (node.type == nexus::graph::NodeType::Agent || node.type == nexus::graph::NodeType::Action) {
            DFSState init;
            init.current_node = node.id;
            init.last_timestamp = node.created_at;
            init.node_trail.push_back(node.id);
            init.accumulated_score = node_scores[node.id];
            init.has_credential_access = false;
            init.has_external_exfil = false;
            init.has_modifies = false;

            PathStep first_step;
            first_step.node_id = node.id;
            first_step.node_type = node.type;
            first_step.node_key = node.key;
            first_step.edge_id = -1;
            first_step.relation = nexus::graph::RelationType::Performs;
            first_step.timestamp = node.created_at;
            first_step.edge_score = node_scores[node.id];
            init.steps.push_back(first_step);

            stack.push_back(std::move(init));
        }
    }

    while (!stack.empty()) {
        DFSState cur = std::move(stack.back());
        stack.pop_back();

        // If path length >= 2 and terminates at a high-risk entity or Action
        if (cur.node_trail.size() >= 2) {
            const auto& end_node = graph.getNode(cur.current_node);
            const bool is_target_worthy = (end_node.type == nexus::graph::NodeType::Destination) ||
                                          (end_node.type == nexus::graph::NodeType::Resource && isHighRiskResource(end_node.key)) ||
                                          (end_node.type == nexus::graph::NodeType::Action && (cur.has_credential_access || cur.has_external_exfil || cur.has_modifies));

            if (is_target_worthy || cur.node_trail.size() >= 3) {
                double multiplier = 1.0;
                if (cur.has_credential_access) multiplier *= 1.8;
                if (cur.has_external_exfil) multiplier *= 2.0;
                if (cur.has_modifies) multiplier *= 1.5;

                AttackPath path;
                path.node_ids = cur.node_trail;
                path.edge_ids = cur.edge_trail;
                path.steps = cur.steps;
                path.total_score = cur.accumulated_score * multiplier;
                path.confidence = std::clamp(prediction.confidence * (1.0 - std::exp(-path.total_score / 10.0)), 0.1, 0.99);
                path.summary = generateSummary(path, prediction);

                candidate_paths.push_back(std::move(path));
            }
        }

        if (static_cast<int>(cur.node_trail.size()) >= max_depth) {
            continue;
        }

        // Explore outgoing edges
        for (int out_edge_id : graph.outgoingEdges(cur.current_node)) {
            const auto& edge = graph.getEdge(out_edge_id);

            // Avoid repeating edges or creating tight loops
            if (std::find(cur.edge_trail.begin(), cur.edge_trail.end(), out_edge_id) != cur.edge_trail.end()) {
                continue;
            }
            if (std::find(cur.node_trail.begin(), cur.node_trail.end(), edge.destination) != cur.node_trail.end()) {
                continue;
            }

            // Temporal causality check: allow slight tolerance (e.g. 0s or positive dt)
            if (edge.timestamp < cur.last_timestamp) {
                continue;
            }

            const auto& dst_node = graph.getNode(edge.destination);

            DFSState next = cur;
            next.current_node = edge.destination;
            next.last_timestamp = edge.timestamp;
            next.node_trail.push_back(edge.destination);
            next.edge_trail.push_back(out_edge_id);

            const double edge_score = getEdgeAttribution(out_edge_id, edge, readout);
            next.accumulated_score += (edge_score + node_scores[edge.destination]);

            if (dst_node.type == nexus::graph::NodeType::Resource && isHighRiskResource(dst_node.key)) {
                next.has_credential_access = true;
            }
            if (dst_node.type == nexus::graph::NodeType::Destination || isExternalDestination(dst_node.key) || edge.relation == nexus::graph::RelationType::CommunicatesWith) {
                next.has_external_exfil = true;
            }
            if (edge.relation == nexus::graph::RelationType::Modifies) {
                next.has_modifies = true;
            }

            PathStep step;
            step.node_id = dst_node.id;
            step.node_type = dst_node.type;
            step.node_key = dst_node.key;
            step.edge_id = out_edge_id;
            step.relation = edge.relation;
            step.timestamp = edge.timestamp;
            step.edge_score = edge_score;
            next.steps.push_back(step);

            stack.push_back(std::move(next));
        }
    }

    // Sort paths by total_score descending
    std::sort(candidate_paths.begin(), candidate_paths.end(), [](const AttackPath& a, const AttackPath& b) {
        return a.total_score > b.total_score;
    });

    // Deduplicate paths with identical node trails
    std::vector<AttackPath> unique_paths;
    std::unordered_set<std::string> seen_trails;

    for (auto& path : candidate_paths) {
        std::string trail_key;
        for (int nid : path.node_ids) {
            trail_key += std::to_string(nid) + "->";
        }
        if (seen_trails.find(trail_key) == seen_trails.end()) {
            seen_trails.insert(trail_key);
            unique_paths.push_back(std::move(path));
            if (unique_paths.size() >= top_k) {
                break;
            }
        }
    }

    return unique_paths;
}

} // namespace nexus::analysis
