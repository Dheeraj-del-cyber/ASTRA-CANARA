#pragma once

#include <string>

namespace kvdb {

enum class Status {
    Ok = 0,
    NotFound,
    InvalidArgument,
    AlreadyOpen,
    NotOpen,
    Corruption,
    NoSpace,
    IoError,
    InternalError
};

inline const char* status_to_string(Status status) {
    switch (status) {
        case Status::Ok:              return "Ok";
        case Status::NotFound:        return "NotFound";
        case Status::InvalidArgument: return "InvalidArgument";
        case Status::AlreadyOpen:     return "AlreadyOpen";
        case Status::NotOpen:         return "NotOpen";
        case Status::Corruption:      return "Corruption";
        case Status::NoSpace:         return "NoSpace";
        case Status::IoError:         return "IoError";
        case Status::InternalError:   return "InternalError";
        default:                      return "UnknownStatus";
    }
}

inline bool is_ok(Status status) {
    return status == Status::Ok;
}

} // namespace kvdb

