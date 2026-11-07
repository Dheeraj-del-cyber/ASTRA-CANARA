#include "child_process.hpp"
#include "oracle.hpp"
#include "workload_generator.hpp"

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;

struct Options {
    std::uint64_t seed{42};
    std::uint64_t cycles{500};
    std::uint64_t operations_per_worker{16};
    std::uint32_t threads{4};
    std::uint32_t compact_rate{16};
    std::string db_path{"torture_phase10.db"};
    std::string oracle_path{"torture_phase10.ack.log"};
    std::string kvdb_path;
};

struct Request {
    torture::GeneratedOperation generated{};
    bool compact{false};
    std::string id;
    std::string batch_path;
    std::string description;
};

void print_usage(std::ostream& out) {
    out << "Usage: torture_tool_phase10 [options]\n"
        << "  --seed <uint>                    Workload and crash timing seed (default: 42)\n"
        << "  --iterations <int>               Compaction/crash cycles (default: 500)\n"
        << "  --threads <int>                  Concurrent worker threads (default: 4)\n"
        << "  --operations-per-cycle <int>     Maximum requests per worker per cycle (default: 16)\n"
        << "  --compact-rate <int>             Average compaction interval in requests (default: 16)\n"
        << "  --db <path>                      Database path (default: torture_phase10.db)\n"
        << "  --oracle <path>                  ACK journal path (default: torture_phase10.ack.log)\n"
        << "  --kvdb <path>                    kvdb executable (default: beside this executable)\n"
        << "  --help                           Show this help\n";
}

bool parse_unsigned(const std::string& text, std::uint64_t& value) {
    if (text.empty()) return false;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool parse_options(int argc, char* argv[], Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (i + 1 >= argc) {
            std::cerr << "Missing value for " << argument << "\n";
            return false;
        }
        const std::string value = argv[++i];
        std::uint64_t number = 0;
        if (argument == "--seed") {
            if (!parse_unsigned(value, options.seed)) {
                std::cerr << "Invalid unsigned seed: " << value << "\n";
                return false;
            }
        } else if (argument == "--iterations") {
            if (!parse_unsigned(value, options.cycles) || options.cycles == 0) {
                std::cerr << "Iterations must be a positive integer\n";
                return false;
            }
        } else if (argument == "--threads") {
            if (!parse_unsigned(value, number) || number == 0 ||
                number > std::numeric_limits<std::uint32_t>::max()) {
                std::cerr << "Threads must be a positive 32-bit integer\n";
                return false;
            }
            options.threads = static_cast<std::uint32_t>(number);
        } else if (argument == "--operations-per-cycle") {
            if (!parse_unsigned(value, options.operations_per_worker) ||
                options.operations_per_worker == 0) {
                std::cerr << "Operations per cycle must be a positive integer\n";
                return false;
            }
        } else if (argument == "--compact-rate") {
            if (!parse_unsigned(value, number) || number == 0 ||
                number > std::numeric_limits<std::uint32_t>::max()) {
                std::cerr << "Compaction rate must be a positive 32-bit integer\n";
                return false;
            }
            options.compact_rate = static_cast<std::uint32_t>(number);
        } else if (argument == "--db") {
            options.db_path = value;
        } else if (argument == "--oracle") {
            options.oracle_path = value;
        } else if (argument == "--kvdb") {
            options.kvdb_path = value;
        } else {
            std::cerr << "Unknown option: " << argument << "\n";
            return false;
        }
    }

    const auto db = std::filesystem::path(options.db_path).lexically_normal();
    const auto oracle = std::filesystem::path(options.oracle_path).lexically_normal();
    if (options.db_path.empty() || options.oracle_path.empty() || db == oracle) {
        std::cerr << "Database and oracle paths must be non-empty and different\n";
        return false;
    }
    if (options.operations_per_worker >
        std::numeric_limits<std::uint64_t>::max() / options.threads) {
        std::cerr << "Total operations per cycle exceed the supported range\n";
        return false;
    }
    return true;
}

bool resolve_kvdb_path(const char* executable_path, std::string& path, std::string& error) {
    if (!path.empty()) return true;
    std::error_code ec;
    const auto runner = std::filesystem::absolute(executable_path, ec);
    if (ec) {
        error = "cannot resolve torture runner path: " + ec.message();
        return false;
    }
#if defined(_WIN32)
    path = (runner.parent_path() / "kvdb.exe").string();
#else
    path = (runner.parent_path() / "kvdb").string();
#endif
    return true;
}

