namespace kvdb::storage {

// Lightweight index record pointing to the physical location on disk (24 bytes)
struct IndexEntry {
    std::uint64_t val_file_offset{0}; // Absolute byte offset of the value payload in the .db file
    std::uint64_t val_len{0};         // Length of the value in bytes
    TxId          tx_id{0};           // Transaction ID that committed this version
};

class Index {
private:
    // Core hash table mapping live string keys to disk coordinates
    std::unordered_map<std::string, IndexEntry> table_;
};

} // namespace kvdb::storage