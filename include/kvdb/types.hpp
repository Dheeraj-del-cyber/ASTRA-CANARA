#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "kvdb/status.hpp"

namespace kvdb {

// Type aliases
using TxId = std::uint64_t;
using FileOffset = std::uint64_t;

// Binary layout constants (agreed little-endian)
inline constexpr std::uint64_t FILE_MAGIC = 0x313030304244564BULL; // "KVDB0001" in ASCII (LE)
inline constexpr std::uint32_t FILE_FORMAT_VERSION = 1;
inline constexpr std::uint32_t FILE_HEADER_LENGTH = 32;

inline constexpr std::uint32_t FRAME_MAGIC = 0x464D564B;            // "KVMF" (KVDB Frame Magic)
inline constexpr std::uint16_t FRAME_FORMAT_VERSION = 1;

// Configurable and architectural bounds
inline constexpr std::uint32_t MAX_KEY_SIZE = 64 * 1024;           // 64 KB
inline constexpr std::uint64_t MAX_VALUE_SIZE = 16 * 1024 * 1024;   // 16 MB
inline constexpr std::size_t MAX_BATCH_OPERATIONS = 10000;         // 10,000 ops/batch

// Record types within transaction frames
enum class RecordType : std::uint8_t {
    Begin  = 1,
    Put    = 2,
    Delete = 3,
    Commit = 4
};

// Operation representation for batch / individual mutations
struct Operation {
    enum class Type { Put, Delete };
    Type type;
    std::string key;
    std::string value; // Ignored for Delete
};

// Result returned by Get operations
struct Result {
    Status status{Status::NotFound};
    std::string value{};
};

// Database configuration options
struct Config {
    std::uint64_t max_bytes = 0; // 0 = unlimited / disk-governed
    bool read_only = false;
};

} // namespace kvdb

