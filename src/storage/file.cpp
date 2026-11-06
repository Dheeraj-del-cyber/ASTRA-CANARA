#include "kvdb/storage/file.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

namespace kvdb::storage {

struct File::Impl {
    std::string path;
    std::FILE* handle{nullptr};
    std::uint64_t file_size{0};
};

File::File() : impl_(new Impl()) {}

File::~File() {
    close();
    delete impl_;
}

File::File(File&& other) noexcept : impl_(other.impl_) {
    other.impl_ = new Impl();
}

File& File::operator=(File&& other) noexcept {
    if (this != &other) {
        close();
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = new Impl();
    }
    return *this;
}

Status File::open(const std::string& path, bool create_if_missing) {
    close();
    impl_->path = path;

    std::FILE* f = std::fopen(path.c_str(), "r+b");
    if (!f && create_if_missing) {
        f = std::fopen(path.c_str(), "w+b");
    }
    if (!f) {
        return Status::IoError;
    }

    impl_->handle = f;
    std::fseek(f, 0, SEEK_END);
    impl_->file_size = static_cast<std::uint64_t>(std::ftell(f));
    return Status::Ok;
}

Status File::close() {
    if (impl_->handle) {
        std::fclose(impl_->handle);
        impl_->handle = nullptr;
        impl_->file_size = 0;
    }
    return Status::Ok;
}

Status File::append(const std::uint8_t* data, std::size_t size) {
    if (!impl_->handle) return Status::NotOpen;
    std::fseek(impl_->handle, 0, SEEK_END);
    std::size_t written = std::fwrite(data, 1, size, impl_->handle);
    if (written != size) {
        return Status::IoError;
    }
    impl_->file_size += size;
    return Status::Ok;
}

Status File::read_at(std::uint64_t offset, std::uint8_t* buffer, std::size_t size) {
    if (!impl_->handle) return Status::NotOpen;
    if (std::fseek(impl_->handle, static_cast<long>(offset), SEEK_SET) != 0) {
        return Status::IoError;
    }
    std::size_t read_bytes = std::fread(buffer, 1, size, impl_->handle);
    if (read_bytes != size) {
        return Status::IoError;
    }
    return Status::Ok;
}

Status File::sync() {
    if (!impl_->handle) return Status::NotOpen;
    std::fflush(impl_->handle);

#if defined(_WIN32)
    int fd = _fileno(impl_->handle);
    HANDLE hFile = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    if (hFile == INVALID_HANDLE_VALUE || !FlushFileBuffers(hFile)) {
        return Status::IoError;
    }
#else
    int fd = fileno(impl_->handle);
    if (fsync(fd) != 0) {
        return Status::IoError;
    }
#endif
    return Status::Ok;
}

Status File::truncate(std::uint64_t new_size) {
    if (!impl_->handle) return Status::NotOpen;
    std::fflush(impl_->handle);
#if defined(_WIN32)
    int fd = _fileno(impl_->handle);
    if (_chsize_s(fd, static_cast<__int64>(new_size)) != 0) {
        return Status::IoError;
    }
#else
    int fd = fileno(impl_->handle);
    if (ftruncate(fd, static_cast<off_t>(new_size)) != 0) {
        return Status::IoError;
    }
#endif
    impl_->file_size = new_size;
    return Status::Ok;
}

std::uint64_t File::size() const {
    return impl_->file_size;
}

bool File::is_open() const {
    return impl_->handle != nullptr;
}

const std::string& File::path() const {
    return impl_->path;
}

Status atomic_rename_file(const std::string& old_path, const std::string& new_path) {
    std::error_code ec;
    std::filesystem::rename(old_path, new_path, ec);
    if (ec) {
        return Status::IoError;
    }
    return Status::Ok;
}

} // namespace kvdb::storage

