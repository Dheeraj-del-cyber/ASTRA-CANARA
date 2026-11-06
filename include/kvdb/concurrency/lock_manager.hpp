#pragma once

#include <shared_mutex>
#include <mutex>

namespace kvdb::concurrency {

class LockManager {
public:
    LockManager() = default;

    // Shared read lock for GET
    void lock_shared() { mutex_.lock_shared(); }
    void unlock_shared() { mutex_.unlock_shared(); }

    // Exclusive write lock for PUT, DELETE, BATCH, COMPACT, CLOSE
    void lock_exclusive() { mutex_.lock(); }
    void unlock_exclusive() { mutex_.unlock(); }

    std::shared_mutex& mutex() { return mutex_; }

private:
    std::shared_mutex mutex_;
};

} // namespace kvdb::concurrency

