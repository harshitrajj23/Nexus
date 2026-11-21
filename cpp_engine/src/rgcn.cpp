#include "rgcn.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace nexus::nn {

// ----------------------------------------------------------------------------
// RGCNLayer Implementation
// ----------------------------------------------------------------------------
RGCNLayerImpl::RGCNLayerImpl(int64_t in_features,
                             int64_t out_features,
                             int64_t num_relations,
                             double dropout,
                             bool use_basis_decomp,
                             int64_t num_bases)
    : in_features_(in_features),
      out_features_(out_features),
      num_relations_(num_relations),
      dropout_rate_(dropout),
      use_basis_decomp_(use_basis_decomp),
      num_bases_(num_bases) {
    if (in_features <= 0 || out_features <= 0 || num_relations <= 0) {
        throw std::invalid_argument("RGCNLayer dimensions and num_relations must be strictly positive");
    }

    const double gain = std::sqrt(2.0 / static_cast<double>(in_features + out_features));

    if (use_basis_decomp_) {
        if (num_bases_ <= 0 || num_bases_ > num_relations_) {
            num_bases_ = std::max<int64_t>(1, num_relations_ / 2);
        }
        basis_weights_ = register_parameter(
            "basis_weights",
            torch::randn({num_bases_, in_features_, out_features_}) * gain);
        basis_coeffs_ = register_parameter(
            "basis_coeffs",
            torch::randn({num_relations_, num_bases_}) * 0.1);
    } else {
        weight_rel_ = register_parameter(
            "weight_rel",
            torch::randn({num_relations_, in_features_, out_features_}) * gain);
    }

    weight_self_ = register_parameter(
        "weight_self",
        torch::randn({in_features_, out_features_}) * gain);

    bias_ = register_parameter("bias", torch::zeros({out_features_}));
}

torch::Tensor RGCNLayerImpl::forward(
    const torch::Tensor& node_x,
    const std::map<nexus::graph::RelationType, torch::Tensor>& relation_edge_index,
    const std::map<nexus::graph::RelationType, torch::Tensor>& /*relation_edge_attr*/) {
    const int64_t num_nodes = node_x.size(0);
    if (num_nodes == 0) {
        return torch::empty({0, out_features_}, node_x.options());
    }

    // Compute all relation weight matrices W_r
    torch::Tensor all_w;
    if (use_basis_decomp_) {
        // basis_coeffs_: [R, B], basis_weights_: [B, In, Out] -> all_w: [R, In, Out]
        all_w = torch::einsum("rb,bio->rio", {basis_coeffs_, basis_weights_});
    } else {
        all_w = weight_rel_;
    }

    // 1. Self-loop contribution: W_0 * h_i
    torch::Tensor total = torch::matmul(node_x, weight_self_); // [num_nodes, out_features]

    // 2. Accumulate relation-specific message passing: sum_r (1 / c_{i,r}) * W_r * h_j
    torch::Tensor msg_accumulator = torch::zeros({num_nodes, out_features_}, node_x.options());

    for (const auto& [rel, edge_tensor] : relation_edge_index) {
        if (edge_tensor.numel() == 0 || edge_tensor.size(0) < 2 || edge_tensor.size(1) == 0) {
            continue; // Cleanly handle empty edge tensors
        }

        const auto r_idx = static_cast<int64_t>(rel);
        if (r_idx < 0 || r_idx >= num_relations_) {
            continue; // Ignore out-of-range relation index
        }

        const torch::Tensor src = edge_tensor[0]; // Source indices j [E_r]
        const torch::Tensor dst = edge_tensor[1]; // Destination indices i [E_r]
        const int64_t num_edges = src.size(0);
        if (num_edges == 0) {
            continue;
        }

        // Get relation weight W_r: [in_features, out_features]
        torch::Tensor w_r = all_w[r_idx];

        // Gather source node features and project through W_r: [E_r, out_features]
        torch::Tensor h_src = torch::index_select(node_x, 0, src);
        torch::Tensor msg = torch::matmul(h_src, w_r);

        // Compute per-target normalization factor: c_{i,r} = max(in_degree_r(i), 1)
        torch::Tensor in_degree = torch::zeros({num_nodes}, torch::TensorOptions().dtype(torch::kFloat32).device(node_x.device()));
        torch::Tensor edge_ones = torch::ones({num_edges}, torch::TensorOptions().dtype(torch::kFloat32).device(node_x.device()));
        in_degree.index_add_(0, dst, edge_ones);

        torch::Tensor c_ir = torch::index_select(in_degree, 0, dst).clamp_min(1.0f); // [E_r]
        msg = msg / c_ir.unsqueeze(1); // Scaled message

        // Accumulate into destination nodes
        msg_accumulator.index_add_(0, dst, msg);
    }

    total = total + msg_accumulator + bias_;

    // 3. Activation & Dropout
    torch::Tensor h_next = torch::relu(total);
    if (dropout_rate_ > 0.0) {
        h_next = torch::dropout(h_next, dropout_rate_, this->is_training());
    }

    return h_next;
}

