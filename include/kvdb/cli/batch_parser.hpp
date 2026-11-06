#pragma once

#include <string>
#include <vector>
#include "kvdb/types.hpp"
#include "kvdb/status.hpp"

namespace kvdb::cli {

class BatchParser {
public:
    static Status parse_file(const std::string& filepath, std::vector<Operation>& out_operations);
    static Status parse_line(const std::string& line, Operation& out_op);
};

} // namespace kvdb::cli