bool run_cli(const std::string& executable, const std::vector<std::string>& arguments,
             std::vector<std::string>& output, int& exit_code, std::string& error) {
    torture::ChildProcess child;
    if (!child.start(executable, arguments, error)) return false;
    if (!child.close_input(error)) {
        std::string terminate_error;
        if (!child.terminate(terminate_error)) error += "; " + terminate_error;
        return false;
    }
    while (true) {
        std::string line;
        bool eof = false;
        if (!child.read_line(line, 30s, eof, error)) {
            if (eof) break;
            std::string terminate_error;
            if (!child.terminate(terminate_error)) error += "; " + terminate_error;
            return false;
        }
        output.push_back(std::move(line));
    }
    if (!child.wait(30s, exit_code, error)) {
        std::string terminate_error;
        if (!child.terminate(terminate_error)) error += "; " + terminate_error;
        return false;
    }
    return true;
}

bool read_database_state(const Options& options, std::uint64_t cycle,
                         std::map<std::string, std::string>& state, std::string& error) {
    std::vector<std::string> output;
    int exit_code = -1;
    if (!run_cli(options.kvdb_path, {"check", options.db_path}, output, exit_code, error)) {
        error = "cycle " + std::to_string(cycle) + " integrity-check process failed: " + error;
        return false;
    }
    if (exit_code != 0 || output.size() != 1 || output.front() != "INTEGRITY_OK") {
        error = "cycle " + std::to_string(cycle) + " integrity check failed";
        if (!output.empty()) error += ": " + output.front();
        return false;
    }

    output.clear();
    if (!run_cli(options.kvdb_path, {"dump", options.db_path}, output, exit_code, error)) {
        error = "cycle " + std::to_string(cycle) + " dump process failed: " + error;
        return false;
    }
    if (exit_code != 0) {
        error = "cycle " + std::to_string(cycle) + " dump exited with code " +
            std::to_string(exit_code);
        return false;
    }

    state.clear();
    for (const auto& line : output) {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos || separator == 0 ||
            !state.emplace(line.substr(0, separator), line.substr(separator + 1)).second) {
            error = "cycle " + std::to_string(cycle) + " malformed/duplicate dump row: " + line;
            return false;
        }
    }
    return true;
}

std::string describe_difference(const std::map<std::string, std::string>& expected,
                                const std::map<std::string, std::string>& actual) {
    auto expected_it = expected.begin();
    auto actual_it = actual.begin();
    while (expected_it != expected.end() || actual_it != actual.end()) {
        if (actual_it == actual.end() ||
            (expected_it != expected.end() && expected_it->first < actual_it->first)) {
            return "missing key '" + expected_it->first + "'";
        }
        if (expected_it == expected.end() || actual_it->first < expected_it->first) {
            return "unexpected key '" + actual_it->first + "'";
        }
        if (expected_it->second != actual_it->second) {
            return "value mismatch for key '" + expected_it->first + "'";
        }
        ++expected_it;
        ++actual_it;
    }
    return "state differs";
}

bool verify_state(const Options& options, const AckOracle& oracle, std::uint64_t cycle,
                  std::string& error) {
    std::map<std::string, std::string> actual;
    if (!read_database_state(options, cycle, actual, error)) return false;
    const auto snapshot = oracle.snapshot();
    const std::map<std::string, std::string> expected(snapshot.begin(), snapshot.end());
    if (actual == expected) return true;
    error = "cycle " + std::to_string(cycle) + " state mismatch: " +
        describe_difference(expected, actual) + " (expected " +
        std::to_string(expected.size()) + " keys, found " +
        std::to_string(actual.size()) + ")";
    return false;
}

bool verify_initial_state(const Options& options, const AckOracle& oracle,
                          std::string& error) {
    std::error_code ec;
    const bool exists = std::filesystem::exists(options.db_path, ec);
    if (ec) {
        error = "cannot inspect database path '" + options.db_path + "': " + ec.message();
        return false;
    }
    const auto expected = oracle.snapshot();
    if (!exists) {
        if (!expected.empty()) {
            error = "database is missing but the ACK oracle contains " +
                std::to_string(expected.size()) + " keys";
            return false;
        }
        return true;
    }
    return verify_state(options, oracle, 0, error);
}

std::vector<kvdb::Operation> mutations_for(
    const torture::GeneratedOperation& generated) {
    if (generated.type == torture::GeneratedOperation::Type::Put) {
        return {{kvdb::Operation::Type::Put, generated.key, generated.value}};
    }
    if (generated.type == torture::GeneratedOperation::Type::Delete) {
        return {{kvdb::Operation::Type::Delete, generated.key, {}}};
    }
    if (generated.type == torture::GeneratedOperation::Type::Batch) {
        return generated.batch;
    }
    return {};
}

