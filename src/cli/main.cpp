#include "kvdb/database.hpp"
#include "kvdb/cli/batch_parser.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

void print_usage() {
    std::cout << "Usage:\n"
              << "  kvdb open <path> [--max-bytes N]\n"
              << "  kvdb put <path> <key> <value>\n"
              << "  kvdb get <path> <key>\n"
              << "  kvdb del <path> <key>\n"
              << "  kvdb batch <path> <batch_file>\n"
              << "  kvdb dump <path>\n"
              << "  kvdb check <path>\n"
              << "  kvdb serve <path>\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];
    std::string db_path = argv[2];

    kvdb::Database db;

    if (command == "open") {
        std::uint64_t max_bytes = 0;
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "--max-bytes" && i + 1 < argc) {
                max_bytes = std::stoull(argv[i + 1]);
                i++;
            }
        }
        kvdb::Status s = db.open(db_path, max_bytes);
        if (!kvdb::is_ok(s)) {
            std::cerr << "Error: " << kvdb::status_to_string(s) << "\n";
            return 1;
        }
        std::cout << "OK\n";
        return 0;
    }

    kvdb::Status s = db.open(db_path);
    if (!kvdb::is_ok(s)) {
        std::cerr << "Failed to open database: " << kvdb::status_to_string(s) << "\n";
        return 1;
    }

    if (command == "put") {
        if (argc < 5) {
            std::cerr << "Usage: kvdb put <path> <key> <value>\n";
            return 1;
        }
        std::string key = argv[3];
        std::string val = argv[4];
        s = db.put(key, val);
        if (!kvdb::is_ok(s)) {
            std::cerr << "Error: " << kvdb::status_to_string(s) << "\n";
            return 1;
        }
        std::cout << "OK\n";
        return 0;
    } else if (command == "get") {
        if (argc < 4) {
            std::cerr << "Usage: kvdb get <path> <key>\n";
            return 1;
        }
        std::string key = argv[3];
        auto res = db.get(key);
        if (res.status == kvdb::Status::NotFound) {
            std::cerr << "NOT_FOUND\n";
            return 2;
        } else if (!kvdb::is_ok(res.status)) {
            std::cerr << "Error: " << kvdb::status_to_string(res.status) << "\n";
            return 1;
        }
        std::cout << res.value << "\n";
        return 0;
    } else if (command == "del") {
        if (argc < 4) {
            std::cerr << "Usage: kvdb del <path> <key>\n";
            return 1;
        }
        std::string key = argv[3];
        s = db.remove(key);
        if (!kvdb::is_ok(s)) {
            std::cerr << "Error: " << kvdb::status_to_string(s) << "\n";
            return 1;
        }
        std::cout << "OK\n";
        return 0;
    } else if (command == "batch") {
        if (argc < 4) {
            std::cerr << "Usage: kvdb batch <path> <file>\n";
            return 1;
        }
        std::string batch_file = argv[3];
        std::vector<kvdb::Operation> ops;
        s = kvdb::cli::BatchParser::parse_file(batch_file, ops);
        if (!kvdb::is_ok(s)) {
            std::cerr << "Invalid batch file\n";
            return 1;
        }
        s = db.batch(ops);
        if (!kvdb::is_ok(s)) {
            std::cerr << "Batch error: " << kvdb::status_to_string(s) << "\n";
            return 1;
        }
        std::cout << "OK\n";
        return 0;
    } else if (command == "dump") {
        db.dump(std::cout);
        return 0;
    } else if (command == "check") {
        s = db.check();
        if (kvdb::is_ok(s)) {
            std::cout << "INTEGRITY_OK\n";
            return 0;
        } else {
            std::cerr << "INTEGRITY_FAILED: " << kvdb::status_to_string(s) << "\n";
            return 1;
        }
    } else if (command == "serve") {
        // Line-based server protocol: prints ACK <id> upon durable commit
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty() || line == "QUIT") break;
            // e.g. PUT <op_id> <key> <val>
            // or DEL <op_id> <key>
            // or GET <op_id> <key>
            std::istringstream iss(line);
            std::string op, op_id, key, val;
            if (!(iss >> op >> op_id >> key)) {
                std::cout << "ERR invalid_command\n";
                std::cout.flush();
                continue;
            }
            if (op == "PUT") {
                std::getline(iss, val);
                if (!val.empty() && val[0] == ' ') val = val.substr(1);
                s = db.put(key, val);
                if (kvdb::is_ok(s)) {
                    std::cout << "ACK " << op_id << "\n";
                } else {
                    std::cout << "ERR " << op_id << " " << kvdb::status_to_string(s) << "\n";
                }
            } else if (op == "DEL") {
                s = db.remove(key);
                if (kvdb::is_ok(s)) {
                    std::cout << "ACK " << op_id << "\n";
                } else {
                    std::cout << "ERR " << op_id << " " << kvdb::status_to_string(s) << "\n";
                }
            } else if (op == "GET") {
                auto res = db.get(key);
                if (kvdb::is_ok(res.status)) {
                    std::cout << "VALUE " << op_id << " " << res.value << "\n";
                } else {
                    std::cout << "NOT_FOUND " << op_id << "\n";
                }
            }
            std::cout.flush();
        }
        return 0;
    }

    std::cerr << "Unknown command: " << command << "\n";
    print_usage();
    return 1;
}
