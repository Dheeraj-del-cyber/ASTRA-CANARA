#include "kvdb/recovery/recovery.hpp"
#include "kvdb/storage/format.hpp"
#include "kvdb/storage/file.hpp"
#include "kvdb/storage/index.hpp"
#include "../../src/storage/file_test_hooks.hpp"
#include <cassert>
#include <iostream>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace kvdb;
using namespace kvdb::storage;

void append_frame(File& file, RecordType type, TxId tx_id,
                  const std::string& key = {}, const std::string& value = {}) {
    const auto bytes = serialize_frame(Frame{type, tx_id, key, value});
    assert(is_ok(file.append(bytes.data(), bytes.size())));
}

void append_transaction(File& file, TxId tx_id,
                        const std::vector<Frame>& operations) {
    append_frame(file, RecordType::Begin, tx_id);
    for (const auto& operation : operations) {
        append_frame(file, operation.type, tx_id, operation.key, operation.value);
    }
    append_frame(file, RecordType::Commit, tx_id);
}

void append_header(File& file) {
    const auto header = serialize_file_header();
    assert(is_ok(file.append(header.data(), header.size())));
}

void expect_value(File& file, const Index& index, const std::string& key,
                  const std::string& expected) {
    const auto entry = index.get(key);
    assert(entry.has_value());
    std::string value(static_cast<std::size_t>(entry->val_len), '\0');
    if (!value.empty()) {
        assert(is_ok(file.read_at(entry->val_file_offset,
            reinterpret_cast<std::uint8_t*>(&value[0]), value.size())));
    }
    assert(value == expected);
}

void test_replays_only_committed_transactions() {
    const std::string path = "recovery_committed_test.db";
    std::remove(path.c_str());

    File file;
    assert(is_ok(file.open(path, true)));
    append_header(file);
    append_transaction(file, 1, {{RecordType::Put, 1, "replace", "old"}});
    append_transaction(file, 2, {
        {RecordType::Put, 2, "batch-a", "A"},
        {RecordType::Delete, 2, "replace", ""},
        {RecordType::Put, 2, "replace", "new"}
    });
    append_transaction(file, 3, {{RecordType::Delete, 3, "batch-a", ""}});
    assert(is_ok(file.sync()));

    Index index;
    index.put("stale", IndexEntry{0, 0, 0});
    const auto stats = recovery::RecoveryEngine::recover(file, index);
    assert(is_ok(stats.status));
    assert(stats.committed_transactions == 3);
    assert(stats.clean_shutdown);
    assert(!index.contains("stale"));
    assert(!index.contains("batch-a"));
    assert(index.contains("replace"));
    expect_value(file, index, "replace", "new");

    assert(is_ok(file.close()));
    std::remove(path.c_str());
}

void test_truncates_uncommitted_and_torn_tail() {
    const std::string path = "recovery_uncommitted_test.db";
    std::remove(path.c_str());

    File file;
    assert(is_ok(file.open(path, true)));
    append_header(file);
    append_transaction(file, 1, {{RecordType::Put, 1, "committed", "yes"}});
    const std::uint64_t committed_size = file.size();

    append_frame(file, RecordType::Begin, 2);
    append_frame(file, RecordType::Put, 2, "uncommitted", "no");
    const auto torn_frame = serialize_frame(Frame{RecordType::Commit, 2, "", ""});
    assert(is_ok(file.append(torn_frame.data(), torn_frame.size() / 2)));
    assert(is_ok(file.sync()));
    const std::uint64_t original_size = file.size();

    Index index;
    const auto stats = recovery::RecoveryEngine::recover(file, index);
    assert(is_ok(stats.status));
    assert(stats.committed_transactions == 1);
    assert(stats.uncommitted_transactions == 1);
    assert(stats.truncated_bytes == original_size - committed_size);
    assert(file.size() == committed_size);
    assert(index.contains("committed"));
    assert(!index.contains("uncommitted"));

    assert(is_ok(file.close()));
    std::remove(path.c_str());
}

