#include "telemetry_collector.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace nexus::telemetry;

static int g_failures = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  Assertion failed: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            test_passed = false; \
        } \
    } while (0)

static void test_missing_required_fields() {
    const std::string test_name = "Missing Required Fields & Negative Timestamps";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    const fs::path test_file = "test_output/missing_fields.jsonl";
    if (fs::exists(test_file)) fs::remove(test_file);

    {
        TelemetryCollector collector(test_file);

        // 1. Missing event_id
        collector.push({"", "AGENT_01", 100, "READ", "TOOL_FS"});
        // 2. Missing agent_id
        collector.push({"EVT_01", "", 100, "READ", "TOOL_FS"});
        // 3. Missing action
        collector.push({"EVT_02", "AGENT_01", 100, "", "TOOL_FS"});
        // 4. Missing tool
        collector.push({"EVT_03", "AGENT_01", 100, "READ", ""});
        // 5. Negative timestamp
        collector.push({"EVT_04", "AGENT_01", -1, "READ", "TOOL_FS"});
        collector.push({"EVT_05", "AGENT_01", -9999, "READ", "TOOL_FS"});

        // 6. Push one valid event
        collector.push({"EVT_VALID", "AGENT_01", 100, "READ", "TOOL_FS"});

        collector.flush();
        collector.stop();

        TEST_ASSERT(collector.dropped_invalid_count() == 6, "Expected exactly 6 dropped invalid events");
    }

    auto loaded = load_jsonl(test_file);
    TEST_ASSERT(loaded.size() == 1, "Only the single valid event should be written to file");
    if (!loaded.empty()) {
        TEST_ASSERT(loaded[0].event_id == "EVT_VALID", "Persisted event must be EVT_VALID");
    }

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_out_of_order_timestamps() {
    const std::string test_name = "Out-of-Order Timestamps Sorting";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    const fs::path test_file = "test_output/out_of_order.jsonl";
    if (fs::exists(test_file)) fs::remove(test_file);

    {
        TelemetryCollector collector(test_file);

        // Push events in shuffled order: 500, 100, 400, 200, 300
        collector.push({"EVT_500", "AGENT_1", 500, "QUERY", "DB"});
        collector.push({"EVT_100", "AGENT_1", 100, "LOGIN", "AUTH"});
        collector.push({"EVT_400", "AGENT_1", 400, "UPDATE", "DB"});
        collector.push({"EVT_200", "AGENT_1", 200, "READ", "FS"});
        collector.push({"EVT_300", "AGENT_1", 300, "WRITE", "FS"});

        collector.flush();
        collector.stop();

        TEST_ASSERT(collector.dropped_invalid_count() == 0, "No valid events should be dropped");
    }

    auto events = load_jsonl(test_file);
    TEST_ASSERT(events.size() == 5, "Expected 5 loaded events");

    if (events.size() == 5) {
        TEST_ASSERT(events[0].timestamp == 100 && events[0].event_id == "EVT_100", "Index 0 should be timestamp 100");
        TEST_ASSERT(events[1].timestamp == 200 && events[1].event_id == "EVT_200", "Index 1 should be timestamp 200");
        TEST_ASSERT(events[2].timestamp == 300 && events[2].event_id == "EVT_300", "Index 2 should be timestamp 300");
        TEST_ASSERT(events[3].timestamp == 400 && events[3].event_id == "EVT_400", "Index 3 should be timestamp 400");
        TEST_ASSERT(events[4].timestamp == 500 && events[4].event_id == "EVT_500", "Index 4 should be timestamp 500");
    }

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_duplicate_and_tied_timestamps() {
    const std::string test_name = "Duplicate and Tied Timestamps Tie-Breaking";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    const fs::path test_file = "test_output/tied_timestamps.jsonl";
    if (fs::exists(test_file)) fs::remove(test_file);

    {
        TelemetryCollector collector(test_file);

        // Tied timestamp 200 with out-of-order event IDs: D, A, C, B
        collector.push({"EVT_D", "AGENT_1", 200, "READ", "API"});
        collector.push({"EVT_A", "AGENT_1", 200, "READ", "API"});
        collector.push({"EVT_C", "AGENT_1", 200, "READ", "API"});
        collector.push({"EVT_B", "AGENT_1", 200, "READ", "API"});

        // Earlier tied timestamp 100: Z, M
        collector.push({"EVT_Z", "AGENT_1", 100, "SCAN", "NETWORK"});
        collector.push({"EVT_M", "AGENT_1", 100, "SCAN", "NETWORK"});

        collector.flush();
        collector.stop();
    }

    auto events = load_jsonl(test_file);
    TEST_ASSERT(events.size() == 6, "Expected 6 loaded events");

    if (events.size() == 6) {
        // Timestamp 100 group, sorted by event_id ascending: M, Z
        TEST_ASSERT(events[0].timestamp == 100 && events[0].event_id == "EVT_M", "Index 0 should be EVT_M");
        TEST_ASSERT(events[1].timestamp == 100 && events[1].event_id == "EVT_Z", "Index 1 should be EVT_Z");

        // Timestamp 200 group, sorted by event_id ascending: A, B, C, D
        TEST_ASSERT(events[2].timestamp == 200 && events[2].event_id == "EVT_A", "Index 2 should be EVT_A");
        TEST_ASSERT(events[3].timestamp == 200 && events[3].event_id == "EVT_B", "Index 3 should be EVT_B");
        TEST_ASSERT(events[4].timestamp == 200 && events[4].event_id == "EVT_C", "Index 4 should be EVT_C");
        TEST_ASSERT(events[5].timestamp == 200 && events[5].event_id == "EVT_D", "Index 5 should be EVT_D");
    }

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_malformed_jsonl_lines() {
    const std::string test_name = "Malformed JSONL Lines Handling";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    const fs::path test_file = "test_output/malformed_test.jsonl";
    if (fs::exists(test_file)) fs::remove(test_file);

    if (test_file.has_parent_path()) {
        fs::create_directories(test_file.parent_path());
    }

    // Write a file with mixed valid, invalid JSON, missing fields, and blank lines
    {
        std::ofstream out(test_file);
        // Line 1: Valid event
        out << "{\"event_id\":\"EVT_01\",\"agent_id\":\"AG_1\",\"timestamp\":1000,\"action\":\"READ\",\"tool\":\"GITHUB\"}\n";
        // Line 2: Truncated corrupted JSON
        out << "{\"event_id\":\"EVT_BROKEN\", \"agent_id\": \n";
        // Line 3: Valid event
        out << "{\"event_id\":\"EVT_02\",\"agent_id\":\"AG_1\",\"timestamp\":1010,\"action\":\"EXEC\",\"tool\":\"SHELL\"}\n";
        // Line 4: Blank whitespace line
        out << "     \n";
        // Line 5: Missing required field 'tool'
        out << "{\"event_id\":\"EVT_NO_TOOL\",\"agent_id\":\"AG_1\",\"timestamp\":1020,\"action\":\"EXEC\"}\n";
        // Line 6: Negative timestamp
        out << "{\"event_id\":\"EVT_NEG\",\"agent_id\":\"AG_1\",\"timestamp\":-50,\"action\":\"EXEC\",\"tool\":\"BASH\"}\n";
        // Line 7: Non-JSON raw text
        out << "ERROR: System connection timeout at 12:00:00\n";
        // Line 8: Valid event
        out << "{\"event_id\":\"EVT_03\",\"agent_id\":\"AG_1\",\"timestamp\":1030,\"action\":\"EXIT\",\"tool\":\"PROCESS\"}\n";
    }

    auto loaded = load_jsonl(test_file);

    // Exactly 3 valid events should survive
    TEST_ASSERT(loaded.size() == 3, "Only the 3 valid JSON lines should be returned");
    if (loaded.size() == 3) {
        TEST_ASSERT(loaded[0].event_id == "EVT_01", "Line 1 event_id mismatch");
        TEST_ASSERT(loaded[1].event_id == "EVT_02", "Line 3 event_id mismatch");
        TEST_ASSERT(loaded[2].event_id == "EVT_03", "Line 8 event_id mismatch");
    }

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_file_round_trip_serialization() {
    const std::string test_name = "File Round-Trip Serialization";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    const fs::path test_file = "test_output/round_trip.jsonl";
    if (fs::exists(test_file)) fs::remove(test_file);

    TelemetryEvent original;
    original.event_id = "EVT_SEC_2026_001";
    original.agent_id = "AGENT_SECURITY_AUDITOR";
    original.timestamp = 1795330420;
    original.action = "READ_CREDENTIALS";
    original.tool = "VAULT_API";
    original.resource = "secret/credentials/aws_root.key";
    original.permission = "READ";
    original.intent_id = "INTENT_PROMPT_INJECTION_RECON";
    original.status = "SUCCESS";
    original.label = "SUSPICIOUS";
    original.is_synthetic = true;

    {
        TelemetryCollector collector(test_file);
        collector.push(original);
        collector.flush();
        collector.stop();
    }

    auto loaded = load_jsonl(test_file);
    TEST_ASSERT(loaded.size() == 1, "Expected 1 event round-tripped");

    if (!loaded.empty()) {
        const auto& ev = loaded[0];
        TEST_ASSERT(ev == original, "Loaded event must equal original event exactly");
        TEST_ASSERT(ev.event_id == original.event_id, "event_id mismatch");
        TEST_ASSERT(ev.agent_id == original.agent_id, "agent_id mismatch");
        TEST_ASSERT(ev.timestamp == original.timestamp, "timestamp mismatch");
        TEST_ASSERT(ev.action == original.action, "action mismatch");
        TEST_ASSERT(ev.tool == original.tool, "tool mismatch");
        TEST_ASSERT(ev.resource == original.resource, "resource mismatch");
        TEST_ASSERT(ev.permission == original.permission, "permission mismatch");
        TEST_ASSERT(ev.intent_id == original.intent_id, "intent_id mismatch");
        TEST_ASSERT(ev.status == original.status, "status mismatch");
        TEST_ASSERT(ev.label == original.label, "label mismatch");
        TEST_ASSERT(ev.is_synthetic == original.is_synthetic, "is_synthetic mismatch");
    }

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

static void test_concurrent_pushes() {
    const std::string test_name = "Concurrent Multi-Threaded Ingestion & Clean Stop";
    std::cout << "\n--- Running Test: " << test_name << " ---" << std::endl;
    bool test_passed = true;

    const fs::path test_file = "test_output/concurrent.jsonl";
    if (fs::exists(test_file)) fs::remove(test_file);

    constexpr int kThreads = 4;
    constexpr int kEventsPerThread = 50;

    {
        TelemetryCollector collector(test_file);
        std::vector<std::thread> workers;
        workers.reserve(kThreads);

        for (int t = 0; t < kThreads; ++t) {
            workers.emplace_back([t, &collector]() {
                for (int i = 0; i < kEventsPerThread; ++i) {
                    TelemetryEvent ev;
                    ev.event_id = "EVT_T" + std::to_string(t) + "_" + std::to_string(i);
                    ev.agent_id = "AGENT_CONCURRENT";
                    ev.timestamp = 1000 + (i * 10);
                    ev.action = "INVOKE_TOOL";
                    ev.tool = "API_GATEWAY";
                    collector.push(std::move(ev));
                }
            });
        }

        for (auto& th : workers) {
            th.join();
        }

        collector.flush();
        collector.stop();

        TEST_ASSERT(collector.dropped_invalid_count() == 0, "No valid events should be dropped in concurrency test");
    }

    auto loaded = load_jsonl(test_file);
    TEST_ASSERT(loaded.size() == kThreads * kEventsPerThread, "All concurrent events must be persisted");

    if (test_passed) {
        std::cout << test_name << ": PASS" << std::endl;
    } else {
        std::cout << test_name << ": FAIL" << std::endl;
        ++g_failures;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "NEXUS System 01: Telemetry Collector Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    test_missing_required_fields();
    test_out_of_order_timestamps();
    test_duplicate_and_tied_timestamps();
    test_malformed_jsonl_lines();
    test_file_round_trip_serialization();
    test_concurrent_pushes();

    std::cout << "\n========================================" << std::endl;
    if (g_failures == 0) {
        std::cout << "ALL TELEMETRY TESTS PASSED SUCCESSFULLY!" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    } else {
        std::cerr << "FAILURES DETECTED: " << g_failures << " test(s) failed." << std::endl;
        std::cout << "========================================" << std::endl;
        return 1;
    }
}
