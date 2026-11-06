#pragma once

#include "kvdb/types.hpp"

#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace torture {

enum class WorkloadMode {
    All,
    BatchOnly
};

struct GeneratedOperation {
    enum class Type {
        Put,
        Get,
        Delete,
        Batch
    };

    Type type;
    std::string key;
    std::string value;
    std::vector<kvdb::Operation> batch;
};

class WorkloadGenerator {
public:
    WorkloadGenerator(std::uint64_t seed, std::uint32_t worker_id, WorkloadMode mode)
        : engine_(worker_seed(seed, worker_id)), worker_id_(worker_id), mode_(mode) {}

    GeneratedOperation next() {
        if (mode_ == WorkloadMode::BatchOnly) return make_batch();

        const unsigned int choice = std::uniform_int_distribution<unsigned int>(0, 99)(engine_);
        if (choice < 50) return make_put();
        if (choice < 80) return make_get();
        if (choice < 95) return make_delete();
        return make_batch();
    }

private:
    static std::uint64_t worker_seed(std::uint64_t seed, std::uint32_t worker_id) {
        std::uint64_t value = seed + 0x9e3779b97f4a7c15ULL * (static_cast<std::uint64_t>(worker_id) + 1);
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }

    std::string next_key() {
        const unsigned int key_id = std::uniform_int_distribution<unsigned int>(0, 63)(engine_);
        return "worker_" + std::to_string(worker_id_) + "_key_" + std::to_string(key_id);
    }

    std::string next_value() {
        const std::uint64_t value_id = engine_();
        return "worker_" + std::to_string(worker_id_) + "_value_" + std::to_string(value_id);
    }

    GeneratedOperation make_put() {
        return {GeneratedOperation::Type::Put, next_key(), next_value(), {}};
    }

    GeneratedOperation make_get() {
        return {GeneratedOperation::Type::Get, next_key(), {}, {}};
    }

    GeneratedOperation make_delete() {
        return {GeneratedOperation::Type::Delete, next_key(), {}, {}};
    }

    GeneratedOperation make_batch() {
        const std::size_t count = std::uniform_int_distribution<std::size_t>(1, 500)(engine_);
        std::vector<kvdb::Operation> operations;
        operations.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            const bool put = std::uniform_int_distribution<unsigned int>(0, 99)(engine_) < 75;
            if (put) {
                operations.push_back({kvdb::Operation::Type::Put, next_key(), next_value()});
            } else {
                operations.push_back({kvdb::Operation::Type::Delete, next_key(), {}});
            }
        }
        return {GeneratedOperation::Type::Batch, {}, {}, std::move(operations)};
    }

    std::mt19937_64 engine_;
    std::uint32_t worker_id_;
    WorkloadMode mode_;
};

} // namespace torture
