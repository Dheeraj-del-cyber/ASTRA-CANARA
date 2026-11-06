#include "kvdb/recovery/recovery.hpp"
#include "kvdb/storage/format.hpp"
#include <vector>

namespace kvdb::recovery {

RecoveryResult RecoveryEngine::recover(storage::File& file, storage::Index& index) {
    RecoveryResult res;
    index.clear();

    std::uint64_t file_len = file.size();
    if (file_len == 0) {
        // Brand new empty file, initialize with header
        auto hdr = storage::serialize_file_header();
        Status s = file.append(hdr.data(), hdr.size());
        if (!is_ok(s)) {
            res.status = s;
            return res;
        }
        file.sync();
        res.valid_bytes = hdr.size();
        res.status = Status::Ok;
        return res;
    }

    if (file_len < sizeof(storage::RawFileHeader)) {
        res.status = Status::Corruption;
        return res;
    }

    // Read and validate file header
    std::vector<std::uint8_t> hdr_buf(sizeof(storage::RawFileHeader));
    Status s = file.read_at(0, hdr_buf.data(), hdr_buf.size());
    if (!is_ok(s)) {
        res.status = s;
        return res;
    }

    s = storage::deserialize_file_header(hdr_buf.data(), hdr_buf.size());
    if (!is_ok(s)) {
        res.status = Status::Corruption;
        return res;
    }

    std::uint64_t cursor = sizeof(storage::RawFileHeader);
    std::uint64_t last_valid_sync_point = cursor;

    // Scan frames sequentially
    std::vector<std::uint8_t> read_buffer;
    std::vector<storage::Frame> pending_tx_frames;
    TxId current_tx_id = 0;
    bool in_transaction = false;

    while (cursor < file_len) {
        std::size_t bytes_to_read = static_cast<std::size_t>(std::min<std::uint64_t>(file_len - cursor, 64 * 1024 * 1024));
        // Simple scan: read remaining file into memory for recovery
        read_buffer.resize(bytes_to_read);
        s = file.read_at(cursor, read_buffer.data(), bytes_to_read);
        if (!is_ok(s)) {
            res.status = s;
            return res;
        }

        std::size_t buffer_offset = 0;
        while (buffer_offset < bytes_to_read) {
            storage::Frame frame;
            std::size_t consumed = 0;
            s = storage::deserialize_frame(read_buffer.data() + buffer_offset, bytes_to_read - buffer_offset, frame, consumed);
            if (!is_ok(s)) {
                // Encountered incomplete or corrupted frame
                // If at the end of the file, this is an uncommitted/torn tail
                break;
            }

            buffer_offset += consumed;
            cursor += consumed;

            if (frame.type == RecordType::Begin) {
                pending_tx_frames.clear();
                current_tx_id = frame.tx_id;
                in_transaction = true;
            } else if (in_transaction && frame.tx_id == current_tx_id) {
                if (frame.type == RecordType::Commit) {
                    // Transaction is committed: apply pending frames to in-memory index
                    for (const auto& op : pending_tx_frames) {
                        if (op.type == RecordType::Put) {
                            storage::IndexEntry entry;
                            entry.cached_value = op.value;
                            entry.value_len = op.value.size();
                            index.put(op.key, entry);
                        } else if (op.type == RecordType::Delete) {
                            index.remove(op.key);
                        }
                    }
                    pending_tx_frames.clear();
                    in_transaction = false;
                    last_valid_sync_point = cursor;
                    res.committed_transactions++;
                } else {
                    pending_tx_frames.push_back(frame);
                }
            }
        }
        break; // Scan completed
    }

    // Truncate uncommitted trailing tail if needed
    if (last_valid_sync_point < file_len) {
        file.truncate(last_valid_sync_point);
        file.sync();
    }

    res.valid_bytes = last_valid_sync_point;
    res.recovered_keys = index.size();
    res.status = Status::Ok;
    return res;
}

} // namespace kvdb::recovery

