#include "graph_engine.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace nexus::graph {

std::string nodeTypeToString(NodeType type) {
    switch (type) {
        case NodeType::Agent: return "Agent";
        case NodeType::Tool: return "Tool";
        case NodeType::Resource: return "Resource";
        case NodeType::Action: return "Action";
        case NodeType::Permission: return "Permission";
        case NodeType::Identity: return "Identity";
        case NodeType::Destination: return "Destination";
    }
    return "Unknown";
}

std::string relationTypeToString(RelationType relation) {
    switch (relation) {
        case RelationType::Uses: return "USES";
        case RelationType::Accesses: return "ACCESSES";
        case RelationType::Performs: return "PERFORMS";
        case RelationType::AuthorizedBy: return "AUTHORIZED_BY";
        case RelationType::Modifies: return "MODIFIES";
        case RelationType::Follows: return "FOLLOWS";
        case RelationType::CommunicatesWith: return "COMMUNICATES_WITH";
    }
    return "UNKNOWN";
}

std::string DynamicGraphEngine::makeNodeLookupKey(NodeType type, const std::string& key) {
    return std::to_string(static_cast<int>(type)) + "::" + key;
}

bool DynamicGraphEngine::isExternalDestination(const std::string& resource) {
    if (resource.rfind("http://", 0) == 0 ||
        resource.rfind("https://", 0) == 0 ||
        resource.find("://") != std::string::npos ||
        resource.rfind("api.", 0) == 0 ||
        resource.find(".com") != std::string::npos ||
        resource.find(".org") != std::string::npos ||
        resource.find(".net") != std::string::npos ||
        resource.find(".io") != std::string::npos ||
        resource.rfind("external:", 0) == 0) {
        return true;
    }
    return false;
}

bool DynamicGraphEngine::isModifyingAction(const std::string& action, const std::string& permission) {
    if (permission == "WRITE" || permission == "READ_WRITE" || permission == "ADMIN" || permission == "MODIFY") {
        return true;
    }
    if (action == "WRITE" || action == "MODIFY" || action == "DELETE" ||
        action == "UPDATE" || action == "CREATE" || action == "POST" || action == "PUT") {
        return true;
    }
    if (action.find("WRITE") != std::string::npos ||
        action.find("MODIF") != std::string::npos ||
        action.find("DELETE") != std::string::npos ||
        action.find("UPDATE") != std::string::npos) {
        return true;
    }
    return false;
}

std::vector<float> encodeNodeFeatures(NodeType type, int64_t timestamp) {
    std::vector<float> features(kNodeFeatureDim, 0.0f);

    // 1. One-hot encoding for 7 NodeTypes (indices 0..6)
    const auto type_idx = static_cast<size_t>(type);
    if (type_idx < 7) {
        features[type_idx] = 1.0f;
    }

    // 2. Normalized continuous epoch timestamp (index 7)
    // Clamped across plausible range [0, 2,000,000,000]
    const float safe_ts = static_cast<float>(std::max<int64_t>(0, timestamp));
    features[7] = std::clamp(safe_ts / 2000000000.0f, 0.0f, 1.0f);

    // 3. Cyclic diurnal temporal features: sin/cos of day fraction (indices 8, 9)
    constexpr float kTwoPi = 6.28318530717958647692f;
    const int64_t sec_in_day = (timestamp >= 0) ? (timestamp % 86400) : 0;
    const float day_frac = static_cast<float>(sec_in_day) / 86400.0f;
    features[8] = std::sin(kTwoPi * day_frac);
    features[9] = std::cos(kTwoPi * day_frac);

    return features;
}

std::vector<float> encodeEdgeFeatures(RelationType relation, int64_t timestamp, int64_t dt_seconds) {
    std::vector<float> features(kEdgeFeatureDim, 0.0f);

    // 1. One-hot encoding for 7 RelationTypes (indices 0..6)
    const auto rel_idx = static_cast<size_t>(relation);
    if (rel_idx < 7) {
        features[rel_idx] = 1.0f;
    }

    // 2. Normalized temporal delta (dt) clamped to 1 hour (index 7)
    const float safe_dt = static_cast<float>(std::max<int64_t>(0, dt_seconds));
    features[7] = std::clamp(safe_dt / 3600.0f, 0.0f, 1.0f);

    // 3. Log-scaled temporal delta: log1p(dt) / log(86400) (index 8)
    constexpr float kLogMaxDay = 11.366743f; // std::log(86400)
    features[8] = std::clamp(std::log1p(safe_dt) / kLogMaxDay, 0.0f, 1.0f);

    // 4. Normalized timestamp (index 9)
    const float safe_ts = static_cast<float>(std::max<int64_t>(0, timestamp));
    features[9] = std::clamp(safe_ts / 2000000000.0f, 0.0f, 1.0f);

    return features;
}

