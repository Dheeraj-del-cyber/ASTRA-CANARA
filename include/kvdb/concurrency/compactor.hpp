#pragma once

#include <string>
#include <cstdint>
#include "kvdb/status.hpp"
#include "kvdb/storage/file.hpp"
#include "kvdb/storage/index.hpp"
#include "kvdb/concurrency/lock_manager.hpp"

namespace kvdb::concurrency {

class Compactor {
public:
    static Status compact(
        const std::string& db_path,
        storage::File& file,
        storage::Index& index,
        LockManager& lock_mgr,
        std::uint64_t max_bytes
    );
};

class SpaceManager {
public:
    static bool has_space_for(std::uint64_t current_file_size, std::uint64_t additional_bytes, std::uint64_t max_bytes);
};

} // namespace kvdb::concurrency

