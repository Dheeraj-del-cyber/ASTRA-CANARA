#include "kvdb/recovery/log_scanner.hpp"
#include <algorithm>
#include <cstring>

namespace kvdb::recovery {

LogScanner::LogScanner(storage::File& file, std::uint64_t start_offset, std::size_t buffer_capacity)
    : file_(file),
      cursor_(start_offset),
      buffer_capacity_(std::max<std::size_t>(buffer_capacity, 4096)),
      buffer_(buffer_capacity_) {}

Status LogScanner::refill_buffer() {
    std::uint64_t file_len = file_.size();
    if (cursor_ >= file_len) {
        return Status::Ok; // Nothing more to read
    }

    std::uint64_t remaining_in_file = file_len - cursor_;
    std::size_t to_read = static_cast<std::size_t>(std::min<std::uint64_t>(remaining_in_file, buffer_.size() - buffer_valid_bytes_));

    if (to_read == 0) {
        return Status::Ok;
    }

    Status s = file_.read_at(cursor_, buffer_.data() + buffer_valid_bytes_, to_read);
    if (!is_ok(s)) {
        return s;
    }

    buffer_valid_bytes_ += to_read;
    cursor_ += to_read;
    return Status::Ok;
}

ScanStatus LogScanner::next_record(ScannedRecord& record) {
    std::uint64_t file_len = file_.size();

    while (true) {
        std::size_t available = buffer_valid_bytes_ - buffer_offset_;

        // If we have fewer bytes than minimum frame header + CRC, try refilling
        if (available < sizeof(storage::RawFrameHeader) + sizeof(std::uint32_t)) {
            // Shift remaining bytes to front of buffer
            if (available > 0 && buffer_offset_ > 0) {
                std::memmove(buffer_.data(), buffer_.data() + buffer_offset_, available);
            }
            buffer_valid_bytes_ = available;
            buffer_offset_ = 0;

            Status s = refill_buffer();
            if (!is_ok(s)) {
                return ScanStatus::IoError;
            }

            available = buffer_valid_bytes_ - buffer_offset_;
            if (available == 0) {
                return ScanStatus::EndOfFile;
            }

            // If still less than frame header + CRC and at EOF, it's an incomplete tail
            if (available < sizeof(storage::RawFrameHeader) + sizeof(std::uint32_t)) {
                if (cursor_ >= file_len) {
                    return ScanStatus::IncompleteTail;
                }
            }
        }

        const std::uint8_t* ptr = buffer_.data() + buffer_offset_;
        const auto* raw_hdr = reinterpret_cast<const storage::RawFrameHeader*>(ptr);

        // Quick check on frame magic and version
        if (raw_hdr->frame_magic != FRAME_MAGIC || raw_hdr->format_version != FRAME_FORMAT_VERSION) {
            return ScanStatus::CorruptedRecord;
        }

        // Validate lengths against configured bounds
        if (raw_hdr->key_len > MAX_KEY_SIZE || raw_hdr->val_len > MAX_VALUE_SIZE) {
            return ScanStatus::CorruptedRecord;
        }

        std::size_t frame_total_size = sizeof(storage::RawFrameHeader) + raw_hdr->key_len + raw_hdr->val_len + sizeof(std::uint32_t);

        // If current buffer capacity is smaller than frame total size, expand buffer
        if (frame_total_size > buffer_.size()) {
            std::size_t new_cap = std::max(buffer_.size() * 2, frame_total_size);
            std::vector<std::uint8_t> new_buf(new_cap);
            if (available > 0) {
                std::memcpy(new_buf.data(), buffer_.data() + buffer_offset_, available);
            }
            buffer_ = std::move(new_buf);
            buffer_valid_bytes_ = available;
            buffer_offset_ = 0;

            Status s = refill_buffer();
            if (!is_ok(s)) {
                return ScanStatus::IoError;
            }
            available = buffer_valid_bytes_ - buffer_offset_;
            ptr = buffer_.data() + buffer_offset_;
        }

        // Check if full frame is buffered
        if (available < frame_total_size) {
            // Need to refill more
            if (cursor_ < file_len) {
                if (buffer_offset_ > 0) {
                    std::memmove(buffer_.data(), buffer_.data() + buffer_offset_, available);
                    buffer_valid_bytes_ = available;
                    buffer_offset_ = 0;
                }
                Status s = refill_buffer();
                if (!is_ok(s)) {
                    return ScanStatus::IoError;
                }
                continue; // Re-evaluate with refilled data
            } else {
                // At EOF but cannot read full frame: crash-torn tail
                return ScanStatus::IncompleteTail;
            }
        }

        // Full frame is in memory; deserialize and verify CRC
        std::size_t bytes_consumed = 0;
        Status s = storage::deserialize_frame(ptr, available, record.frame, bytes_consumed);
        if (!is_ok(s)) {
            // Checksum failure or structure corruption
            return ScanStatus::CorruptedRecord;
        }

        record.offset = (cursor_ - buffer_valid_bytes_) + buffer_offset_;
        record.length = bytes_consumed;

        buffer_offset_ += bytes_consumed;
        return ScanStatus::Ok;
    }
}

} // namespace kvdb::recovery

