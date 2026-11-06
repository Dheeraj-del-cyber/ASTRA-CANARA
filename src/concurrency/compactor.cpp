#include "kvdb/concurrency/compactor.hpp"
#include "kvdb/storage/format.hpp"
#include <vector>

namespace kvdb::concurrency {

bool SpaceManager::has_space_for(std::uint64_t current_file_size, std::uint64_t additional_bytes, std::uint64_t max_bytes) {
    if (max_bytes == 0) return true; // unlimited
    return (current_file_size + additional_bytes) <= max_bytes;
}

Status Compactor::compact(
    const std::string& db_path,
    storage::File& file,
    storage::Index& index,
    LockManager& lock_mgr,
    std::uint64_t max_bytes
) {
    (void)max_bytes;
    // 1. Lock exclusively
    std::unique_lock<std::shared_mutex> lock(lock_mgr.mutex());

    std::string tmp_path = db_path + ".compact.tmp";

    // 2. Create and write temporary database file
    storage::File tmp_file;
    Status s = tmp_file.open(tmp_path, true);
    if (!is_ok(s)) return s;

    auto hdr = storage::serialize_file_header();
    s = tmp_file.append(hdr.data(), hdr.size());
    if (!is_ok(s)) return s;

    // 3. Dump all live entries as a single committed transaction
    auto entries = index.get_all_entries();
    if (!entries.empty()) {
        TxId tx_id = 1;
        storage::Frame begin_frame{RecordType::Begin, tx_id, "", ""};
        auto b_bytes = storage::serialize_frame(begin_frame);
        s = tmp_file.append(b_bytes.data(), b_bytes.size());
        if (!is_ok(s)) return s;

        for (const auto& [k, v] : entries) {
            storage::Frame put_frame{RecordType::Put, tx_id, k, v.cached_value};
            auto p_bytes = storage::serialize_frame(put_frame);
            s = tmp_file.append(p_bytes.data(), p_bytes.size());
            if (!is_ok(s)) return s;
        }

        storage::Frame commit_frame{RecordType::Commit, tx_id, "", ""};
        auto c_bytes = storage::serialize_frame(commit_frame);
        s = tmp_file.append(c_bytes.data(), c_bytes.size());
        if (!is_ok(s)) return s;
    }

    // 4. Flush and sync tmp file
    s = tmp_file.sync();
    if (!is_ok(s)) return s;
    tmp_file.close();

    // 5. Close existing database file before atomic rename
    file.close();

    // 6. Atomic replacement
    s = storage::atomic_rename_file(tmp_path, db_path);
    if (!is_ok(s)) return s;

    // 7. Reopen primary file
    s = file.open(db_path, false);
    return s;
}

} // namespace kvdb::concurrency
