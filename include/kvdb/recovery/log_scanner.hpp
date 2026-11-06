#pragma once

#include <cstdint>
#include <string>
#include "kvdb/status.hpp"
#include "kvdb/types.hpp"
#include "kvdb/storage/file.hpp"
#include "kvdb/storage/format.hpp"

namespace kvdb::recovery {

// Detailed outcome for each scan attempt
enum class ScanStatus {
    Ok,               // Valid frame successfully parsed and verified
    EndOfFile,        // Reached end of file cleanly at a valid frame boundary
    IncompleteTail,   // Trailing partial/torn frame caused by an abrupt crash
    CorruptedRecord,  // CRC32 mismatch, invalid magic, or out-of-bound lengths
    IoError           // File I/O failure while reading from disk
};

// Represents a verified record yielded by the scanner
struct ScannedRecord {
    storage::Frame frame;
    std::uint64_t offset{0}; // Starting byte offset in the file
    std::size_t length{0};   // Total size in bytes (header + payload + CRC32)
};

class LogScanner {
public:
    // Scans sequentially starting at start_offset (default 32, right after the file header)
    explicit LogScanner(storage::File& file, std::uint64_t start_offset = FILE_HEADER_LENGTH, std::size_t buffer_capacity = 64 * 1024);

    // Yields the next valid frame or reports an error / EOF condition
    ScanStatus next_record(ScannedRecord& record);

    // Current byte offset in the file being examined
    std::uint64_t current_offset() const { return cursor_; }

private:
    Status refill_buffer();

    storage::File& file_;
    std::uint64_t cursor_;
    std::size_t buffer_capacity_;
    std::vector<std::uint8_t> buffer_;
    std::size_t buffer_offset_{0};
    std::size_t buffer_valid_bytes_{0};
};

} // namespace kvdb::recovery

