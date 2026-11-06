#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "kvdb/status.hpp"

namespace kvdb::storage {

class File {
public:
    File();
    ~File();

    File(const File&) = delete;
    File& operator=(const File&) = delete;
    File(File&&) noexcept;
    File& operator=(File&&) noexcept;

    Status open(const std::string& path, bool create_if_missing = true);
    Status close();

    Status append(const std::uint8_t* data, std::size_t size);
    Status read_at(std::uint64_t offset, std::uint8_t* buffer, std::size_t size);
    Status sync(); // Durable OS sync (fsync / FlushFileBuffers)
    Status truncate(std::uint64_t size);

    std::uint64_t size() const;
    bool is_open() const;
    const std::string& path() const;

private:
    struct Impl;
    Impl* impl_{nullptr};
};

// Platform-aware atomic file rename & directory sync
Status atomic_rename_file(const std::string& old_path, const std::string& new_path);

} // namespace kvdb::storage