bool write_batch_file(const std::string& path, const std::vector<kvdb::Operation>& operations,
                      std::string& error) {
    std::error_code ec;
    if (std::filesystem::exists(path, ec) || ec) {
        error = ec ? "cannot inspect batch path '" + path + "': " + ec.message()
                   : "refusing to overwrite batch file '" + path + "'";
        return false;
    }
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        error = "cannot create batch file '" + path + "'";
        return false;
    }
    for (const auto& operation : operations) {
        if (operation.type == kvdb::Operation::Type::Put) {
            output << "PUT " << operation.key << " " << operation.value << "\n";
        } else {
            output << "DEL " << operation.key << "\n";
        }
    }
    output.close();
    if (!output) {
        error = "failed writing batch file '" + path + "'";
        return false;
    }
    return true;
}

std::string describe_request(const Request& request) {
    if (request.compact) return "COMPACT";
    switch (request.generated.type) {
    case torture::GeneratedOperation::Type::Put:
        return "PUT " + request.generated.key + "=" + request.generated.value;
    case torture::GeneratedOperation::Type::Get:
        return "GET " + request.generated.key;
    case torture::GeneratedOperation::Type::Delete:
        return "DEL " + request.generated.key;
    case torture::GeneratedOperation::Type::Batch:
        return "BATCH " + std::to_string(request.generated.batch.size()) + " ops";
    }
    return "unknown";
}

bool make_request(torture::WorkloadGenerator& generator, std::mt19937_64& compaction_rng,
                  std::uint32_t compact_rate, std::uint32_t worker_id,
                  std::uint64_t operation_index, std::uint64_t nonce,
                  Request& request, std::string& error) {
    request.generated = generator.next();
    request.id = std::to_string(worker_id) + "_" + std::to_string(operation_index);
    request.compact = std::uniform_int_distribution<std::uint32_t>(
        0, compact_rate - 1)(compaction_rng) == 0;
    if (!request.compact &&
        request.generated.type == torture::GeneratedOperation::Type::Batch) {
        request.batch_path = "torture_phase10_" + std::to_string(nonce) + "_w" +
            std::to_string(worker_id) + "_op" + std::to_string(operation_index) + ".batch";
        if (!write_batch_file(request.batch_path, request.generated.batch, error)) return false;
    }
    request.description = describe_request(request);
    return true;
}

class CompactionSession {
public:
    CompactionSession(const Options& options, AckOracle& oracle, std::uint64_t cycle,
                      std::uint64_t stop_after)
        : options_(options), oracle_(oracle), cycle_(cycle), stop_after_(stop_after) {}

    bool start(std::string& error) {
        if (!server_.start(options_.kvdb_path, {"serve", options_.db_path}, error)) return false;
        if (!server_.write_line("PING phase10_ready", error)) return false;
        std::string response;
        bool eof = false;
        if (!server_.read_line(response, 30s, eof, error) || response != "PONG phase10_ready") {
            if (error.empty()) error = "unexpected server startup response: " + response;
            return false;
        }
        return true;
    }

    bool execute(const Request& request, std::uint32_t worker_id,
                 std::uint64_t operation_index, std::string& error) {
        std::lock_guard<std::mutex> lock(io_mutex_);
        if (stopped_) return false;
        if (completed_ >= stop_after_) {
            if (!server_.request_termination(error)) {
                fatal_error_ = "could not inject cycle crash: " + error;
                return false;
            }
            stopped_ = true;
            return false;
        }

        const auto mutations = mutations_for(request.generated);
        std::string command;
        if (request.compact) {
            command = "COMPACT " + request.id;
        } else if (request.generated.type == torture::GeneratedOperation::Type::Put) {
            command = "PUT " + request.id + " " + request.generated.key + " " +
                request.generated.value;
        } else if (request.generated.type == torture::GeneratedOperation::Type::Get) {
            command = "GET " + request.id + " " + request.generated.key;
        } else if (request.generated.type == torture::GeneratedOperation::Type::Delete) {
            command = "DEL " + request.id + " " + request.generated.key;
        } else {
            command = "BATCH " + request.id + " " + request.batch_path;
        }

        std::string response;
        bool eof = false;
        if (!server_.write_line(command, error) ||
            !server_.read_line(response, 30s, eof, error)) {
            fatal_error_ = "child failed during " + request.description + ": " + error;
            add_history(worker_id, operation_index, request.description + " [no response]");
            return false;
        }
        if (!validate_response(request, response, error)) {
            fatal_error_ = "worker " + std::to_string(worker_id) + " operation " +
                std::to_string(operation_index) + ": " + error;
            add_history(worker_id, operation_index, request.description + " [invalid response]");
            return false;
        }

        if (!record_acknowledged_mutation(request.generated, request.compact, error)) {
            fatal_error_ = "ACK received but oracle update failed: " + error;
            add_history(worker_id, operation_index, request.description + " [oracle failure]");
            return false;
        }
        ++completed_;
        if (request.compact) ++compactions_;
        add_history(worker_id, operation_index, request.description + " [ACK]");
        return true;
    }

