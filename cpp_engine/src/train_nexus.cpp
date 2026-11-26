#include "attack_path.hpp"
#include "graph_engine.hpp"
#include "nexus_model.hpp"
#include "rgcn.hpp"
#include "telemetry_collector.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <torch/torch.h>

namespace fs = std::filesystem;
using namespace nexus::graph;
using namespace nexus::nn;
using namespace nexus::telemetry;

// ----------------------------------------------------------------------------
// Execution Sample Representation
// ----------------------------------------------------------------------------
struct ExecutionSample {
    std::string intent_id;
    int64_t label{0}; // 0: Normal, 1: Suspicious, 2: HighRisk, 3: Critical
    std::vector<TelemetryEvent> events;
    std::vector<GraphTensorBatch> snapshots;
};

// ----------------------------------------------------------------------------
// Ablation Baseline: Non-Temporal R-GCN (Final Graph Snapshot -> MLP)
// ----------------------------------------------------------------------------
class AblationStaticRGCNImpl : public torch::nn::Module {
public:
    AblationStaticRGCNImpl(int64_t in_dim = kNodeFeatureDim,
                           int64_t hidden_dim = 32,
                           int64_t num_layers = 2,
                           double dropout = 0.1) {
        encoder_ = register_module("encoder", RGCNEncoder(in_dim, hidden_dim, num_layers, dropout));
        mlp_head_ = register_module("mlp_head", torch::nn::Sequential(
            torch::nn::Linear(hidden_dim, hidden_dim),
            torch::nn::ReLU(),
            torch::nn::Dropout(dropout),
            torch::nn::Linear(hidden_dim, 4)
        ));
    }

    torch::Tensor forward(const std::vector<GraphTensorBatch>& snapshots) {
        if (snapshots.empty()) {
            return torch::zeros({4});
        }
        // Baseline ignores temporal recurrence and reads from final graph state
        auto readout = encoder_->forward(snapshots.back());
        return mlp_head_->forward(readout.graph_embedding);
    }

private:
    RGCNEncoder encoder_{nullptr};
    torch::nn::Sequential mlp_head_{nullptr};
};
TORCH_MODULE(AblationStaticRGCN);

// ----------------------------------------------------------------------------
// Evaluation Metrics Container
// ----------------------------------------------------------------------------
struct EvaluationReport {
    double loss{0.0};
    double macro_f1{0.0};
    double false_positive_rate{0.0};
    double false_negative_rate{0.0};
    double avg_latency_ms{0.0};
    std::vector<std::vector<int>> confusion_matrix; // 4x4
    std::vector<double> precision;                   // 4 classes
    std::vector<double> recall;                      // 4 classes
    std::vector<double> f1_score;                    // 4 classes
};

// Map string labels to integer classes
static int64_t labelToClass(const std::string& label) {
    if (label == "Critical" || label == "3") return 3;
    if (label == "HighRisk" || label == "2") return 2;
    if (label == "Suspicious" || label == "1") return 1;
    return 0; // "Normal"
}

// Convert JSONL events into structured, chronological graph executions
static std::vector<ExecutionSample> prepareExecutions(const fs::path& jsonl_path) {
    std::vector<ExecutionSample> samples;
    auto events = load_jsonl(jsonl_path);
    if (events.empty()) {
        std::cerr << "[WARNING] No events loaded from: " << jsonl_path << std::endl;
        return samples;
    }

    // Group events by intent_id (execution scenario)
    std::map<std::string, std::vector<TelemetryEvent>> grouped;
    for (auto& ev : events) {
        grouped[ev.intent_id].push_back(std::move(ev));
    }

    samples.reserve(grouped.size());

    for (auto& [intent_id, ev_list] : grouped) {
        // Sort events chronologically
        std::sort(ev_list.begin(), ev_list.end(), [](const TelemetryEvent& a, const TelemetryEvent& b) {
            if (a.timestamp != b.timestamp) return a.timestamp < b.timestamp;
            return a.event_id < b.event_id;
        });

        // Determine ground truth class from highest risk in sequence
        int64_t execution_label = 0;
        for (const auto& ev : ev_list) {
            execution_label = std::max(execution_label, labelToClass(ev.label));
        }

        // Build incremental graph snapshots
        DynamicGraphEngine engine;
        std::vector<GraphTensorBatch> snapshots;
        snapshots.reserve(ev_list.size());

        for (const auto& ev : ev_list) {
            engine.ingestEvent(ev);
            auto snap = engine.getSnapshot(ev.timestamp);
            snapshots.push_back(snap.toTensorBatch());
        }

        ExecutionSample sample;
        sample.intent_id = intent_id;
        sample.label = execution_label;
        sample.events = std::move(ev_list);
        sample.snapshots = std::move(snapshots);

        samples.push_back(std::move(sample));
    }

    return samples;
}

