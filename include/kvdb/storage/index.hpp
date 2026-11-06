#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <cstdint>
#include <vector>
#include "kvdb/types.hpp"

namespace kvdb::storage {

struct IndexEntry {
    std::uint64_t file_offset{0};
    std::uint64_t value_len{0};
    // Optional in-memory caching of value
    std::string cached_value{};
};

class Index {
public:
    Index() = default;

    void put(const std::string& key, const IndexEntry& entry);
    bool remove(const std::string& key);
    std::optional<IndexEntry> get(const std::string& key) const;
    bool contains(const std::string& key) const;
    std::size_t size() const;
    void clear();

    // Iterator / dump helper for compaction and dump
    std::vector<std::pair<std::string, IndexEntry>> get_all_entries() const;

private:
    std::unordered_map<std::string, IndexEntry> table_;
};

} // namespace kvdb::storage

