#include "telemetry_collector.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>

namespace nexus::telemetry {

namespace {

bool is_valid_event(const TelemetryEvent& event, std::string& error_msg) {
    if (event.event_id.empty()) {
        error_msg = "event_id is empty";
        return false;
    }
    if (event.agent_id.empty()) {
        error_msg = "agent_id is empty";
        return false;
    }
    if (event.action.empty()) {
        error_msg = "action is empty";
        return false;
    }
    if (event.tool.empty()) {
        error_msg = "tool is empty";
        return false;
    }
    if (event.timestamp < 0) {
        error_msg = "timestamp is negative (" + std::to_string(event.timestamp) + ")";
        return false;
    }
    return true;
}

} // namespace

void to_json(nlohmann::json& j, const TelemetryEvent& e) {
    j = nlohmann::json{
        {"event_id", e.event_id},
        {"agent_id", e.agent_id},
        {"timestamp", e.timestamp},
        {"action", e.action},
        {"tool", e.tool},
        {"resource", e.resource},
        {"permission", e.permission},
        {"intent_id", e.intent_id},
        {"status", e.status},
        {"label", e.label},
        {"is_synthetic", e.is_synthetic}
    };
}

void from_json(const nlohmann::json& j, TelemetryEvent& e) {
    j.at("event_id").get_to(e.event_id);
    j.at("agent_id").get_to(e.agent_id);
    j.at("timestamp").get_to(e.timestamp);
    j.at("action").get_to(e.action);
    j.at("tool").get_to(e.tool);

    if (j.contains("resource") && !j.at("resource").is_null()) {
        j.at("resource").get_to(e.resource);
    } else {
        e.resource.clear();
    }

    if (j.contains("permission") && !j.at("permission").is_null()) {
        j.at("permission").get_to(e.permission);
    } else {
        e.permission.clear();
    }

    if (j.contains("intent_id") && !j.at("intent_id").is_null()) {
        j.at("intent_id").get_to(e.intent_id);
    } else {
        e.intent_id.clear();
    }

    if (j.contains("status") && !j.at("status").is_null()) {
        j.at("status").get_to(e.status);
    } else {
        e.status.clear();
    }

    if (j.contains("label") && !j.at("label").is_null()) {
        j.at("label").get_to(e.label);
    } else {
        e.label.clear();
    }

    if (j.contains("is_synthetic") && !j.at("is_synthetic").is_null()) {
        j.at("is_synthetic").get_to(e.is_synthetic);
    } else {
        e.is_synthetic = false;
    }
}

std::vector<TelemetryEvent> load_jsonl(const std::filesystem::path& path) {
    std::vector<TelemetryEvent> events;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[load_jsonl: ERROR] Failed to open file: " << path << std::endl;
        return events;
    }

    std::string line;
    size_t line_number = 0;
    while (std::getline(file, line)) {
        ++line_number;
        const auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            continue; // Ignore blank lines
        }
        const auto end = line.find_last_not_of(" \t\r\n");
        const std::string trimmed = line.substr(start, end - start + 1);

        try {
            const nlohmann::json j = nlohmann::json::parse(trimmed);
            TelemetryEvent event;
            from_json(j, event);

            std::string validation_error;
            if (!is_valid_event(event, validation_error)) {
                std::cerr << "[load_jsonl: REJECTED] Line " << line_number
                          << ": Invalid event (" << validation_error << ")" << std::endl;
                continue;
            }

            events.push_back(std::move(event));
        } catch (const nlohmann::json::exception& ex) {
            std::cerr << "[load_jsonl: REJECTED] Line " << line_number
                      << ": Malformed JSON (" << ex.what() << ")" << std::endl;
        } catch (const std::exception& ex) {
            std::cerr << "[load_jsonl: REJECTED] Line " << line_number
                      << ": Failed to parse (" << ex.what() << ")" << std::endl;
        }
    }

    return events;
}

TelemetryCollector::TelemetryCollector(const std::filesystem::path& output_jsonl_path)
    : output_path_(output_jsonl_path),
      stop_requested_(false),
      flush_requested_(false),
      is_flushing_(false),
      dropped_invalid_count_(0) {
    if (output_path_.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(output_path_.parent_path(), ec);
    }
    worker_thread_ = std::thread(&TelemetryCollector::worker_loop, this);
}

TelemetryCollector::~TelemetryCollector() {
    stop();
}

void TelemetryCollector::push(TelemetryEvent event) {
    std::string validation_error;
    if (!is_valid_event(event, validation_error)) {
        std::cerr << "[TelemetryCollector: REJECTED] Dropping invalid event: "
                  << validation_error << std::endl;
        dropped_invalid_count_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.push_back(std::move(event));
    }
    cv_.notify_one();
}

void TelemetryCollector::flush() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (buffer_.empty() && !is_flushing_) {
        return;
    }
    flush_requested_ = true;
    cv_.notify_all();
    flush_done_cv_.wait(lock, [this] {
        return buffer_.empty() && !flush_requested_ && !is_flushing_;
    });
}

void TelemetryCollector::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_requested_) {
            return;
        }
        stop_requested_ = true;
        flush_requested_ = true;
    }
    cv_.notify_all();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

size_t TelemetryCollector::dropped_invalid_count() const noexcept {
    return dropped_invalid_count_.load(std::memory_order_relaxed);
}

void TelemetryCollector::worker_loop() {
    while (true) {
        std::vector<TelemetryEvent> batch;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return stop_requested_ || flush_requested_ || buffer_.size() >= 1000;
            });

            if (stop_requested_ && buffer_.empty()) {
                break;
            }

            if (!buffer_.empty()) {
                batch.swap(buffer_);
                is_flushing_ = true;
            }
        }

        if (!batch.empty()) {
            // Sort deterministically: timestamp ascending, ties broken by event_id ascending
            std::sort(batch.begin(), batch.end(), [](const TelemetryEvent& a, const TelemetryEvent& b) {
                if (a.timestamp != b.timestamp) {
                    return a.timestamp < b.timestamp;
                }
                return a.event_id < b.event_id;
            });

            if (output_path_.has_parent_path()) {
                std::error_code ec;
                std::filesystem::create_directories(output_path_.parent_path(), ec);
            }

            std::ofstream out(output_path_, std::ios::out | std::ios::app);
            if (out.is_open()) {
                for (const auto& ev : batch) {
                    nlohmann::json j;
                    to_json(j, ev);
                    out << j.dump() << "\n";
                }
                out.flush();
            } else {
                std::cerr << "[TelemetryCollector: ERROR] Failed to open output file: "
                          << output_path_ << std::endl;
            }
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            is_flushing_ = false;
            flush_requested_ = false;
        }
        flush_done_cv_.notify_all();

        if (stop_requested_) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (buffer_.empty()) {
                break;
            }
        }
    }
}

} // namespace nexus::telemetry
