#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <iosfwd>
#include "kvdb/status.hpp"
#include "kvdb/types.hpp"

namespace kvdb {

// Forward declaration of internal implementation
class DatabaseImpl;

class Database {
public:
    Database();
    ~Database();

    // Move-only semantics (no copying a database handle)
    Database(Database&&) noexcept;
    Database& operator=(Database&&) noexcept;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Public API as defined in CONTRACT.md
    Status open(const std::string& path, std::uint64_t max_bytes = 0);
    Status put(const std::string& key, const std::string& value);
    Result get(const std::string& key);
    Status remove(const std::string& key);
    Status batch(const std::vector<Operation>& operations);
    Status compact();
    Status close();

    // Maintenance & inspection tools
    Status dump(std::ostream& out);
    Status check();
    static Status check_file(const std::string& path);

    // Query state
    bool is_open() const;

private:
    std::unique_ptr<DatabaseImpl> impl_;
};

} // namespace kvdb

