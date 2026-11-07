#include "kvdb/recovery/log_scanner.hpp"
#include "kvdb/storage/file.hpp"
#include "kvdb/storage/format.hpp"
#include <cassert>
#include <iostream>
#include <cstdio>
#include <vector>

using namespace kvdb;

void test_empty_log() {
    std::string test_path = "test_empty_scan.db";
    std::remove(test_path.c_str());

    storage::File file;
    assert(is_ok(file.open(test_path, true)));

    auto hdr = storage::serialize_file_header();
    assert(is_ok(file.append(hdr.data(), hdr.size())));
    assert(is_ok(file.sync()));

    recovery::LogScanner scanner(file);
    recovery::ScannedRecord rec;
    assert(scanner.next_record(rec) == recovery::ScanStatus::EndOfFile);

    file.close();
    std::remove(test_path.c_str());
    std::cout << "[Test 1 Passed] Empty log returns EndOfFile cleanly.\n";
}

void test_valid_frames_and_offsets() {
    std::string test_path = "test_valid_scan.db";
    std::remove(test_path.c_str());

    storage::File file;
    assert(is_ok(file.open(test_path, true)));

    auto hdr = storage::serialize_file_header();
    assert(is_ok(file.append(hdr.data(), hdr.size())));

    storage::Frame f1{RecordType::Begin, 1, "", ""};
    storage::Frame f2{RecordType::Put, 1, "k1", "v1"};
    storage::Frame f3{RecordType::Commit, 1, "", ""};

    auto b1 = storage::serialize_frame(f1);
    auto b2 = storage::serialize_frame(f2);
    auto b3 = storage::serialize_frame(f3);

    assert(is_ok(file.append(b1.data(), b1.size())));
    assert(is_ok(file.append(b2.data(), b2.size())));
    assert(is_ok(file.append(b3.data(), b3.size())));
    assert(is_ok(file.sync()));

    // Test with small buffer capacity (64 bytes) to test multi-chunk buffer refills
    recovery::LogScanner scanner(file, FILE_HEADER_LENGTH, 64);
    recovery::ScannedRecord rec;

    assert(scanner.next_record(rec) == recovery::ScanStatus::Ok);
    assert(rec.frame.type == RecordType::Begin);
    assert(rec.frame.tx_id == 1);
    assert(rec.offset == 32);

    assert(scanner.next_record(rec) == recovery::ScanStatus::Ok);
    assert(rec.frame.type == RecordType::Put);
    assert(rec.frame.key == "k1");
    assert(rec.frame.value == "v1");
    assert(rec.offset == 32 + b1.size());

    assert(scanner.next_record(rec) == recovery::ScanStatus::Ok);
    assert(rec.frame.type == RecordType::Commit);
    assert(rec.offset == 32 + b1.size() + b2.size());

    assert(scanner.next_record(rec) == recovery::ScanStatus::EndOfFile);

    file.close();
    std::remove(test_path.c_str());
    std::cout << "[Test 2 Passed] Valid sequential frames and offsets verified across buffer boundaries.\n";
}

void test_corrupted_crc() {
    std::string test_path = "test_corrupt_crc.db";
    std::remove(test_path.c_str());

    storage::File file;
    assert(is_ok(file.open(test_path, true)));

    auto hdr = storage::serialize_file_header();
    assert(is_ok(file.append(hdr.data(), hdr.size())));

    storage::Frame f{RecordType::Put, 1, "k1", "v1"};
    auto b = storage::serialize_frame(f);

    // Corrupt one byte of the payload
    b[sizeof(storage::RawFrameHeader)] ^= 0xFF;

    assert(is_ok(file.append(b.data(), b.size())));
    assert(is_ok(file.sync()));

    recovery::LogScanner scanner(file);
    recovery::ScannedRecord rec;
    assert(scanner.next_record(rec) == recovery::ScanStatus::CorruptedRecord);

    file.close();
    std::remove(test_path.c_str());
    std::cout << "[Test 3 Passed] Corrupted payload CRC32 correctly detected.\n";
}

void test_incomplete_tail() {
    std::string test_path = "test_incomplete_tail.db";
    std::remove(test_path.c_str());

    storage::File file;
    assert(is_ok(file.open(test_path, true)));

    auto hdr = storage::serialize_file_header();
    assert(is_ok(file.append(hdr.data(), hdr.size())));

    storage::Frame f{RecordType::Put, 1, "k1", "v1"};
    auto b = storage::serialize_frame(f);

    // Write only half of the frame (simulating a crash mid-write)
    assert(is_ok(file.append(b.data(), b.size() / 2)));
    assert(is_ok(file.sync()));

    recovery::LogScanner scanner(file);
    recovery::ScannedRecord rec;
    assert(scanner.next_record(rec) == recovery::ScanStatus::IncompleteTail);

    file.close();
    std::remove(test_path.c_str());
    std::cout << "[Test 4 Passed] Incomplete/torn trailing frame detected as IncompleteTail.\n";
}

int main() {
    std::cout << "--- Running LogScanner Unit Tests ---\n";
    test_empty_log();
    test_valid_frames_and_offsets();
    test_corrupted_crc();
    test_incomplete_tail();
    std::cout << "--- All LogScanner Unit Tests Passed! ---\n";
    return 0;
}