    bool stop(std::string& error) {
        std::lock_guard<std::mutex> lock(io_mutex_);
        if (stopped_) {
            int exit_code = -1;
            return server_.wait(30s, exit_code, error);
        }
        stopped_ = true;
        return server_.terminate(error);
    }

    bool stopped() const {
        return stopped_.load();
    }

    std::string fatal_error() const {
        std::lock_guard<std::mutex> lock(io_mutex_);
        return fatal_error_;
    }

    std::uint64_t acknowledgements() const {
        return completed_.load();
    }

    std::uint64_t compactions() const {
        return compactions_.load();
    }

    std::vector<std::string> history() const {
        std::lock_guard<std::mutex> lock(history_mutex_);
        return history_;
    }

private:
    bool validate_response(const Request& request, const std::string& response,
                           std::string& error) {
        if (!request.compact &&
            request.generated.type == torture::GeneratedOperation::Type::Get) {
            const auto expected = oracle_.get(request.generated.key);
            const std::string prefix = "VALUE " + request.id + " ";
            if (expected.has_value()) {
                if (response.compare(0, prefix.size(), prefix) != 0 ||
                    response.substr(prefix.size()) != *expected) {
                    error = "GET response mismatch for " + request.generated.key;
                    return false;
                }
            } else if (response != "NOT_FOUND " + request.id) {
                error = "GET expected NOT_FOUND for " + request.generated.key +
                    ", got: " + response;
                return false;
            }
            return true;
        }

        if (response != "ACK " + request.id) {
            error = "expected ACK " + request.id + ", got: " + response;
            return false;
        }
        return true;
    }

    bool record_acknowledged_mutation(const torture::GeneratedOperation& generated,
                                      bool compact, std::string& error) {
        if (compact || generated.type == torture::GeneratedOperation::Type::Get) return true;
        if (generated.type == torture::GeneratedOperation::Type::Put) {
            return oracle_.record_put(generated.key, generated.value, error);
        }
        if (generated.type == torture::GeneratedOperation::Type::Delete) {
            return oracle_.record_del(generated.key, error);
        }
        return oracle_.record_batch(generated.batch, error);
    }

    void add_history(std::uint32_t worker_id, std::uint64_t operation_index,
                     const std::string& text) {
        std::lock_guard<std::mutex> lock(history_mutex_);
        history_.push_back("cycle " + std::to_string(cycle_) + " worker " +
            std::to_string(worker_id) + " op " + std::to_string(operation_index) +
            ": " + text);
        if (history_.size() > 40) history_.erase(history_.begin());
    }

    const Options& options_;
    AckOracle& oracle_;
    std::uint64_t cycle_;
    std::uint64_t stop_after_;
    torture::ChildProcess server_;
    mutable std::mutex io_mutex_;
    mutable std::mutex history_mutex_;
    std::atomic<bool> stopped_{false};
    std::atomic<std::uint64_t> completed_{0};
    std::atomic<std::uint64_t> compactions_{0};
    std::vector<std::string> history_;
    std::string fatal_error_;
};

