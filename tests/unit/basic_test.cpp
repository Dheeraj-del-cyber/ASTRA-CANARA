#include "kvdb/database.hpp"
#include <cassert>
#include <iostream>
#include <cstdio>

int main() {
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