// Compute comprehensive multiclass evaluation metrics
static EvaluationReport evaluateModel(
    TemporalRiskModel& model,
    const std::vector<ExecutionSample>& dataset,
    torch::nn::CrossEntropyLoss& criterion) {

    model->eval();
    torch::NoGradGuard no_grad;

    EvaluationReport report;
    report.confusion_matrix = std::vector<std::vector<int>>(4, std::vector<int>(4, 0));
    report.precision = std::vector<double>(4, 0.0);
    report.recall = std::vector<double>(4, 0.0);
    report.f1_score = std::vector<double>(4, 0.0);

    double total_loss = 0.0;
    int total_samples = static_cast<int>(dataset.size());
    if (total_samples == 0) return report;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (const auto& sample : dataset) {
        RiskPrediction pred = model->forward(sample.snapshots);
        torch::Tensor target = torch::tensor({sample.label}, torch::kInt64);
        torch::Tensor loss_val = criterion(pred.logits.unsqueeze(0), target);

        total_loss += loss_val.item<double>();

        int true_class = static_cast<int>(sample.label);
        int pred_class = static_cast<int>(pred.class_index);
        report.confusion_matrix[true_class][pred_class]++;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    report.avg_latency_ms = total_time_ms / total_samples;
    report.loss = total_loss / total_samples;

    // Per-class Precision, Recall, F1
    double sum_f1 = 0.0;
    for (int c = 0; c < 4; ++c) {
        int tp = report.confusion_matrix[c][c];
        int fp = 0;
        int fn = 0;
        for (int r = 0; r < 4; ++r) {
            if (r != c) fp += report.confusion_matrix[r][c];
        }
        for (int k = 0; k < 4; ++k) {
            if (k != c) fn += report.confusion_matrix[c][k];
        }

        double p = (tp + fp > 0) ? static_cast<double>(tp) / (tp + fp) : 0.0;
        double r = (tp + fn > 0) ? static_cast<double>(tp) / (tp + fn) : 0.0;
        double f1 = (p + r > 0.0) ? (2.0 * p * r) / (p + r) : 0.0;

        report.precision[c] = p;
        report.recall[c] = r;
        report.f1_score[c] = f1;
        sum_f1 += f1;
    }
    report.macro_f1 = sum_f1 / 4.0;

    // False Positive Rate (Normal classified as attack: Suspicious/HighRisk/Critical)
    int normal_total = 0;
    int normal_fp = 0;
    for (int k = 0; k < 4; ++k) normal_total += report.confusion_matrix[0][k];
    for (int k = 1; k < 4; ++k) normal_fp += report.confusion_matrix[0][k];
    report.false_positive_rate = (normal_total > 0) ? static_cast<double>(normal_fp) / normal_total : 0.0;

    // False Negative Rate (Attacks classified as Normal)
    int attack_total = 0;
    int attack_fn = 0;
    for (int r = 1; r < 4; ++r) {
        for (int k = 0; k < 4; ++k) attack_total += report.confusion_matrix[r][k];
        attack_fn += report.confusion_matrix[r][0];
    }
    report.false_negative_rate = (attack_total > 0) ? static_cast<double>(attack_fn) / attack_total : 0.0;

    return report;
}

// Evaluate Ablation Baseline
static EvaluationReport evaluateAblation(
    AblationStaticRGCN& model,
    const std::vector<ExecutionSample>& dataset,
    torch::nn::CrossEntropyLoss& criterion) {

    model->eval();
    torch::NoGradGuard no_grad;

    EvaluationReport report;
    report.confusion_matrix = std::vector<std::vector<int>>(4, std::vector<int>(4, 0));
    report.precision = std::vector<double>(4, 0.0);
    report.recall = std::vector<double>(4, 0.0);
    report.f1_score = std::vector<double>(4, 0.0);

    double total_loss = 0.0;
    int total_samples = static_cast<int>(dataset.size());
    if (total_samples == 0) return report;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (const auto& sample : dataset) {
        torch::Tensor logits = model->forward(sample.snapshots);
        torch::Tensor target = torch::tensor({sample.label}, torch::kInt64);
        torch::Tensor loss_val = criterion(logits.unsqueeze(0), target);

        total_loss += loss_val.item<double>();

        int true_class = static_cast<int>(sample.label);
        int pred_class = logits.argmax(-1).item<int>();
        report.confusion_matrix[true_class][pred_class]++;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    report.avg_latency_ms = total_time_ms / total_samples;
    report.loss = total_loss / total_samples;

    double sum_f1 = 0.0;
    for (int c = 0; c < 4; ++c) {
        int tp = report.confusion_matrix[c][c];
        int fp = 0, fn = 0;
        for (int r = 0; r < 4; ++r) if (r != c) fp += report.confusion_matrix[r][c];
        for (int k = 0; k < 4; ++k) if (k != c) fn += report.confusion_matrix[c][k];

        double p = (tp + fp > 0) ? static_cast<double>(tp) / (tp + fp) : 0.0;
        double r = (tp + fn > 0) ? static_cast<double>(tp) / (tp + fn) : 0.0;
        double f1 = (p + r > 0.0) ? (2.0 * p * r) / (p + r) : 0.0;

        report.precision[c] = p;
        report.recall[c] = r;
        report.f1_score[c] = f1;
        sum_f1 += f1;
    }
    report.macro_f1 = sum_f1 / 4.0;

    int normal_total = 0, normal_fp = 0;
    for (int k = 0; k < 4; ++k) normal_total += report.confusion_matrix[0][k];
    for (int k = 1; k < 4; ++k) normal_fp += report.confusion_matrix[0][k];
    report.false_positive_rate = (normal_total > 0) ? static_cast<double>(normal_fp) / normal_total : 0.0;

    int attack_total = 0, attack_fn = 0;
    for (int r = 1; r < 4; ++r) {
        for (int k = 0; k < 4; ++k) attack_total += report.confusion_matrix[r][k];
        attack_fn += report.confusion_matrix[r][0];
    }
    report.false_negative_rate = (attack_total > 0) ? static_cast<double>(attack_fn) / attack_total : 0.0;

    return report;
}

int main(int argc, char* argv[]) {
    std::cout << "==============================================================================" << std::endl;
    std::cout << "NEXUS C++17 LIBTORCH TRAINING, ABLATION & EVALUATION PIPELINE" << std::endl;
    std::cout << "==============================================================================" << std::endl;

    fs::path base_data_dir = "data";
    if (!fs::exists(base_data_dir / "train.jsonl")) {
        base_data_dir = "../data";
    }

    std::cout << "[1/5] Loading and converting JSONL splits into dynamic graph snapshot sequences..." << std::endl;
    auto train_data = prepareExecutions(base_data_dir / "train.jsonl");
    auto val_data = prepareExecutions(base_data_dir / "val.jsonl");
    auto test_data = prepareExecutions(base_data_dir / "test.jsonl");
    auto unseen_data = prepareExecutions(base_data_dir / "test_unseen.jsonl");

    std::cout << "      Train executions:       " << train_data.size() << std::endl;
    std::cout << "      Validation executions:  " << val_data.size() << std::endl;
    std::cout << "      Test executions:        " << test_data.size() << std::endl;
    std::cout << "      Held-out Unseen:        " << unseen_data.size() << std::endl;

    // -------------------------------------------------------------------------
    // Compute Class Weights to counter class imbalance
    // -------------------------------------------------------------------------
    std::vector<int64_t> class_counts(4, 0);
    for (const auto& sample : train_data) {
        class_counts[sample.label]++;
    }

    const float total_train = static_cast<float>(train_data.size());
    std::vector<float> weights_vec(4, 1.0f);
    for (int c = 0; c < 4; ++c) {
        weights_vec[c] = total_train / (4.0f * std::max<int64_t>(1, class_counts[c]));
    }
    torch::Tensor class_weights = torch::tensor(weights_vec, torch::kFloat32);
    auto criterion = torch::nn::CrossEntropyLoss(torch::nn::CrossEntropyLossOptions().weight(class_weights));

    std::cout << "\n[2/5] Class distribution in training set:" << std::endl;
    std::cout << "      Normal (0): " << class_counts[0] << " | weight: " << weights_vec[0] << std::endl;
    std::cout << "      Suspicious (1): " << class_counts[1] << " | weight: " << weights_vec[1] << std::endl;
    std::cout << "      HighRisk (2): " << class_counts[2] << " | weight: " << weights_vec[2] << std::endl;
    std::cout << "      Critical (3): " << class_counts[3] << " | weight: " << weights_vec[3] << std::endl;

    // -------------------------------------------------------------------------
    // Training TemporalRiskModel (R-GCN + GRU)
    // -------------------------------------------------------------------------
    constexpr int64_t kHiddenDim = 32;
    constexpr int kEpochs = 10;

    std::cout << "\n[3/5] Training Full TemporalRiskModel (R-GCN + GRU) for " << kEpochs << " epochs..." << std::endl;
    TemporalRiskModel full_model(kNodeFeatureDim, kHiddenDim, kHiddenDim, /*num_rgcn_layers=*/2, /*num_gru_layers=*/1, /*dropout=*/0.1);
    torch::optim::AdamW optimizer(full_model->parameters(), torch::optim::AdamWOptions(1e-3).weight_decay(1e-4));

    std::vector<double> train_losses;
    std::vector<double> val_losses;
    double best_val_f1 = -1.0;
    fs::path best_model_path = base_data_dir / "nexus_best.pt";

    for (int epoch = 1; epoch <= kEpochs; ++epoch) {
        full_model->train();
        double epoch_train_loss = 0.0;

        for (const auto& sample : train_data) {
            optimizer.zero_grad();
            RiskPrediction pred = full_model->forward(sample.snapshots);
            torch::Tensor target = torch::tensor({sample.label}, torch::kInt64);
            torch::Tensor loss = criterion(pred.logits.unsqueeze(0), target);

            loss.backward();
            optimizer.step();

            epoch_train_loss += loss.item<double>();
        }

        epoch_train_loss /= train_data.size();
        train_losses.push_back(epoch_train_loss);

        auto val_report = evaluateModel(full_model, val_data, criterion);
        val_losses.push_back(val_report.loss);

        std::cout << "      Epoch [" << std::setw(2) << epoch << "/" << kEpochs << "] "
                  << "Train Loss: " << std::fixed << std::setprecision(4) << epoch_train_loss << " | "
                  << "Val Loss: " << val_report.loss << " | "
                  << "Val Macro F1: " << std::setprecision(3) << (val_report.macro_f1 * 100.0) << "%"
                  << std::endl;

        if (val_report.macro_f1 > best_val_f1) {
            best_val_f1 = val_report.macro_f1;
            full_model->saveCheckpoint(best_model_path);
        }
    }

    std::cout << "      Saved best checkpoint to: " << best_model_path << std::endl;

    // -------------------------------------------------------------------------
    // Training Ablation Baseline (Non-Temporal Static R-GCN)
    // -------------------------------------------------------------------------
    std::cout << "\n[4/5] Training Ablation Baseline (Static R-GCN without GRU) on exact same splits..." << std::endl;
    AblationStaticRGCN ablation_model(kNodeFeatureDim, kHiddenDim, /*num_layers=*/2, /*dropout=*/0.1);
    torch::optim::AdamW ablation_optimizer(ablation_model->parameters(), torch::optim::AdamWOptions(1e-3).weight_decay(1e-4));

    for (int epoch = 1; epoch <= kEpochs; ++epoch) {
        ablation_model->train();
        for (const auto& sample : train_data) {
            ablation_optimizer.zero_grad();
            torch::Tensor logits = ablation_model->forward(sample.snapshots);
            torch::Tensor target = torch::tensor({sample.label}, torch::kInt64);
            torch::Tensor loss = criterion(logits.unsqueeze(0), target);
            loss.backward();
            ablation_optimizer.step();
        }
    }

    // -------------------------------------------------------------------------
    // Final Evaluation & Comparison
    // -------------------------------------------------------------------------
    std::cout << "\n[5/5] Evaluating Best Model and Ablation Baseline on Test and Withheld Unseen Splits..." << std::endl;
    TemporalRiskModel best_model(kNodeFeatureDim, kHiddenDim, kHiddenDim, 2, 1, 0.0);
    best_model->loadCheckpoint(best_model_path);

    EvaluationReport test_eval = evaluateModel(best_model, test_data, criterion);
    EvaluationReport unseen_eval = evaluateModel(best_model, unseen_data, criterion);
    EvaluationReport ablation_eval = evaluateAblation(ablation_model, test_data, criterion);

    // Save metrics to data/metrics.json
    nlohmann::json metrics_json;
    metrics_json["train_loss_history"] = train_losses;
    metrics_json["val_loss_history"] = val_losses;
    metrics_json["temporal_model_test"] = {
        {"macro_f1", test_eval.macro_f1},
        {"precision_per_class", test_eval.precision},
        {"recall_per_class", test_eval.recall},
        {"f1_per_class", test_eval.f1_score},
        {"false_positive_rate", test_eval.false_positive_rate},
        {"false_negative_rate", test_eval.false_negative_rate},
        {"avg_latency_ms", test_eval.avg_latency_ms},
        {"confusion_matrix", test_eval.confusion_matrix}
    };
    metrics_json["withheld_unseen_attack_eval"] = {
        {"macro_f1", unseen_eval.macro_f1},
        {"precision_per_class", unseen_eval.precision},
        {"recall_per_class", unseen_eval.recall},
        {"f1_per_class", unseen_eval.f1_score},
        {"false_positive_rate", unseen_eval.false_positive_rate},
        {"false_negative_rate", unseen_eval.false_negative_rate},
        {"avg_latency_ms", unseen_eval.avg_latency_ms},
        {"confusion_matrix", unseen_eval.confusion_matrix}
    };
    metrics_json["ablation_comparison"] = {
        {"temporal_rgcn_gru_macro_f1", test_eval.macro_f1},
        {"static_rgcn_baseline_macro_f1", ablation_eval.macro_f1},
        {"temporal_delta_f1", test_eval.macro_f1 - ablation_eval.macro_f1},
        {"temporal_latency_ms", test_eval.avg_latency_ms},
        {"static_latency_ms", ablation_eval.avg_latency_ms}
    };

    fs::path metrics_path = base_data_dir / "metrics.json";
    std::ofstream metrics_out(metrics_path);
    metrics_out << metrics_json.dump(2) << std::endl;

    // -------------------------------------------------------------------------
    // Print Summary Performance Table
    // -------------------------------------------------------------------------
    std::cout << "\n" << std::string(82, '=') << std::endl;
    std::cout << "NEXUS MODEL EVALUATION RESULTS" << std::endl;
    std::cout << std::string(82, '=') << std::endl;
    std::cout << std::left << std::setw(28) << "Architecture / Split"
              << " | " << std::setw(10) << "Macro F1"
              << " | " << std::setw(10) << "FPR"
              << " | " << std::setw(10) << "FNR"
              << " | " << std::setw(12) << "Latency (ms)"
              << std::endl;
    std::cout << std::string(82, '-') << std::endl;
    std::cout << std::left << std::setw(28) << "R-GCN + GRU (Test Seen)"
              << " | " << std::fixed << std::setprecision(1) << std::setw(9) << (test_eval.macro_f1 * 100.0) << "%"
              << " | " << std::setprecision(1) << std::setw(9) << (test_eval.false_positive_rate * 100.0) << "%"
              << " | " << std::setprecision(1) << std::setw(9) << (test_eval.false_negative_rate * 100.0) << "%"
              << " | " << std::setprecision(2) << std::setw(12) << test_eval.avg_latency_ms
              << std::endl;
    std::cout << std::left << std::setw(28) << "Static R-GCN (Ablation)"
              << " | " << std::fixed << std::setprecision(1) << std::setw(9) << (ablation_eval.macro_f1 * 100.0) << "%"
              << " | " << std::setprecision(1) << std::setw(9) << (ablation_eval.false_positive_rate * 100.0) << "%"
              << " | " << std::setprecision(1) << std::setw(9) << (ablation_eval.false_negative_rate * 100.0) << "%"
              << " | " << std::setprecision(2) << std::setw(12) << ablation_eval.avg_latency_ms
              << std::endl;
    std::cout << std::left << std::setw(28) << "R-GCN + GRU (Unseen Attacks)"
              << " | " << std::fixed << std::setprecision(1) << std::setw(9) << (unseen_eval.macro_f1 * 100.0) << "%"
              << " | " << std::setprecision(1) << std::setw(9) << (unseen_eval.false_positive_rate * 100.0) << "%"
              << " | " << std::setprecision(1) << std::setw(9) << (unseen_eval.false_negative_rate * 100.0) << "%"
              << " | " << std::setprecision(2) << std::setw(12) << unseen_eval.avg_latency_ms
              << std::endl;
    std::cout << std::string(82, '=') << std::endl;

    std::cout << "\nPer-Class Breakdown on Test Set (R-GCN + GRU):" << std::endl;
    const std::vector<std::string> class_names = {"Normal", "Suspicious", "HighRisk", "Critical"};
    for (int c = 0; c < 4; ++c) {
        std::cout << "  - " << std::left << std::setw(12) << class_names[c]
                  << " Precision: " << std::fixed << std::setprecision(1) << std::setw(5) << (test_eval.precision[c] * 100.0) << "% | "
                  << "Recall: " << std::setw(5) << (test_eval.recall[c] * 100.0) << "% | "
                  << "F1: " << std::setw(5) << (test_eval.f1_score[c] * 100.0) << "%"
                  << std::endl;
    }

    std::cout << "\nConfusion Matrix (Row=Actual, Col=Predicted):" << std::endl;
    std::cout << "         Normal  Susp  High  Crit" << std::endl;
    for (int r = 0; r < 4; ++r) {
        std::cout << std::left << std::setw(8) << class_names[r];
        for (int c = 0; c < 4; ++c) {
            std::cout << std::right << std::setw(6) << test_eval.confusion_matrix[r][c];
        }
        std::cout << std::endl;
    }

    std::cout << "\nMetrics file generated: " << metrics_path << std::endl;
    std::cout << "Best model checkpoint:  " << best_model_path << std::endl;
    std::cout << "==============================================================================" << std::endl;

    return 0;
}
