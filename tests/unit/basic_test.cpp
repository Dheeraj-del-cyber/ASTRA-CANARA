#include "kvdb/database.hpp"
#include "kvdb/cli/batch_parser.hpp"
#include "../torture/oracle.hpp"
#include <cassert>
#include <iostream>
#include <cstdio>
#include <fstream>
#include <iterator>

int main() {
    const std::string oracle_log = "oracle_unit_test.ack.log";
    std::remove(oracle_log.c_str());
    {
        AckOracle oracle(oracle_log);
        std::string error;
        assert(oracle.record_put("key with spaces", "value\nwith\ttabs\\", error));
        assert(oracle.record_put("deleted", "temporary", error));
        assert(oracle.record_del("deleted", error));
    }
    {
        std::ofstream partial_record(oracle_log, std::ios::binary | std::ios::app);
        partial_record << "P\tincomplete";
        assert(partial_record.good());
    }
    {
        AckOracle recovered_oracle(oracle_log);
        const auto recovered_value = recovered_oracle.get("key with spaces");
        assert(recovered_value.has_value());
        assert(*recovered_value == "value\nwith\ttabs\\");
        assert(!recovered_oracle.get("deleted").has_value());
    }
    {
        std::ifstream journal(oracle_log, std::ios::binary);
        const std::string contents((std::istreambuf_iterator<char>(journal)), std::istreambuf_iterator<char>());
        assert(!contents.empty() && contents.back() == '\n');
    }
    std::remove(oracle_log.c_str());

    // Test BatchParser escaping & unescaping
    {
        std::string raw = "hello\nworld\t\"test\"\\";
        std::string escaped = kvdb::cli::BatchParser::escape(raw);
        std::string unescaped = kvdb::cli::BatchParser::unescape(escaped);
        assert(raw == unescaped);

        kvdb::Operation op;
        bool is_comment = false;
        assert(kvdb::is_ok(kvdb::cli::BatchParser::parse_line("PUT \"key with spaces\" \"val with spaces\"", op, is_comment)));
        assert(!is_comment);
        assert(op.type == kvdb::Operation::Type::Put);
        assert(op.key == "key with spaces");
        assert(op.value == "val with spaces");

        assert(kvdb::is_ok(kvdb::cli::BatchParser::parse_line("DEL \"key with spaces\"", op, is_comment)));
        assert(op.type == kvdb::Operation::Type::Delete);
        assert(op.key == "key with spaces");

        // Invalid DEL with extra token
        assert(!kvdb::is_ok(kvdb::cli::BatchParser::parse_line("DEL key extra_arg", op, is_comment)));
    }

    std::string test_db = "unit_test.db";
    std::remove(test_db.c_str());

    kvdb::Database db;
    assert(kvdb::is_ok(db.open(test_db)));

    assert(kvdb::is_ok(db.put("hello", "world")));
    auto res = db.get("hello");
    assert(kvdb::is_ok(res.status));
    assert(res.value == "world");

    assert(kvdb::is_ok(db.remove("hello")));
    res = db.get("hello");
    assert(res.status == kvdb::Status::NotFound);

    // Test empty value
    assert(kvdb::is_ok(db.put("empty_key", "")));
    res = db.get("empty_key");
    assert(kvdb::is_ok(res.status));
    assert(res.value.empty());

    // Test delete of non-existent key (idempotent success)
    assert(kvdb::is_ok(db.remove("does_not_exist")));

    // Test batch
    std::vector<kvdb::Operation> batch_ops = {
        {kvdb::Operation::Type::Put, "k1", "v1"},
        {kvdb::Operation::Type::Put, "k2", "v2"},
        {kvdb::Operation::Type::Put, "k3", "v3"}
    };
    assert(kvdb::is_ok(db.batch(batch_ops)));
    assert(db.get("k1").value == "v1");
    assert(db.get("k2").value == "v2");
    assert(db.get("k3").value == "v3");

    // Test compact
    assert(kvdb::is_ok(db.compact()));
    assert(db.get("k1").value == "v1");

    assert(kvdb::is_ok(db.close()));

    // Reopen
    assert(kvdb::is_ok(db.open(test_db)));
    assert(db.get("k1").value == "v1");
    assert(kvdb::is_ok(db.close()));

    std::remove(test_db.c_str());
    std::cout << "[Unit Test] All basic unit tests passed!\n";
    return 0;
}
