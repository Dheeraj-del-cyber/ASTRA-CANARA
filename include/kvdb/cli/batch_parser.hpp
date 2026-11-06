#pragma once

#include <string>
#include <vector>
#include "kvdb/types.hpp"
#include "kvdb/status.hpp"

namespace kvdb::cli {

class BatchParser {
public:
    // Parses a batch file from disk, strictly validating all operations
    static Status parse_file(const std::string& filepath, std::vector<Operation>& out_operations);

    // Parses a single line. Sets is_empty_or_comment to true for comments and blank lines.
    static Status parse_line(const std::string& line, Operation& out_op, bool& is_empty_or_comment);

    // Utility: unescapes C-style escape sequences (\n, \r, \t, \\, \", \ )
    static std::string unescape(const std::string& input);

    // Utility: escapes binary / whitespace strings for serialization
    static std::string escape(const std::string& input);
};

} // namespace kvdb::cli