bool run_cycle(const Options& options, AckOracle& oracle, std::uint64_t cycle,
               std::uint64_t nonce, std::mt19937_64& crash_rng,
               std::vector<std::unique_ptr<torture::WorkloadGenerator>>& generators,
               std::vector<std::mt19937_64>& compaction_rngs,
               std::vector<std::uint64_t>& operation_indices, std::uint64_t& compactions,
               std::string& error) {
    const std::uint64_t total_requests =
        options.operations_per_worker * options.threads;
    const std::uint64_t stop_after = std::uniform_int_distribution<std::uint64_t>(
        1, total_requests)(crash_rng);
    CompactionSession session(options, oracle, cycle, stop_after);
    if (!session.start(error)) {
        std::string stop_error;
        if (!session.stop(stop_error)) error += "; " + stop_error;
        return false;
    }

    std::atomic<bool> failed{false};
    std::vector<std::string> errors(options.threads);
    std::vector<std::thread> workers;
    workers.reserve(options.threads);
    for (std::uint32_t worker_id = 0; worker_id < options.threads; ++worker_id) {
        workers.emplace_back([&, worker_id] {
            for (std::uint64_t count = 0; count < options.operations_per_worker; ++count) {
                if (failed.load() || session.stopped()) break;
                const std::uint64_t operation_index = operation_indices[worker_id]++;
                Request request;
                std::string operation_error;
                if (!make_request(*generators[worker_id], compaction_rngs[worker_id],
                                  options.compact_rate, worker_id, operation_index,
                                  nonce, request, operation_error)) {
                    errors[worker_id] = operation_error;
                    failed.store(true);
                    break;
                }
                const bool completed =
                    session.execute(request, worker_id, operation_index, operation_error);
                if (!request.batch_path.empty()) {
                    std::error_code ec;
                    std::filesystem::remove(request.batch_path, ec);
                    if (ec && operation_error.empty()) {
                        operation_error = "cannot remove batch file '" + request.batch_path +
                            "': " + ec.message();
                    }
                }
                if (!operation_error.empty()) {
                    errors[worker_id] = operation_error;
                    failed.store(true);
                    break;
                }
                if (!completed) {
                    if (!session.fatal_error().empty()) {
                        errors[worker_id] = session.fatal_error();
                        failed.store(true);
                    }
                    break;
                }
            }
        });
    }
    for (auto& worker : workers) worker.join();

    std::string stop_error;
    if (!session.stop(stop_error)) {
        error = "could not stop/reap child: " + stop_error;
        return false;
    }
    if (failed.load()) {
        for (const auto& item : errors) {
            if (!item.empty()) {
                error = item;
                break;
            }
        }
        for (const auto& item : session.history()) std::cerr << "[Torture]   " << item << "\n";
        return false;
    }
    compactions += session.compactions();
    return verify_state(options, oracle, cycle, error);
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
    std::string error;
    if (!resolve_kvdb_path(argv[0], options.kvdb_path, error)) {
        std::cerr << error << "\n";
        return 1;
    }

    std::unique_ptr<AckOracle> oracle;
    try {
        oracle = std::make_unique<AckOracle>(options.oracle_path);
    } catch (const std::exception& ex) {
        std::cerr << "Failed to load ACK oracle: " << ex.what() << "\n";
        return 1;
    }
    if (!verify_initial_state(options, *oracle, error)) {
        std::cerr << "[Torture] Refusing to start: " << error << "\n";
        return 1;
    }

    std::vector<std::unique_ptr<torture::WorkloadGenerator>> generators;
    std::vector<std::mt19937_64> compaction_rngs;
    std::vector<std::uint64_t> operation_indices(options.threads, 0);
    generators.reserve(options.threads);
    compaction_rngs.reserve(options.threads);
    for (std::uint32_t worker_id = 0; worker_id < options.threads; ++worker_id) {
        generators.push_back(std::make_unique<torture::WorkloadGenerator>(
            options.seed, worker_id, torture::WorkloadMode::All));
        compaction_rngs.emplace_back(options.seed ^
            (0x9e3779b97f4a7c15ULL * (static_cast<std::uint64_t>(worker_id) + 1)));
    }

    std::mt19937_64 crash_rng(options.seed);
    const std::uint64_t nonce = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::uint64_t total_compactions = 0;
    std::cout << "[Torture] Phase 10 seed=" << options.seed
              << " cycles=" << options.cycles
              << " threads=" << options.threads
              << " operations_per_worker=" << options.operations_per_worker
              << " compact_rate=1/" << options.compact_rate << "\n";

    for (std::uint64_t cycle = 0; cycle < options.cycles; ++cycle) {
        if (!run_cycle(options, *oracle, cycle, nonce, crash_rng, generators,
                       compaction_rngs, operation_indices, total_compactions, error)) {
            std::cerr << "[Torture] FAILURE seed=" << options.seed
                      << " cycle=" << cycle << ": " << error << "\n";
            return 1;
        }
        if ((cycle + 1) % 25 == 0 || cycle + 1 == options.cycles) {
            std::cout << "[Torture] Verified " << cycle + 1 << "/"
                      << options.cycles << " crash/recovery cycles"
                      << " (compactions=" << total_compactions << ")\n";
        }
    }

    std::cout << "[Torture] Passed compaction-under-load crash testing\n";
    return 0;
}
