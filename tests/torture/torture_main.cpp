#include "kvdb/database.hpp"
#include <iostream>
#include <random>
#include <unordered_map>
#include <string>
#include <cassert>

// Independent Oracle
class SimpleOracle {
public:
    void record_put(const std::string& key, const std::string& val) {
        state_[key] = val;
    }
    void record_del(const std::string& key) {
        state_.erase(key);
    }
    const std::unordered_map<std::string, std::string>& state() const {
        return state_;
    }
private:
    std::unordered_map<std::string, std::string> state_;
};

int main(int argc, char* argv[]) {
    unsigned int seed = 42;
    int iterations = 100;
    if (argc > 1) seed = std::stoul(argv[1]);
    if (argc > 2) iterations = std::stoi(argv[2]);

    std::cout << "[Torture] Starting torture test with seed: " << seed << ", iterations: " << iterations << "\n";
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> op_dist(0, 2); // 0=PUT, 1=GET, 2=DEL
    std::uniform_int_distribution<int> key_dist(1, 1000);

    std::string test_db = "torture_test.db";
    // Remove old test db
    std::remove(test_db.c_str());

    SimpleOracle oracle;
    {
        kvdb::Database db;
        kvdb::Status s = db.open(test_db);
        if (!kvdb::is_ok(s)) {
            std::cerr << "[Torture] Failed to open database: " << kvdb::status_to_string(s) << "\n";
            return 1;
        }

        for (int i = 0; i < iterations; ++i) {
            int op = op_dist(rng);
            std::string key = "key_" + std::to_string(key_dist(rng));

            if (op == 0) { // PUT
                std::string val = "val_" + std::to_string(i);
                s = db.put(key, val);
                if (kvdb::is_ok(s)) {
                    // Only update oracle after durable ACK
                    oracle.record_put(key, val);
                }
            } else if (op == 1) { // GET
                auto res = db.get(key);
                auto it = oracle.state().find(key);
                if (it != oracle.state().end()) {
                    if (!kvdb::is_ok(res.status) || res.value != it->second) {
                        std::cerr << "[Torture] Mismatch on GET " << key << ": expected " << it->second << ", got " << res.value << "\n";
                        return 1;
                    }
                } else {
                    if (res.status != kvdb::Status::NotFound) {
                        std::cerr << "[Torture] Expected NotFound for " << key << ", got " << kvdb::status_to_string(res.status) << "\n";
                        return 1;
                    }
                }
            } else if (op == 2) { // DEL
                s = db.remove(key);
                if (kvdb::is_ok(s)) {
                    oracle.record_del(key);
                }
            }
        }
        db.close();
    }

    // Reopen and verify final state against oracle
    {
        kvdb::Database db;
        kvdb::Status s = db.open(test_db);
        if (!kvdb::is_ok(s)) {
            std::cerr << "[Torture] Reopen failed: " << kvdb::status_to_string(s) << "\n";
            return 1;
        }

        for (const auto& [k, v] : oracle.state()) {
            auto res = db.get(k);
            if (!kvdb::is_ok(res.status) || res.value != v) {
                std::cerr << "[Torture] Post-recovery mismatch for key " << k << "\n";
                return 1;
            }
        }
        std::cout << "[Torture] Passed all " << iterations << " operations successfully!\n";
    }

    std::remove(test_db.c_str());
    return 0;
}

