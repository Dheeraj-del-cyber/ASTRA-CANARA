#include "kvdb/database.hpp"
#include "oracle.hpp"
#include "workload_generator.hpp"

#include <atomic>
#include <charconv>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Options {
    std::uint64_t seed{42};
    std::uint64_t iterations{500};
    std::uint32_t threads{4};
    std::string db_path{"torture_test.db"};
    std::string oracle_path{"oracle.ack.log"};
    torture::WorkloadMode mode{torture::WorkloadMode::All};
    bool reset{false};
};

class StartBarrier {
public:
    explicit StartBarrier(std::size_t participants) : participants_(participants) {}

    void arrive_and_wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        ++waiting_;
        condition_.notify_all();
        condition_.wait(lock, [this] { return released_; });
    }

    void release_when_ready() {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return waiting_ == participants_; });
        released_ = true;
        lock.unlock();
        condition_.notify_all();
    }

private:
    const std::size_t participants_;
    std::size_t waiting_{0};
    bool released_{false};
    std::mutex mutex_;
    std::condition_variable condition_;
};

class ActiveDatabaseCall {
public:
    ActiveDatabaseCall(std::atomic<std::size_t>& active,
                       std::atomic<std::size_t>& peak)
        : active_(active), peak_(peak) {
        const std::size_t current = active_.fetch_add(1, std::memory_order_relaxed) + 1;
        std::size_t observed = peak_.load(std::memory_order_relaxed);
        while (observed < current &&
               !peak_.compare_exchange_weak(observed, current, std::memory_order_relaxed)) {
        }
    }

    ~ActiveDatabaseCall() {
        active_.fetch_sub(1, std::memory_order_relaxed);
    }

    ActiveDatabaseCall(const ActiveDatabaseCall&) = delete;
    ActiveDatabaseCall& operator=(const ActiveDatabaseCall&) = delete;

private:
    std::atomic<std::size_t>& active_;
    std::atomic<std::size_t>& peak_;
};

struct DatabaseCallTracker {
    std::atomic<std::size_t> active{0};
    std::atomic<std::size_t> peak{0};
};

void print_usage(std::ostream& out) {
    out << "Usage: torture_tool_phase5 [options]\n"
        << "  --seed <uint>        PRNG seed (default: 42)\n"
        << "  --iterations <int>   Total top-level operations (default: 500)\n"
        << "  --threads <int>      Worker threads (default: 4)\n"
        << "  --db <path>          Database path (default: torture_test.db)\n"
        << "  --oracle <path>      ACK journal path (default: oracle.ack.log)\n"
        << "  --mode <all|batch>   Workload selection (default: all)\n"
        << "  --reset              Remove the specified database and oracle before starting\n"
        << "  --help               Show this help\n";
}