int DynamicGraphEngine::upsertNode(NodeType type, const std::string& key, int64_t timestamp) {
    const std::string lookup_key = makeNodeLookupKey(type, key);
    auto it = node_lookup_.find(lookup_key);
    if (it != node_lookup_.end()) {
        return it->second;
    }

    const int new_id = static_cast<int>(nodes_.size());
    Node node;
    node.id = new_id;
    node.type = type;
    node.key = key;
    node.created_at = timestamp;
    node.features = encodeNodeFeatures(type, timestamp);

    nodes_.push_back(std::move(node));
    outgoing_edges_.emplace_back();
    incoming_edges_.emplace_back();
    node_lookup_[lookup_key] = new_id;

    return new_id;
}

int DynamicGraphEngine::addEdge(int source, int destination, RelationType relation, int64_t timestamp, int64_t dt_seconds) {
    if (source < 0 || source >= static_cast<int>(nodes_.size())) {
        std::ostringstream oss;
        oss << "DynamicGraphEngine::addEdge invalid source ID: " << source
            << " (nodes count: " << nodes_.size() << ")";
        throw std::out_of_range(oss.str());
    }
    if (destination < 0 || destination >= static_cast<int>(nodes_.size())) {
        std::ostringstream oss;
        oss << "DynamicGraphEngine::addEdge invalid destination ID: " << destination
            << " (nodes count: " << nodes_.size() << ")";
        throw std::out_of_range(oss.str());
    }

    const int new_edge_id = static_cast<int>(edges_.size());
    Edge edge;
    edge.id = new_edge_id;
    edge.source = source;
    edge.destination = destination;
    edge.relation = relation;
    edge.timestamp = timestamp;
    edge.features = encodeEdgeFeatures(relation, timestamp, dt_seconds);

    edges_.push_back(std::move(edge));
    outgoing_edges_[source].push_back(new_edge_id);
    incoming_edges_[destination].push_back(new_edge_id);

    return new_edge_id;
}

const Node& DynamicGraphEngine::getNode(int node_id) const {
    if (node_id < 0 || node_id >= static_cast<int>(nodes_.size())) {
        throw std::out_of_range("DynamicGraphEngine::getNode index out of range: " + std::to_string(node_id));
    }
    return nodes_[node_id];
}

Node& DynamicGraphEngine::getNode(int node_id) {
    if (node_id < 0 || node_id >= static_cast<int>(nodes_.size())) {
        throw std::out_of_range("DynamicGraphEngine::getNode index out of range: " + std::to_string(node_id));
    }
    return nodes_[node_id];
}

const Edge& DynamicGraphEngine::getEdge(int edge_id) const {
    if (edge_id < 0 || edge_id >= static_cast<int>(edges_.size())) {
        throw std::out_of_range("DynamicGraphEngine::getEdge index out of range: " + std::to_string(edge_id));
    }
    return edges_[edge_id];
}

Edge& DynamicGraphEngine::getEdge(int edge_id) {
    if (edge_id < 0 || edge_id >= static_cast<int>(edges_.size())) {
        throw std::out_of_range("DynamicGraphEngine::getEdge index out of range: " + std::to_string(edge_id));
    }
    return edges_[edge_id];
}

const std::vector<int>& DynamicGraphEngine::outgoingEdges(int node_id) const {
    if (node_id < 0 || node_id >= static_cast<int>(outgoing_edges_.size())) {
        throw std::out_of_range("DynamicGraphEngine::outgoingEdges index out of range: " + std::to_string(node_id));
    }
    return outgoing_edges_[node_id];
}

const std::vector<int>& DynamicGraphEngine::incomingEdges(int node_id) const {
    if (node_id < 0 || node_id >= static_cast<int>(incoming_edges_.size())) {
        throw std::out_of_range("DynamicGraphEngine::incomingEdges index out of range: " + std::to_string(node_id));
    }
    return incoming_edges_[node_id];
}

bool DynamicGraphEngine::hasNode(NodeType type, const std::string& key) const {
    return node_lookup_.find(makeNodeLookupKey(type, key)) != node_lookup_.end();
}

