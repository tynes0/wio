#pragma once

#include <cstdint>

namespace wio::runtime::detail::io {
    enum class WriteError : uint8_t
    {
        none = 0,
        null_file,
        null_data,
        size_overflow,
        io_error,
        partial_write
    };

    enum class ReadError : uint8_t
    {
        none = 0,
        null_file,
        invalid_argument,
        eof,
        io_error,
        partial_read,
        platform_error
    };
}