#pragma once

#include "std_unicode.h"

#include <compare>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace wio::runtime
{
    // A validated, owned UTF-8 value. Wio's `string` remains a byte string;
    // `text` is the semantic Unicode type and therefore indexes by code point.
    class Text
    {
    public:
        Text() = default;

        [[nodiscard]] static Text FromUtf8(const std::string_view value)
        {
            if (!std_unicode::IsValidUtf8(value))
                throw std::invalid_argument("Cannot construct text from invalid UTF-8.");
            return Text(std::string(value), ValidatedTag{});
        }

        [[nodiscard]] const std::string& Utf8() const noexcept { return value_; }
        [[nodiscard]] std::string _WF_ToString() const { return value_; }
        [[nodiscard]] std::size_t size() const noexcept { return std_unicode::CodePointCount(value_); }
        [[nodiscard]] std::size_t byteSize() const noexcept { return value_.size(); }
        [[nodiscard]] bool empty() const noexcept { return value_.empty(); }

        [[nodiscard]] Text slice(const std::size_t start) const
        {
            const std::size_t count = size();
            return start >= count ? Text{} : slice(start, count - start);
        }

        [[nodiscard]] Text slice(const std::size_t start, const std::size_t count) const
        {
            return Text(std_unicode::SliceCodePoints(value_, start, count), ValidatedTag{});
        }

        Text& operator+=(const Text& other)
        {
            value_ += other.value_;
            return *this;
        }

        [[nodiscard]] friend Text operator+(Text left, const Text& right)
        {
            left += right;
            return left;
        }

        [[nodiscard]] friend bool operator==(const Text&, const Text&) = default;
        [[nodiscard]] friend std::strong_ordering operator<=>(const Text& left, const Text& right) noexcept
        {
            return left.value_ <=> right.value_;
        }

        // The safe direction is implicit so text can cross existing console,
        // formatting, and native UTF-8 string boundaries without ceremony.
        [[nodiscard]] operator const std::string&() const noexcept { return value_; }

    private:
        struct ValidatedTag {};

        explicit Text(std::string value, ValidatedTag) : value_(std::move(value)) {}

        std::string value_;
    };

    [[nodiscard]] inline Text TextFromUtf8(const std::string_view value)
    {
        return Text::FromUtf8(value);
    }

    [[nodiscard]] inline std::string TextToUtf8(Text value)
    {
        return value.Utf8();
    }
}