int DynamicGraphEngine::findNodeId(NodeType type, const std::string& key) const {
    auto it = node_lookup_.find(makeNodeLookupKey(type, key));
    if (it != node_lookup_.end()) {
        return it->second;
    }
    return -1;
}

int DynamicGraphEngine::getLastActionForAgent(const std::string& agent_id) const {
    auto it = last_action_per_agent_.find(agent_id);
    if (it != last_action_per_agent_.end()) {
        return it->second;
    }
    return -1;
}

bool DynamicGraphEngine::ingestEvent(const nexus::telemetry::TelemetryEvent& event) {
    if (event.event_id.empty() || event.agent_id.empty() || event.action.empty() ||
        event.tool.empty() || event.timestamp < 0) {
        return false;
    }

    // 1. Agent Node
    const int agent_id = upsertNode(NodeType::Agent, event.agent_id, event.timestamp);

    // 2. Action Node (Keyed uniquely by event_id)
    const int action_id = upsertNode(NodeType::Action, event.event_id, event.timestamp);

    // 3. Agent -> Action (PERFORMS)
    addEdge(agent_id, action_id, RelationType::Performs, event.timestamp);

    // 4. Sequential FOLLOWS edge from the previous Action node of this agent
    auto it = last_action_per_agent_.find(event.agent_id);
    if (it != last_action_per_agent_.end()) {
        const int prev_action_id = it->second;
        const int64_t prev_ts = getNode(prev_action_id).created_at;
        const int64_t dt = std::max<int64_t>(0, event.timestamp - prev_ts);
        addEdge(prev_action_id, action_id, RelationType::Follows, event.timestamp, dt);
    }
    last_action_per_agent_[event.agent_id] = action_id;

    // 5. Tool Node & Agent -> Tool (USES)
    int tool_id = -1;
    if (!event.tool.empty()) {
        tool_id = upsertNode(NodeType::Tool, event.tool, event.timestamp);
        addEdge(agent_id, tool_id, RelationType::Uses, event.timestamp);
    }

    // 6. Resource and Destination Nodes
    if (!event.resource.empty()) {
        if (isExternalDestination(event.resource)) {
            const int dest_id = upsertNode(NodeType::Destination, event.resource, event.timestamp);
            addEdge(agent_id, dest_id, RelationType::CommunicatesWith, event.timestamp);
        }

        const int res_id = upsertNode(NodeType::Resource, event.resource, event.timestamp);

        // Tool -> Resource (ACCESSES)
        if (tool_id >= 0) {
            addEdge(tool_id, res_id, RelationType::Accesses, event.timestamp);
        }

        // Action -> Resource (MODIFIES) for mutating actions/permissions
        if (isModifyingAction(event.action, event.permission)) {
            addEdge(action_id, res_id, RelationType::Modifies, event.timestamp);
        }
    }

    // 7. Permission Node & Action -> Permission (AUTHORIZED_BY)
    if (!event.permission.empty()) {
        const int perm_id = upsertNode(NodeType::Permission, event.permission, event.timestamp);
        addEdge(action_id, perm_id, RelationType::AuthorizedBy, event.timestamp);
    }

    return true;
}

DynamicGraphEngine DynamicGraphEngine::getSnapshot(int64_t up_to_timestamp) const {
    DynamicGraphEngine snapshot;
    std::unordered_map<int, int> old_to_new;

    // Filter nodes created up to the timestamp
    for (const auto& node : nodes_) {
        if (node.created_at <= up_to_timestamp) {
            const int new_id = snapshot.upsertNode(node.type, node.key, node.created_at);
            old_to_new[node.id] = new_id;
        }
    }

    // Filter edges occurring up to the timestamp whose incident nodes exist in the snapshot
    for (const auto& edge : edges_) {
        if (edge.timestamp <= up_to_timestamp) {
            auto src_it = old_to_new.find(edge.source);
            auto dst_it = old_to_new.find(edge.destination);
            if (src_it != old_to_new.end() && dst_it != old_to_new.end()) {
                // Determine dt for FOLLOWS edge
                int64_t dt = 0;
                if (edge.relation == RelationType::Follows) {
                    const int64_t src_ts = getNode(edge.source).created_at;
                    dt = std::max<int64_t>(0, edge.timestamp - src_ts);
                }
                snapshot.addEdge(src_it->second, dst_it->second, edge.relation, edge.timestamp, dt);
            }
        }
    }

    // Reconstruct last_action_per_agent_ state within the snapshot window
    for (const auto& [agent, last_act_id] : last_action_per_agent_) {
        // Find latest action for this agent in snapshot
        int latest_snap_act_id = -1;
        int64_t max_ts = -1;
        for (const auto& snap_node : snapshot.allNodes()) {
            if (snap_node.type == NodeType::Action) {
                // Find if performs edge exists from this agent to this action
                for (int in_edge_id : snapshot.incomingEdges(snap_node.id)) {
                    const auto& in_edge = snapshot.getEdge(in_edge_id);
                    if (in_edge.relation == RelationType::Performs) {
                        const auto& agent_node = snapshot.getNode(in_edge.source);
                        if (agent_node.key == agent && snap_node.created_at > max_ts) {
                            max_ts = snap_node.created_at;
                            latest_snap_act_id = snap_node.id;
                        }
                    }
                }
            }
        }
        if (latest_snap_act_id >= 0) {
            snapshot.last_action_per_agent_[agent] = latest_snap_act_id;
        }
    }

    return snapshot;
}