bool parse_unsigned(const std::string& text, std::uint64_t& value) {
    if (text.empty()) return false;
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto parsed = std::from_chars(begin, end, value);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool parse_options(int argc, char* argv[], Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(std::cout);
            return false;
        }
        if (arg == "--reset") {
            options.reset = true;
            continue;
        }

        if (i + 1 >= argc) {
            std::cerr << "Missing value for " << arg << "\n";
            return false;
        }
        const std::string value = argv[++i];

        if (arg == "--seed") {
            if (!parse_unsigned(value, options.seed)) {
                std::cerr << "Invalid unsigned seed: " << value << "\n";
                return false;
            }
        } else if (arg == "--iterations") {
            if (!parse_unsigned(value, options.iterations) || options.iterations == 0) {
                std::cerr << "Iterations must be a positive integer\n";
                return false;
            }
        } else if (arg == "--threads") {
            std::uint64_t threads = 0;
            if (!parse_unsigned(value, threads) || threads == 0 ||
                threads > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
                std::cerr << "Threads must be a positive 32-bit integer\n";
                return false;
            }
            options.threads = static_cast<std::uint32_t>(threads);
        } else if (arg == "--db") {
            if (value.empty()) {
                std::cerr << "Database path cannot be empty\n";
                return false;
            }
            options.db_path = value;
        } else if (arg == "--oracle") {
            if (value.empty()) {
                std::cerr << "Oracle path cannot be empty\n";
                return false;
            }
            options.oracle_path = value;
        } else if (arg == "--mode") {
            if (value == "all") {
                options.mode = torture::WorkloadMode::All;
            } else if (value == "batch") {
                options.mode = torture::WorkloadMode::BatchOnly;
            } else {
                std::cerr << "Unsupported mode: " << value << " (supported: all, batch)\n";
                return false;
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
    }
    return true;
}

bool apply_mutation(kvdb::Database& db, AckOracle& oracle, const kvdb::Operation& operation,
                    DatabaseCallTracker& calls, std::string& error) {
    kvdb::Status status;
    {
        ActiveDatabaseCall active_call(calls.active, calls.peak);
        status = operation.type == kvdb::Operation::Type::Put
            ? db.put(operation.key, operation.value)
            : db.remove(operation.key);
    }
    if (!kvdb::is_ok(status)) {
        error = kvdb::status_to_string(status);
        return false;
    }

    if (operation.type == kvdb::Operation::Type::Put) {
        return oracle.record_put(operation.key, operation.value, error);
    }
    return oracle.record_del(operation.key, error);
}

bool verify_get(kvdb::Database& db, const AckOracle& oracle, const std::string& key,
                DatabaseCallTracker& calls, std::string& error) {
    kvdb::Result actual;
    {
        ActiveDatabaseCall active_call(calls.active, calls.peak);
        actual = db.get(key);
    }
    const auto expected = oracle.get(key);
    if (expected.has_value()) {
        if (!kvdb::is_ok(actual.status) || actual.value != *expected) {
            error = "GET mismatch for key '" + key + "'";
            return false;
        }
    } else if (actual.status != kvdb::Status::NotFound) {
        error = "GET expected NOT_FOUND for key '" + key + "', got " +
                kvdb::status_to_string(actual.status);
        return false;
    }
    return true;
}

bool execute_generated(kvdb::Database& db, AckOracle& oracle,
                       const torture::GeneratedOperation& generated,
                       DatabaseCallTracker& calls, std::string& error) {
    if (generated.type == torture::GeneratedOperation::Type::Put) {
        return apply_mutation(db, oracle,
            {kvdb::Operation::Type::Put, generated.key, generated.value}, calls, error);
    }
    if (generated.type == torture::GeneratedOperation::Type::Get) {
        return verify_get(db, oracle, generated.key, calls, error);
    }
    if (generated.type == torture::GeneratedOperation::Type::Delete) {
        return apply_mutation(db, oracle,
            {kvdb::Operation::Type::Delete, generated.key, {}}, calls, error);
    }

    kvdb::Status status;
    {
        ActiveDatabaseCall active_call(calls.active, calls.peak);
        status = db.batch(generated.batch);
    }
    if (!kvdb::is_ok(status)) {
        error = std::string("BATCH failed: ") + kvdb::status_to_string(status);
        return false;
    }
    if (!oracle.record_batch(generated.batch, error)) {
        error = "BATCH acknowledged but oracle update failed: " + error;
        return false;
    }
    return true;
}

bool run_same_key_test(kvdb::Database& db, AckOracle& oracle, std::uint64_t seed,
                       DatabaseCallTracker& calls, std::string& error) {
    constexpr std::size_t worker_count = 2;
    const std::string key = "phase5_same_key_test";
    const std::vector<std::string> values{
        "complete-value-worker-0-seed-" + std::to_string(seed),
        "complete-value-worker-1-seed-" + std::to_string(seed)
    };
    StartBarrier start_barrier(worker_count);
    std::mutex acknowledged_mutex;
    std::vector<std::string> acknowledged_values;
    std::vector<std::string> errors(worker_count);
    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    for (std::size_t worker_id = 0; worker_id < worker_count; ++worker_id) {
        workers.emplace_back([&, worker_id] {
            try {
                start_barrier.arrive_and_wait();
                kvdb::Status status;
                {
                    ActiveDatabaseCall active_call(calls.active, calls.peak);
                    status = db.put(key, values[worker_id]);
                }
                if (!kvdb::is_ok(status)) {
                    errors[worker_id] = "same-key PUT failed: " +
                        std::string(kvdb::status_to_string(status));
                    return;
                }
                std::lock_guard<std::mutex> lock(acknowledged_mutex);
                acknowledged_values.push_back(values[worker_id]);
            } catch (const std::exception& ex) {
                errors[worker_id] = std::string("same-key worker failed: ") + ex.what();
            }
        });
    }
    start_barrier.release_when_ready();
    for (auto& worker : workers) worker.join();

    for (const auto& worker_error : errors) {
        if (!worker_error.empty()) {
            error = worker_error;
            return false;
        }
    }
    if (acknowledged_values.size() != worker_count) {
        error = "same-key test did not acknowledge both PUTs";
        return false;
    }

    kvdb::Result actual;
    {
        ActiveDatabaseCall active_call(calls.active, calls.peak);
        actual = db.get(key);
    }
    if (!kvdb::is_ok(actual.status)) {
        error = "same-key GET failed: " + std::string(kvdb::status_to_string(actual.status));
        return false;
    }
    bool matches_acknowledged_value = false;
    for (const auto& value : acknowledged_values) {
        if (actual.value == value) {
            matches_acknowledged_value = true;
            break;
        }
    }
    if (!matches_acknowledged_value) {
        error = "same-key final value is not one of the acknowledged complete values";
        return false;
    }

    kvdb::Status status;
    {
        ActiveDatabaseCall active_call(calls.active, calls.peak);
        status = db.remove(key);
    }
    if (!kvdb::is_ok(status)) {
        error = "same-key test cleanup failed: " +
            std::string(kvdb::status_to_string(status));
        return false;
    }
    if (!oracle.record_del(key, error)) {
        error = "same-key test cleanup was acknowledged but oracle update failed: " + error;
        return false;
    }
    return true;
}

bool verify_final_state(kvdb::Database& db, const AckOracle& oracle, std::string& error) {
    std::ostringstream dump;
    const kvdb::Status status = db.dump(dump);
    if (!kvdb::is_ok(status)) {
        error = std::string("database dump failed: ") + kvdb::status_to_string(status);
        return false;
    }

    std::map<std::string, std::string> actual;
    std::istringstream lines(dump.str());
    std::string line;
    while (std::getline(lines, line)) {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos || separator == 0 ||
            !actual.emplace(line.substr(0, separator), line.substr(separator + 1)).second) {
            error = "malformed or duplicate database dump row: " + line;
            return false;
        }
    }

    const auto expected = oracle.snapshot();
    for (const auto& entry : expected) {
        const auto found = actual.find(entry.first);
        if (found == actual.end() || found->second != entry.second) {
            error = "final state mismatch for key '" + entry.first + "'";
            return false;
        }
    }
    for (const auto& entry : actual) {
        if (expected.find(entry.first) == expected.end()) {
            error = "database contains unacknowledged key '" + entry.first + "'";
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h") {
            print_usage(std::cout);
            return 0;
        }
    }

    Options options;
    if (!parse_options(argc, argv, options)) {
        print_usage(std::cerr);
        return 1;
    }

    if (options.reset) {
        const std::string test_paths[] = {options.db_path, options.oracle_path};
        for (const auto& path : test_paths) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
            if (ec) {
                std::cerr << "Failed to remove test file '" << path << "': " << ec.message() << "\n";
                return 1;
            }
        }
    }

    std::unique_ptr<AckOracle> oracle;
    try {
        oracle = std::make_unique<AckOracle>(options.oracle_path);
    } catch (const std::exception& ex) {
        std::cerr << "Failed to load ACK oracle: " << ex.what() << "\n";
        return 1;
    }

    kvdb::Database db;
    const kvdb::Status open_status = db.open(options.db_path);
    if (!kvdb::is_ok(open_status)) {
        std::cerr << "Failed to open database: " << kvdb::status_to_string(open_status) << "\n";
        return 1;
    }

    std::cout << "[Torture] Phase 5 seed=" << options.seed
              << " operations=" << options.iterations
              << " threads=" << options.threads << "\n";

    std::atomic<bool> failed{false};
    DatabaseCallTracker calls;
    StartBarrier start_barrier(options.threads);
    std::vector<std::string> errors(options.threads);
    std::vector<std::thread> workers;
    workers.reserve(options.threads);

    const std::uint64_t base_count = options.iterations / options.threads;
    const std::uint64_t remainder = options.iterations % options.threads;
    for (std::uint32_t worker_id = 0; worker_id < options.threads; ++worker_id) {
        const std::uint64_t operation_count = base_count + (worker_id < remainder ? 1 : 0);
        workers.emplace_back([&, worker_id, operation_count] {
            try {
                start_barrier.arrive_and_wait();
                torture::WorkloadGenerator generator(options.seed, worker_id, options.mode);
                for (std::uint64_t operation_index = 0;
                     operation_index < operation_count && !failed.load(); ++operation_index) {
                    const auto operation = generator.next();
                    std::string error;
                    if (failed.load()) break;
                    if (!execute_generated(db, *oracle, operation, calls, error)) {
                        errors[worker_id] = "worker " + std::to_string(worker_id) +
                            " operation " + std::to_string(operation_index) + ": " + error;
                        failed.store(true);
                        break;
                    }
                }
            } catch (const std::exception& ex) {
                errors[worker_id] = "worker " + std::to_string(worker_id) +
                    " failed: " + ex.what();
                failed.store(true);
            }
        });
    }

    start_barrier.release_when_ready();
    for (auto& worker : workers) worker.join();

    const std::size_t peak_calls = calls.peak.load(std::memory_order_relaxed);
    if (failed.load()) {
        for (const auto& error : errors) {
            if (!error.empty()) std::cerr << "[Torture] " << error << "\n";
        }
        std::cerr << "[Torture] Peak simultaneous database calls=" << peak_calls << "\n";
        if (peak_calls <= 1) {
            std::cerr << "[Torture] Concurrency check failed: peak must exceed one\n";
        }
        db.close();
        return 1;
    }

    if (peak_calls <= 1) {
        std::cerr << "[Torture] Concurrency check failed: peak simultaneous database calls was "
                  << peak_calls << "\n";
        db.close();
        return 1;
    }

    std::string verification_error;
    if (!verify_final_state(db, *oracle, verification_error)) {
        std::cerr << "[Torture] " << verification_error << "\n";
        db.close();
        return 1;
    }

    const kvdb::Status close_status = db.close();
    if (!kvdb::is_ok(close_status)) {
        std::cerr << "Failed to close database: " << kvdb::status_to_string(close_status) << "\n";
        return 1;
    }

    const kvdb::Status reopen_status = db.open(options.db_path);
    if (!kvdb::is_ok(reopen_status)) {
        std::cerr << "Failed to reopen database: " << kvdb::status_to_string(reopen_status) << "\n";
        return 1;
    }
    if (!verify_final_state(db, *oracle, verification_error)) {
        std::cerr << "[Torture] " << verification_error << "\n";
        db.close();
        return 1;
    }

    if (!run_same_key_test(db, *oracle, options.seed, calls, verification_error)) {
        std::cerr << "[Torture] " << verification_error << "\n";
        db.close();
        return 1;
    }
    if (!verify_final_state(db, *oracle, verification_error)) {
        std::cerr << "[Torture] " << verification_error << "\n";
        db.close();
        return 1;
    }
    const kvdb::Status final_close_status = db.close();
    if (!kvdb::is_ok(final_close_status)) {
        std::cerr << "Failed to close reopened database: "
                  << kvdb::status_to_string(final_close_status) << "\n";
        return 1;
    }

    std::cout << "[Torture] Passed " << options.iterations << " operations across "
              << options.threads << " workers; peak simultaneous database calls="
              << peak_calls << "\n";
    return 0;
}
