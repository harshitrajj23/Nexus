#include "nexus_model.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace nexus::nn {

std::string riskLevelToString(RiskLevel level) {
    switch (level) {
        case RiskLevel::Normal: return "Normal";
        case RiskLevel::Suspicious: return "Suspicious";
        case RiskLevel::HighRisk: return "HighRisk";
        case RiskLevel::Critical: return "Critical";
    }
    return "Unknown";
}

TemporalRiskModelImpl::TemporalRiskModelImpl(int64_t in_dim,
                                             int64_t graph_hidden_dim,
                                             int64_t gru_hidden_dim,
                                             int64_t num_rgcn_layers,
                                             int64_t num_gru_layers,
                                             double dropout,
                                             int64_t num_relations,
                                             bool use_basis_decomp,
                                             int64_t num_bases)
    : in_dim_(in_dim),
      graph_hidden_dim_(graph_hidden_dim),
      gru_hidden_dim_(gru_hidden_dim),
      num_gru_layers_(num_gru_layers),
      dropout_rate_(dropout) {

    // 1. Relational Graph Convolutional Network Encoder
    encoder_ = register_module(
        "encoder",
        RGCNEncoder(in_dim_,
                    graph_hidden_dim_,
                    num_rgcn_layers,
                    dropout_rate_,
                    num_relations,
                    use_basis_decomp,
                    num_bases));

    // 2. Temporal GRU Layer
    torch::nn::GRUOptions gru_options(graph_hidden_dim_, gru_hidden_dim_);
    gru_options.num_layers(num_gru_layers_)
        .batch_first(true)
        .dropout(num_gru_layers_ > 1 ? dropout_rate_ : 0.0);
    gru_ = register_module("gru", torch::nn::GRU(gru_options));

    // 3. MLP Risk Prediction Head
    mlp_head_ = register_module(
        "mlp_head",
        torch::nn::Sequential(
            torch::nn::Linear(gru_hidden_dim_, gru_hidden_dim_),
            torch::nn::ReLU(),
            torch::nn::Dropout(dropout_rate_),
            torch::nn::Linear(gru_hidden_dim_, 4) // 4 classes: Normal, Suspicious, HighRisk, Critical
        ));
}

RiskPrediction TemporalRiskModelImpl::forward(
    const std::vector<nexus::graph::GraphTensorBatch>& snapshot_sequence,
    std::optional<torch::Tensor> h_0) {
    if (snapshot_sequence.empty()) {
        RiskPrediction empty_pred;
        empty_pred.logits = torch::zeros({4});
        empty_pred.probabilities = torch::full({4}, 0.25f);
        empty_pred.confidence = 0.25;
        empty_pred.temporal_state = torch::zeros({gru_hidden_dim_});
        return empty_pred;
    }

    // Pass each snapshot through RGCNEncoder to extract sequence of graph embeddings g_t
    std::vector<torch::Tensor> graph_embeddings;
    graph_embeddings.reserve(snapshot_sequence.size());
    std::optional<GraphReadout> latest_readout;

    for (const auto& snap : snapshot_sequence) {
        GraphReadout readout = encoder_->forward(snap);
        graph_embeddings.push_back(readout.graph_embedding);
        latest_readout = readout;
    }

    // Stack embeddings into 2D [seq_len, graph_hidden_dim] for single-sequence evaluation
    torch::Tensor seq_tensor = torch::stack(graph_embeddings, /*dim=*/0);

    return forward(seq_tensor, h_0, latest_readout);
}

RiskPrediction TemporalRiskModelImpl::forward(
    const torch::Tensor& graph_embedding_seq,
    std::optional<torch::Tensor> h_0,
    std::optional<GraphReadout> latest_readout) {
    torch::Tensor seq_input = graph_embedding_seq;
    bool is_batched = true;

    if (seq_input.dim() == 2) {
        // [seq_len, graph_dim] -> [1, seq_len, graph_dim]
        seq_input = seq_input.unsqueeze(0);
        is_batched = false;
    } else if (seq_input.dim() != 3) {
        throw std::invalid_argument("graph_embedding_seq must be 2D [seq_len, dim] or 3D [batch, seq_len, dim]");
    }

    const int64_t batch_size = seq_input.size(0);

    // Automatic zero initialization for initial hidden state if nullopt
    torch::Tensor init_h;
    if (h_0.has_value() && h_0->defined()) {
        init_h = h_0.value();
        if (init_h.dim() == 1) {
            init_h = init_h.view({num_gru_layers_, batch_size, gru_hidden_dim_});
        } else if (init_h.dim() == 2) {
            init_h = init_h.unsqueeze(0);
        }
    } else {
        init_h = torch::zeros({num_gru_layers_, batch_size, gru_hidden_dim_}, seq_input.options());
    }

    // Compute temporal evolution z_t = GRU(g_t, z_{t-1})
    auto [gru_out, h_n] = gru_->forward(seq_input, init_h);
    // gru_out: [batch, seq_len, gru_hidden_dim]

    // Final accumulated temporal state z_t
    torch::Tensor z_t = gru_out.select(/*dim=*/1, /*index=*/-1); // [batch, gru_hidden_dim]

    // Predict risk logits: [batch, 4]
    torch::Tensor logits = mlp_head_->forward(z_t);
    torch::Tensor probabilities = torch::softmax(logits, /*dim=*/-1);

    RiskPrediction pred;
    if (!is_batched) {
        logits = logits.squeeze(0);
        probabilities = probabilities.squeeze(0);
        z_t = z_t.squeeze(0);

        const int64_t pred_idx = probabilities.argmax(-1).item<int64_t>();
        pred.class_index = pred_idx;
        pred.predicted_class = static_cast<RiskLevel>(pred_idx);
        pred.confidence = probabilities[pred_idx].item<double>();
    } else {
        const int64_t first_pred_idx = probabilities[0].argmax(-1).item<int64_t>();
        pred.class_index = first_pred_idx;
        pred.predicted_class = static_cast<RiskLevel>(first_pred_idx);
        pred.confidence = probabilities[0][first_pred_idx].item<double>();
    }

    pred.logits = logits;
    pred.probabilities = probabilities;
    pred.temporal_state = z_t;
    if (latest_readout.has_value()) {
        pred.latest_readout = latest_readout.value();
    }

    return pred;
}

void TemporalRiskModelImpl::saveCheckpoint(const std::filesystem::path& path) const {
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    torch::serialize::OutputArchive archive;
    const_cast<TemporalRiskModelImpl*>(this)->save(archive);
    archive.save_to(path.string());
}

void TemporalRiskModelImpl::loadCheckpoint(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Checkpoint file does not exist: " + path.string());
    }
    torch::serialize::InputArchive archive;
    archive.load_from(path.string());
    load(archive);
}

} // namespace nexus::nn
