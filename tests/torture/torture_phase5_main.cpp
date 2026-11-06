#include "kvdb/database.hpp"
#include "oracle.hpp"
#include "workload_generator.hpp"

#include <atomic>
#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
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
};

void print_usage(std::ostream& out) {
    out << "Usage: torture_tool_phase5 [options]\n"
        << "  --seed <uint>        PRNG seed (default: 42)\n"
        << "  --iterations <int>   Total top-level operations (default: 500)\n"
        << "  --threads <int>      Worker threads (default: 4)\n"
        << "  --db <path>          Database path (default: torture_test.db)\n"
        << "  --oracle <path>      ACK journal path (default: oracle.ack.log)\n"
        << "  --mode <all|batch>   Workload selection (default: all)\n"
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
                    std::string& error) {
    const kvdb::Status status = operation.type == kvdb::Operation::Type::Put
        ? db.put(operation.key, operation.value)
        : db.remove(operation.key);
    if (!kvdb::is_ok(status)) {
        error = kvdb::status_to_string(status);
        return false;
    }

    if (operation.type == kvdb::Operation::Type::Put) {
        return oracle.record_put(operation.key, operation.value, error);
    }
    return oracle.record_del(operation.key, error);
}

bool record_acknowledged_mutation(AckOracle& oracle, const kvdb::Operation& operation,
                                  std::string& error) {
    if (operation.type == kvdb::Operation::Type::Put) {
        return oracle.record_put(operation.key, operation.value, error);
    }
    return oracle.record_del(operation.key, error);
}

bool verify_get(kvdb::Database& db, const AckOracle& oracle, const std::string& key,
                std::string& error) {
    const auto actual = db.get(key);
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
                       const torture::GeneratedOperation& generated, std::string& error) {
    if (generated.type == torture::GeneratedOperation::Type::Put) {
        return apply_mutation(db, oracle,
            {kvdb::Operation::Type::Put, generated.key, generated.value}, error);
    }
    if (generated.type == torture::GeneratedOperation::Type::Get) {
        return verify_get(db, oracle, generated.key, error);
    }
    if (generated.type == torture::GeneratedOperation::Type::Delete) {
        return apply_mutation(db, oracle,
            {kvdb::Operation::Type::Delete, generated.key, {}}, error);
    }

    const kvdb::Status status = db.batch(generated.batch);
    if (!kvdb::is_ok(status)) {
        error = std::string("BATCH failed: ") + kvdb::status_to_string(status);
        return false;
    }
    for (const auto& operation : generated.batch) {
        if (!record_acknowledged_mutation(oracle, operation, error)) {
            error = "BATCH acknowledged but oracle update failed: " + error;
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
    std::vector<std::string> errors(options.threads);
    std::vector<std::thread> workers;
    workers.reserve(options.threads);

    const std::uint64_t base_count = options.iterations / options.threads;
    const std::uint64_t remainder = options.iterations % options.threads;
    for (std::uint32_t worker_id = 0; worker_id < options.threads; ++worker_id) {
        const std::uint64_t operation_count = base_count + (worker_id < remainder ? 1 : 0);
        workers.emplace_back([&, worker_id, operation_count] {
            try {
                torture::WorkloadGenerator generator(options.seed, worker_id, options.mode);
                for (std::uint64_t operation_index = 0;
                     operation_index < operation_count && !failed.load(); ++operation_index) {
                    const auto operation = generator.next();
                    std::string error;
                    if (!execute_generated(db, *oracle, operation, error)) {
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

    for (auto& worker : workers) worker.join();

    if (failed.load()) {
        for (const auto& error : errors) {
            if (!error.empty()) std::cerr << "[Torture] " << error << "\n";
        }
        db.close();
        return 1;
    }

    for (const auto& entry : oracle->snapshot()) {
        const auto actual = db.get(entry.first);
        if (!kvdb::is_ok(actual.status) || actual.value != entry.second) {
            std::cerr << "[Torture] Final state mismatch for key '" << entry.first << "'\n";
            db.close();
            return 1;
        }
    }

    const kvdb::Status close_status = db.close();
    if (!kvdb::is_ok(close_status)) {
        std::cerr << "Failed to close database: " << kvdb::status_to_string(close_status) << "\n";
        return 1;
    }

    std::cout << "[Torture] Passed " << options.iterations << " operations across "
              << options.threads << " workers\n";
    return 0;
}
