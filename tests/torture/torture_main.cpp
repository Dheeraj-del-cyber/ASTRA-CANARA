#include "kvdb/database.hpp"
#include "oracle.hpp"
#include <iostream>
#include <exception>
#include <memory>
#include <random>
#include <string>

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
    std::unique_ptr<AckOracle> oracle;
    try {
        oracle = std::make_unique<AckOracle>("oracle.ack.log");
    } catch (const std::exception& ex) {
        std::cerr << "[Torture] Failed to load ACK oracle: " << ex.what() << "\n";
        return 1;
    }

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
                if (!kvdb::is_ok(s)) {
                    std::cerr << "[Torture] PUT failed: " << kvdb::status_to_string(s) << "\n";
                    return 1;
                }
                // The database call returns only after its durable ACK.
                std::string error;
                if (!oracle->record_put(key, val, error)) {
                    std::cerr << "[Torture] Failed to persist acknowledged PUT: " << error << "\n";
                    return 1;
                }
            } else if (op == 1) { // GET
                auto res = db.get(key);
                const auto expected = oracle->get(key);
                if (expected.has_value()) {
                    if (!kvdb::is_ok(res.status) || res.value != *expected) {
                        std::cerr << "[Torture] Mismatch on GET " << key << ": expected " << *expected << ", got " << res.value << "\n";
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
                if (!kvdb::is_ok(s)) {
                    std::cerr << "[Torture] DEL failed: " << kvdb::status_to_string(s) << "\n";
                    return 1;
                }
                std::string error;
                if (!oracle->record_del(key, error)) {
                    std::cerr << "[Torture] Failed to persist acknowledged DEL: " << error << "\n";
                    return 1;
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

        for (const auto& [k, v] : oracle->snapshot()) {
            auto res = db.get(k);
            if (!kvdb::is_ok(res.status) || res.value != v) {
                std::cerr << "[Torture] Post-recovery mismatch for key " << k << "\n";
                return 1;
            }
        }
        std::cout << "[Torture] Passed all " << iterations << " operations successfully!\n";
    }

    return 0;
}
