#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "kvdb/types.hpp"
#include "kvdb/status.hpp"

namespace kvdb::storage {

#pragma pack(push, 1)
struct RawFileHeader {
    std::uint64_t magic;          // 8 bytes: "KVDB0001"
    std::uint32_t format_version; // 4 bytes: 1
    std::uint32_t header_length;  // 4 bytes: 32
    std::uint8_t  reserved[16];   // 16 bytes: zeros
};
static_assert(sizeof(RawFileHeader) == 32, "RawFileHeader must be exactly 32 bytes");

struct RawFrameHeader {
    std::uint32_t frame_magic;    // 4 bytes: "KVMF"
    std::uint16_t format_version; // 2 bytes: 1
    std::uint8_t  record_type;    // 1 byte: BEGIN, PUT, DELETE, COMMIT
    std::uint8_t  flags;          // 1 byte: 0
    std::uint64_t tx_id;          // 8 bytes
    std::uint32_t key_len;        // 4 bytes
    std::uint64_t val_len;        // 8 bytes
};
static_assert(sizeof(RawFrameHeader) == 28, "RawFrameHeader must be exactly 28 bytes");
#pragma pack(pop)

struct Frame {
    RecordType type;
    TxId tx_id;
    std::string key;
    std::string value;
};

// Checksum utilities
std::uint32_t calculate_crc32(const std::uint8_t* data, std::size_t size);

// Serialization & Deserialization
std::vector<std::uint8_t> serialize_file_header();
Status deserialize_file_header(const std::uint8_t* buffer, std::size_t size);

std::vector<std::uint8_t> serialize_frame(const Frame& frame);
Status deserialize_frame(const std::uint8_t* buffer, std::size_t size, Frame& out_frame, std::size_t& bytes_consumed);

} // namespace kvdb::storage