GraphTensorBatch DynamicGraphEngine::toTensorBatch() const {
    GraphTensorBatch batch;
    batch.num_nodes = nodes_.size();
    batch.num_edges = edges_.size();

    // 1. Raw Node Features
    batch.raw_node_features.reserve(nodes_.size());
    for (const auto& node : nodes_) {
        batch.raw_node_features.push_back(node.features);
    }

    // 2. Initialize all relation buckets
    const std::vector<RelationType> all_relations = {
        RelationType::Uses,
        RelationType::Accesses,
        RelationType::Performs,
        RelationType::AuthorizedBy,
        RelationType::Modifies,
        RelationType::Follows,
        RelationType::CommunicatesWith
    };

    for (const auto rel : all_relations) {
        batch.raw_relation_edge_index[rel] = {std::vector<int64_t>{}, std::vector<int64_t>{}};
        batch.raw_relation_edge_attr[rel] = std::vector<std::vector<float>>{};
    }

    // 3. Populate relation buckets from graph edges
    for (const auto& edge : edges_) {
        auto& [sources, targets] = batch.raw_relation_edge_index[edge.relation];
        sources.push_back(static_cast<int64_t>(edge.source));
        targets.push_back(static_cast<int64_t>(edge.destination));
        batch.raw_relation_edge_attr[edge.relation].push_back(edge.features);
    }

#if NEXUS_HAS_TORCH
    // 4. Construct LibTorch Tensors
    const int64_t num_nodes_i64 = static_cast<int64_t>(nodes_.size());
    const int64_t node_dim_i64 = static_cast<int64_t>(kNodeFeatureDim);
    const int64_t edge_dim_i64 = static_cast<int64_t>(kEdgeFeatureDim);

    if (num_nodes_i64 == 0) {
        batch.node_features = torch::empty({0, node_dim_i64}, torch::kFloat32);
    } else {
        batch.node_features = torch::empty({num_nodes_i64, node_dim_i64}, torch::kFloat32);
        float* node_data = batch.node_features.data_ptr<float>();
        for (int64_t i = 0; i < num_nodes_i64; ++i) {
            std::copy(nodes_[i].features.begin(), nodes_[i].features.end(), node_data + i * node_dim_i64);
        }
    }

    for (const auto rel : all_relations) {
        const auto& [src_vec, dst_vec] = batch.raw_relation_edge_index[rel];
        const int64_t rel_edges = static_cast<int64_t>(src_vec.size());

        if (rel_edges == 0) {
            batch.relation_edge_index[rel] = torch::empty({2, 0}, torch::kInt64);
            batch.relation_edge_attr[rel] = torch::empty({0, edge_dim_i64}, torch::kFloat32);
        } else {
            batch.relation_edge_index[rel] = torch::empty({2, rel_edges}, torch::kInt64);
            int64_t* idx_data = batch.relation_edge_index[rel].data_ptr<int64_t>();
            std::copy(src_vec.begin(), src_vec.end(), idx_data);
            std::copy(dst_vec.begin(), dst_vec.end(), idx_data + rel_edges);

            batch.relation_edge_attr[rel] = torch::empty({rel_edges, edge_dim_i64}, torch::kFloat32);
            float* attr_data = batch.relation_edge_attr[rel].data_ptr<float>();
            const auto& attr_list = batch.raw_relation_edge_attr[rel];
            for (int64_t e = 0; e < rel_edges; ++e) {
                std::copy(attr_list[e].begin(), attr_list[e].end(), attr_data + e * edge_dim_i64);
            }
        }
    }
#endif

    return batch;
}

} // namespace nexus::graph
