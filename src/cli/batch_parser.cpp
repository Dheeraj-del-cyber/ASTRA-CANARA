#include "kvdb/cli/batch_parser.hpp"
#include <fstream>
#include <sstream>

namespace kvdb::cli {

Status BatchParser::parse_line(const std::string& line, Operation& out_op) {
    if (line.empty() || line[0] == '#') {
        return Status::InvalidArgument; // Empty or comment line
    }

    std::istringstream iss(line);
    std::string op_type;
    if (!(iss >> op_type)) {
        return Status::InvalidArgument;
    }

    if (op_type == "PUT") {
        std::string key, value;
        if (!(iss >> key)) return Status::InvalidArgument;
        // Remaining part of line is value
        std::string remaining;
        std::getline(iss, remaining);
        // Trim leading space from value
        if (!remaining.empty() && remaining[0] == ' ') {
            remaining = remaining.substr(1);
        }
        out_op.type = Operation::Type::Put;
        out_op.key = key;
        out_op.value = remaining;
        return Status::Ok;
    } else if (op_type == "DEL") {
        std::string key;
        if (!(iss >> key)) return Status::InvalidArgument;
        out_op.type = Operation::Type::Delete;
        out_op.key = key;
        out_op.value = "";
        return Status::Ok;
    }

    return Status::InvalidArgument;
}

Status BatchParser::parse_file(const std::string& filepath, std::vector<Operation>& out_operations) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return Status::IoError;
    }

    out_operations.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        Operation op;
        Status s = parse_line(line, op);
        if (!is_ok(s)) {
            return s;
        }
        out_operations.push_back(std::move(op));
    }

    return Status::Ok;
}

} // namespace kvdb::cli

