#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace wio::vm
{
    struct Utf8Error
    {
        std::size_t offset = 0;
        std::string message;
    };

    struct Utf8DecodeResult
    {
        std::u32string value;
        std::optional<Utf8Error> error;

        [[nodiscard]] bool succeeded() const noexcept
        {
            return !error.has_value();
        }
    };

    [[nodiscard]] Utf8DecodeResult decodeUtf8(std::string_view input);
    [[nodiscard]] std::string encodeUtf8(std::u32string_view input);
} // namespace wio::vm
