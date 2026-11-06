#pragma once

#include <cstdint>
#include <string>
#include "kvdb/status.hpp"
#include "kvdb/storage/file.hpp"
#include "kvdb/storage/index.hpp"

namespace kvdb::recovery {

struct RecoveryResult {
    Status status{Status::Ok};
    std::uint64_t valid_bytes{0};
    std::uint64_t recovered_keys{0};
    std::uint64_t committed_transactions{0};
    bool clean_shutdown{false};
};

class RecoveryEngine {
public:
    static RecoveryResult recover(storage::File& file, storage::Index& index);
};

} // namespace kvdb::recovery

