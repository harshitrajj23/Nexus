#pragma once

#include "graph_engine.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <vector>
#include <torch/torch.h>

namespace nexus::nn {

// Structured readout holding graph and node representations alongside attribution metrics
struct GraphReadout {
    torch::Tensor graph_embedding;                                      // [hidden_dim]
    torch::Tensor node_embeddings;                                       // [num_nodes, hidden_dim]
    std::map<nexus::graph::RelationType, torch::Tensor> edge_importance; // Non-negative scores per relation [E_r]
    torch::Tensor node_importance;                                       // Non-negative importance [num_nodes]
};

// ----------------------------------------------------------------------------
// RGCNLayer: Relation-aware Graph Convolutional Network Layer
// Computes:
//   h_i^{(l+1)} = \sigma( \sum_r \sum_{j \in N_i^r} (1 / c_{i,r}) W_r^{(l)} h_j^{(l)} + W_0^{(l)} h_i^{(l)} )
// ----------------------------------------------------------------------------
class RGCNLayerImpl : public torch::nn::Module {
public:
    RGCNLayerImpl(int64_t in_features,
                  int64_t out_features,
                  int64_t num_relations = 7,
                  double dropout = 0.1,
                  bool use_basis_decomp = false,
                  int64_t num_bases = 0);

    torch::Tensor forward(const torch::Tensor& node_x,
                          const std::map<nexus::graph::RelationType, torch::Tensor>& relation_edge_index,
                          const std::map<nexus::graph::RelationType, torch::Tensor>& relation_edge_attr = {});

    [[nodiscard]] int64_t in_features() const noexcept { return in_features_; }
    [[nodiscard]] int64_t out_features() const noexcept { return out_features_; }
    [[nodiscard]] int64_t num_relations() const noexcept { return num_relations_; }
    [[nodiscard]] double dropout_rate() const noexcept { return dropout_rate_; }
    [[nodiscard]] bool use_basis_decomp() const noexcept { return use_basis_decomp_; }
    [[nodiscard]] int64_t num_bases() const noexcept { return num_bases_; }

private:
    int64_t in_features_;
    int64_t out_features_;
    int64_t num_relations_;
    double dropout_rate_;
    bool use_basis_decomp_;
    int64_t num_bases_;

    // Learnable weights
    torch::Tensor weight_rel_;    // Direct: [num_relations, in_features, out_features]
    torch::Tensor basis_weights_; // Basis decomp: [num_bases, in_features, out_features]
    torch::Tensor basis_coeffs_;  // Basis decomp: [num_relations, num_bases]
    torch::Tensor weight_self_;   // Self-loop W_0: [in_features, out_features]
    torch::Tensor bias_;          // Bias: [out_features]
};
TORCH_MODULE(RGCNLayer);

// ----------------------------------------------------------------------------
// RGCNEncoder: Multi-Layer RGCN Encoder with Residuals, Pooling & Attributions
// ----------------------------------------------------------------------------
class RGCNEncoderImpl : public torch::nn::Module {
public:
    RGCNEncoderImpl(int64_t in_dim,
                    int64_t hidden_dim,
                    int64_t num_layers = 2,
                    double dropout = 0.1,
                    int64_t num_relations = 7,
                    bool use_basis_decomp = false,
                    int64_t num_bases = 0);

    GraphReadout forward(const torch::Tensor& node_x,
                         const std::map<nexus::graph::RelationType, torch::Tensor>& relation_edge_index,
                         const std::map<nexus::graph::RelationType, torch::Tensor>& relation_edge_attr = {},
                         const std::vector<int64_t>& target_node_indices = {});

    GraphReadout forward(const nexus::graph::GraphTensorBatch& batch,
                         const std::vector<int64_t>& target_node_indices = {});

    [[nodiscard]] int64_t in_dim() const noexcept { return in_dim_; }
    [[nodiscard]] int64_t hidden_dim() const noexcept { return hidden_dim_; }
    [[nodiscard]] int64_t num_layers() const noexcept { return num_layers_; }

private:
    int64_t in_dim_;
    int64_t hidden_dim_;
    int64_t num_layers_;

    torch::nn::Linear input_proj_{nullptr};
    torch::nn::ModuleList layers_{nullptr};
    torch::nn::Linear readout_proj_{nullptr}; // [2 * hidden_dim -> hidden_dim]
};
TORCH_MODULE(RGCNEncoder);

} // namespace nexus::nn
