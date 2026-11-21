#pragma once

#include "graph_engine.hpp"
#include "rgcn.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <torch/torch.h>

namespace nexus::nn {

// Four standard behavioral risk classes defined in PRD Section 5.4
enum class RiskLevel : int64_t {
    Normal = 0,
    Suspicious = 1,
    HighRisk = 2,
    Critical = 3
};

std::string riskLevelToString(RiskLevel level);

// Structured risk assessment output
struct RiskPrediction {
    RiskLevel predicted_class{RiskLevel::Normal};
    int64_t class_index{0};
    torch::Tensor logits;           // [4] or [batch, 4]
    torch::Tensor probabilities;    // [4] or [batch, 4] (softmax)
    double confidence{0.0};         // Maximum class probability in [0.0, 1.0]
    torch::Tensor temporal_state;   // Accumulated GRU hidden state z_t
    nexus::nn::GraphReadout latest_readout;
};

// ----------------------------------------------------------------------------
// TemporalRiskModel: Combines RGCNEncoder, Temporal GRU, and MLP Risk Head
// Computes:
//   g_t = RGCNEncoder(G_t)
//   z_t = GRU(g_t, z_{t-1})
//   logits = MLP(z_t)
// ----------------------------------------------------------------------------
class TemporalRiskModelImpl : public torch::nn::Module {
public:
    TemporalRiskModelImpl(int64_t in_dim = nexus::graph::kNodeFeatureDim,
                          int64_t graph_hidden_dim = 64,
                          int64_t gru_hidden_dim = 64,
                          int64_t num_rgcn_layers = 2,
                          int64_t num_gru_layers = 1,
                          double dropout = 0.1,
                          int64_t num_relations = 7,
                          bool use_basis_decomp = false,
                          int64_t num_bases = 0);

    // Forward over a temporal sequence of dynamic graph snapshots
    RiskPrediction forward(const std::vector<nexus::graph::GraphTensorBatch>& snapshot_sequence,
                           std::optional<torch::Tensor> h_0 = std::nullopt);

    // Forward over a sequence of precomputed graph embedding tensors: [seq_len, graph_dim] or [batch, seq_len, graph_dim]
    RiskPrediction forward(const torch::Tensor& graph_embedding_seq,
                           std::optional<torch::Tensor> h_0 = std::nullopt,
                           std::optional<nexus::nn::GraphReadout> latest_readout = std::nullopt);

    // Checkpointing
    void saveCheckpoint(const std::filesystem::path& path) const;
    void loadCheckpoint(const std::filesystem::path& path);

    [[nodiscard]] int64_t in_dim() const noexcept { return in_dim_; }
    [[nodiscard]] int64_t graph_hidden_dim() const noexcept { return graph_hidden_dim_; }
    [[nodiscard]] int64_t gru_hidden_dim() const noexcept { return gru_hidden_dim_; }
    [[nodiscard]] int64_t num_gru_layers() const noexcept { return num_gru_layers_; }

    [[nodiscard]] RGCNEncoder& encoder() noexcept { return encoder_; }
    [[nodiscard]] const RGCNEncoder& encoder() const noexcept { return encoder_; }

private:
    int64_t in_dim_;
    int64_t graph_hidden_dim_;
    int64_t gru_hidden_dim_;
    int64_t num_gru_layers_;
    double dropout_rate_;

    RGCNEncoder encoder_{nullptr};
    torch::nn::GRU gru_{nullptr};
    torch::nn::Sequential mlp_head_{nullptr};
};
TORCH_MODULE(TemporalRiskModel);

} // namespace nexus::nn
