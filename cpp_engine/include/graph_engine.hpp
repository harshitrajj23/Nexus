#pragma once

#include "telemetry_collector.hpp"

#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(NEXUS_HAS_TORCH) || __has_include(<torch/torch.h>)
#ifndef NEXUS_HAS_TORCH
#define NEXUS_HAS_TORCH 1
#endif
#include <torch/torch.h>
#endif

namespace nexus::graph {

// Feature dimensions
inline constexpr size_t kNodeFeatureDim = 10;
inline constexpr size_t kEdgeFeatureDim = 10;

// PRD Node types
enum class NodeType {
    Agent,
    Tool,
    Resource,
    Action,
    Permission,
    Identity,
    Destination
};

// PRD Relationship types
enum class RelationType {
    Uses,             // Agent -> Tool
    Accesses,         // Tool -> Resource
    Performs,         // Agent -> Action
    AuthorizedBy,     // Action -> Permission
    Modifies,         // Action -> Resource
    Follows,          // Action A -> Action B
    CommunicatesWith  // Agent -> Destination
};

std::string nodeTypeToString(NodeType type);
std::string relationTypeToString(RelationType relation);

struct Node {
    int id{0};
    NodeType type{NodeType::Agent};
    std::string key;
    int64_t created_at{0};
    std::vector<float> features;

    bool operator==(const Node& other) const {
        return id == other.id &&
               type == other.type &&
               key == other.key &&
               created_at == other.created_at &&
               features == other.features;
    }
};

struct Edge {
    int id{0};
    int source{0};
    int destination{0};
    RelationType relation{RelationType::Uses};
    int64_t timestamp{0};
    std::vector<float> features;

    bool operator==(const Edge& other) const {
        return id == other.id &&
               source == other.source &&
               destination == other.destination &&
               relation == other.relation &&
               timestamp == other.timestamp &&
               features == other.features;
    }
};

// Structured batch representation for graph neural processing
struct GraphTensorBatch {
#if NEXUS_HAS_TORCH
    torch::Tensor node_features;
    std::map<RelationType, torch::Tensor> relation_edge_index;
    std::map<RelationType, torch::Tensor> relation_edge_attr;
#endif

    std::vector<std::vector<float>> raw_node_features;
    std::map<RelationType, std::pair<std::vector<int64_t>, std::vector<int64_t>>> raw_relation_edge_index;
    std::map<RelationType, std::vector<std::vector<float>>> raw_relation_edge_attr;
    size_t num_nodes{0};
    size_t num_edges{0};
};

// Deterministic Feature Encoders
std::vector<float> encodeNodeFeatures(NodeType type, int64_t timestamp);
std::vector<float> encodeEdgeFeatures(RelationType relation, int64_t timestamp, int64_t dt_seconds);

class DynamicGraphEngine {
public:
    DynamicGraphEngine() = default;
    ~DynamicGraphEngine() = default;

    // Node & Edge manipulation with ID/bounds validation
    int upsertNode(NodeType type, const std::string& key, int64_t timestamp);
    int addEdge(int source, int destination, RelationType relation, int64_t timestamp, int64_t dt_seconds = 0);

    [[nodiscard]] const Node& getNode(int node_id) const;
    [[nodiscard]] Node& getNode(int node_id);

    [[nodiscard]] const Edge& getEdge(int edge_id) const;
    [[nodiscard]] Edge& getEdge(int edge_id);

    [[nodiscard]] const std::vector<int>& outgoingEdges(int node_id) const;
    [[nodiscard]] const std::vector<int>& incomingEdges(int node_id) const;

    [[nodiscard]] size_t numNodes() const noexcept { return nodes_.size(); }
    [[nodiscard]] size_t numEdges() const noexcept { return edges_.size(); }

    [[nodiscard]] const std::vector<Node>& allNodes() const noexcept { return nodes_; }
    [[nodiscard]] const std::vector<Edge>& allEdges() const noexcept { return edges_; }

    [[nodiscard]] bool hasNode(NodeType type, const std::string& key) const;
    [[nodiscard]] int findNodeId(NodeType type, const std::string& key) const;

    // Telemetry Event Ingestion
    bool ingestEvent(const nexus::telemetry::TelemetryEvent& event);

    // Temporal Snapshot
    [[nodiscard]] DynamicGraphEngine getSnapshot(int64_t up_to_timestamp) const;

    // LibTorch Tensor Export
    [[nodiscard]] GraphTensorBatch toTensorBatch() const;

    // Inspection for last action per agent
    [[nodiscard]] int getLastActionForAgent(const std::string& agent_id) const;

private:
    [[nodiscard]] static std::string makeNodeLookupKey(NodeType type, const std::string& key);
    [[nodiscard]] static bool isExternalDestination(const std::string& resource);
    [[nodiscard]] static bool isModifyingAction(const std::string& action, const std::string& permission);

    std::vector<Node> nodes_;
    std::vector<Edge> edges_;

    // Adjacency lists (store edge IDs)
    std::vector<std::vector<int>> outgoing_edges_;
    std::vector<std::vector<int>> incoming_edges_;

    // Lookup: (NodeType, key) -> node_id
    std::unordered_map<std::string, int> node_lookup_;

    // Tracks last Action node id for each agent to maintain FOLLOWS chains
    std::unordered_map<std::string, int> last_action_per_agent_;
};

} // namespace nexus::graph