// ----------------------------------------------------------------------------
// RGCNEncoder Implementation
// ----------------------------------------------------------------------------
RGCNEncoderImpl::RGCNEncoderImpl(int64_t in_dim,
                                 int64_t hidden_dim,
                                 int64_t num_layers,
                                 double dropout,
                                 int64_t num_relations,
                                 bool use_basis_decomp,
                                 int64_t num_bases)
    : in_dim_(in_dim),
      hidden_dim_(hidden_dim),
      num_layers_(num_layers) {
    if (num_layers < 1) {
        throw std::invalid_argument("RGCNEncoder num_layers must be >= 1");
    }

    input_proj_ = register_module("input_proj", torch::nn::Linear(in_dim_, hidden_dim_));

    layers_ = register_module("layers", torch::nn::ModuleList());
    for (int64_t l = 0; l < num_layers_; ++l) {
        layers_->push_back(
            RGCNLayer(hidden_dim_, hidden_dim_, num_relations, dropout, use_basis_decomp, num_bases));
    }

    // Combines concatenated [mean_pool, max_pool] (2 * hidden_dim) into hidden_dim
    readout_proj_ = register_module("readout_proj", torch::nn::Linear(2 * hidden_dim_, hidden_dim_));
}

GraphReadout RGCNEncoderImpl::forward(
    const torch::Tensor& node_x,
    const std::map<nexus::graph::RelationType, torch::Tensor>& relation_edge_index,
    const std::map<nexus::graph::RelationType, torch::Tensor>& relation_edge_attr,
    const std::vector<int64_t>& target_node_indices) {
    const int64_t num_nodes = node_x.size(0);

    if (num_nodes == 0) {
        GraphReadout empty_readout;
        empty_readout.graph_embedding = torch::zeros({hidden_dim_}, node_x.options());
        empty_readout.node_embeddings = torch::zeros({0, hidden_dim_}, node_x.options());
        empty_readout.node_importance = torch::zeros({0}, node_x.options());
        for (const auto& [rel, _] : relation_edge_index) {
            empty_readout.edge_importance[rel] = torch::zeros({0}, node_x.options());
        }
        return empty_readout;
    }

    // 1. Initial projection to hidden dimension
    torch::Tensor h = torch::relu(input_proj_->forward(node_x));

    // 2. Multi-layer R-GCN with residual connections
    for (size_t l = 0; l < layers_->size(); ++l) {
        auto layer = layers_[l]->as<RGCNLayerImpl>();
        torch::Tensor h_layer = layer->forward(h, relation_edge_index, relation_edge_attr);
        // Residual connection (hidden_dim == hidden_dim)
        h = h + h_layer;
    }

    torch::Tensor node_embeddings = h; // [num_nodes, hidden_dim]

    // 3. Readout Pooling (Mean + Max)
    torch::Tensor pool_nodes = node_embeddings;
    if (!target_node_indices.empty()) {
        auto opts = torch::TensorOptions().dtype(torch::kInt64).device(node_x.device());
        torch::Tensor t_idx = torch::tensor(target_node_indices, opts);
        torch::Tensor valid_mask = (t_idx >= 0) & (t_idx < num_nodes);
        torch::Tensor valid_idx = t_idx.masked_select(valid_mask);
        if (valid_idx.size(0) > 0) {
            pool_nodes = torch::index_select(node_embeddings, 0, valid_idx);
        }
    }

    torch::Tensor mean_pool = pool_nodes.mean(0);                  // [hidden_dim]
    torch::Tensor max_pool = std::get<0>(pool_nodes.max(0));       // [hidden_dim]
    torch::Tensor cat_pool = torch::cat({mean_pool, max_pool}, 0); // [2 * hidden_dim]
    torch::Tensor graph_embedding = readout_proj_->forward(cat_pool); // [hidden_dim]

    // 4. Attribution Scores
    // Node importance: L2 norm of node representation
    torch::Tensor node_importance = torch::norm(node_embeddings, 2, /*dim=*/1); // [num_nodes]

    // Edge importance: Interaction magnitude between connected node representations
    std::map<nexus::graph::RelationType, torch::Tensor> edge_importance;
    for (const auto& [rel, edge_tensor] : relation_edge_index) {
        if (edge_tensor.numel() == 0 || edge_tensor.size(0) < 2 || edge_tensor.size(1) == 0) {
            edge_importance[rel] = torch::zeros({0}, node_x.options());
            continue;
        }

        torch::Tensor src = edge_tensor[0];
        torch::Tensor dst = edge_tensor[1];

        torch::Tensor h_src = torch::index_select(node_embeddings, 0, src);
        torch::Tensor h_dst = torch::index_select(node_embeddings, 0, dst);

        // Non-negative interaction norm ||h_src * h_dst||_2
        torch::Tensor score = torch::norm(h_src * h_dst, 2, /*dim=*/1);
        edge_importance[rel] = score;
    }

    GraphReadout readout;
    readout.graph_embedding = graph_embedding;
    readout.node_embeddings = node_embeddings;
    readout.edge_importance = std::move(edge_importance);
    readout.node_importance = node_importance;

    return readout;
}

GraphReadout RGCNEncoderImpl::forward(
    const nexus::graph::GraphTensorBatch& batch,
    const std::vector<int64_t>& target_node_indices) {
    return forward(batch.node_features, batch.relation_edge_index, batch.relation_edge_attr, target_node_indices);
}

} // namespace nexus::nn