void test_truncates_corrupt_tail_but_rejects_mid_log_corruption() {
    const std::string tail_path = "recovery_corrupt_tail_test.db";
    std::remove(tail_path.c_str());
    {
        File file;
        assert(is_ok(file.open(tail_path, true)));
        append_header(file);
        append_transaction(file, 1, {{RecordType::Put, 1, "kept", "value"}});
        const std::uint64_t committed_size = file.size();
        auto corrupted = serialize_frame(Frame{RecordType::Put, 2, "tail", "bad"});
        corrupted.back() ^= 0x7F;
        assert(is_ok(file.append(corrupted.data(), corrupted.size())));

        Index index;
        const auto stats = recovery::RecoveryEngine::recover(file, index);
        assert(is_ok(stats.status));
        assert(file.size() == committed_size);
        assert(index.contains("kept"));
        assert(!index.contains("tail"));
        assert(is_ok(file.close()));
    }
    std::remove(tail_path.c_str());

    const std::string middle_path = "recovery_corrupt_middle_test.db";
    std::remove(middle_path.c_str());
    {
        File file;
        assert(is_ok(file.open(middle_path, true)));
        append_header(file);
        append_transaction(file, 1, {{RecordType::Put, 1, "before", "ok"}});
        auto corrupted = serialize_frame(Frame{RecordType::Put, 2, "broken", "x"});
        corrupted.back() ^= 0x01;
        assert(is_ok(file.append(corrupted.data(), corrupted.size())));
        append_transaction(file, 3, {{RecordType::Put, 3, "after", "must-not-replay"}});
        const std::uint64_t original_size = file.size();

        Index index;
        const auto stats = recovery::RecoveryEngine::recover(file, index);
        assert(stats.status == Status::Corruption);
        assert(file.size() == original_size);
        assert(index.contains("before"));
        assert(!index.contains("broken"));
        assert(!index.contains("after"));
        assert(is_ok(file.close()));
    }
    std::remove(middle_path.c_str());
}

void test_reports_truncation_sync_failure() {
    const std::string path = "recovery_sync_failure_test.db";
    std::remove(path.c_str());

    File file;
    assert(is_ok(file.open(path, true)));
    append_header(file);
    append_transaction(file, 1, {{RecordType::Put, 1, "committed", "yes"}});
    const std::uint64_t committed_size = file.size();
    append_frame(file, RecordType::Begin, 2);

    Index index;
    testing::fail_next_file_operation(testing::FileFailurePoint::Sync);
    const auto stats = recovery::RecoveryEngine::recover(file, index);
    testing::clear_file_failure();
    assert(stats.status == Status::IoError);
    assert(file.size() == committed_size);
    assert(index.contains("committed"));

    const auto retry_stats = recovery::RecoveryEngine::recover(file, index);
    assert(is_ok(retry_stats.status));
    assert(index.contains("committed"));
    assert(!index.contains("uncommitted"));

    assert(is_ok(file.close()));
    std::remove(path.c_str());
}

void test_reports_empty_header_sync_failure() {
    const std::string path = "recovery_header_sync_failure_test.db";
    std::remove(path.c_str());

    File file;
    assert(is_ok(file.open(path, true)));
    Index index;
    testing::fail_next_file_operation(testing::FileFailurePoint::Sync);
    const auto stats = recovery::RecoveryEngine::recover(file, index);
    testing::clear_file_failure();
    assert(stats.status == Status::IoError);

    assert(is_ok(file.close()));
    std::remove(path.c_str());
}

} // namespace

int main() {
    test_replays_only_committed_transactions();
    test_truncates_uncommitted_and_torn_tail();
    test_truncates_corrupt_tail_but_rejects_mid_log_corruption();
    test_reports_truncation_sync_failure();
    test_reports_empty_header_sync_failure();
    std::cout << "[Recovery Test] Replay, tail handling, and I/O failure tests passed.\n";
    return 0;
}

