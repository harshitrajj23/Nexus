#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>

namespace nexus::telemetry {

struct TelemetryEvent {
    std::string event_id;
    std::string agent_id;
    int64_t timestamp{0};
    std::string action;
    std::string tool;
    std::string resource;
    std::string permission;
    std::string intent_id;
    std::string status;
    std::string label;
    bool is_synthetic{false};

    bool operator==(const TelemetryEvent& other) const {
        return event_id == other.event_id &&
               agent_id == other.agent_id &&
               timestamp == other.timestamp &&
               action == other.action &&
               tool == other.tool &&
               resource == other.resource &&
               permission == other.permission &&
               intent_id == other.intent_id &&
               status == other.status &&
               label == other.label &&
               is_synthetic == other.is_synthetic;
    }

    bool operator!=(const TelemetryEvent& other) const {
        return !(*this == other);
    }
};

void to_json(nlohmann::json& j, const TelemetryEvent& e);
void from_json(const nlohmann::json& j, TelemetryEvent& e);

std::vector<TelemetryEvent> load_jsonl(const std::filesystem::path& path);

class TelemetryCollector {
public:
    explicit TelemetryCollector(const std::filesystem::path& output_jsonl_path);
    ~TelemetryCollector();

    TelemetryCollector(const TelemetryCollector&) = delete;
    TelemetryCollector& operator=(const TelemetryCollector&) = delete;
    TelemetryCollector(TelemetryCollector&&) = delete;
    TelemetryCollector& operator=(TelemetryCollector&&) = delete;

    void push(TelemetryEvent event);
    void flush();
    void stop();

    [[nodiscard]] size_t dropped_invalid_count() const noexcept;
    [[nodiscard]] size_t get_dropped_invalid_count() const noexcept {
        return dropped_invalid_count();
    }

    static std::vector<TelemetryEvent> load_jsonl(const std::filesystem::path& path) {
        return nexus::telemetry::load_jsonl(path);
    }

private:
    void worker_loop();

    std::filesystem::path output_path_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable flush_done_cv_;
    std::thread worker_thread_;

    std::vector<TelemetryEvent> buffer_;
    bool stop_requested_{false};
    bool flush_requested_{false};
    bool is_flushing_{false};

    std::atomic<size_t> dropped_invalid_count_{0};
};

} // namespace nexus::telemetry
