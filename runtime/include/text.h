#pragma once

#include "std_unicode.h"

#include <compare>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
        [[nodiscard]] Text at(const std::size_t index) const { return slice(index, 1); }
        [[nodiscard]] Text operator[](const std::size_t index) const { return at(index); }
        [[nodiscard]] bool contains(const Text& other) const noexcept
        {
            return value_.find(other.value_) != std::string::npos;
        }
        [[nodiscard]] bool startsWith(const Text& other) const noexcept
        {
            return value_.starts_with(other.value_);
        }
        [[nodiscard]] bool endsWith(const Text& other) const noexcept
        {
            return value_.ends_with(other.value_);
        }
        [[nodiscard]] std::size_t graphemeCount() const noexcept
        {
            return std_unicode::GraphemeCount(value_);
        }
        [[nodiscard]] Text sliceGraphemes(const std::size_t start, const std::size_t count) const
        {
            return Text(std_unicode::SliceGraphemes(value_, start, count), ValidatedTag{});
        }
        [[nodiscard]] std::size_t displayWidth() const noexcept
        {
            return std_unicode::DisplayWidth(value_);
        }
        [[nodiscard]] Text caseFold() const
        {
            return Text(std_unicode::CaseFold(value_), ValidatedTag{});
        }
        [[nodiscard]] std::vector<std::uint32_t> codePoints() const
        {
            std::vector<std::uint32_t> result;
            std::size_t errorOffset = 0;
            (void)std_unicode::TryDecode(value_, result, errorOffset);
            return result;
        }
        [[nodiscard]] std::vector<Text> graphemes() const
        {
            const auto boundaries = std_unicode::GraphemeBoundaries(value_);
            std::vector<Text> result;
            if (boundaries.size() < 2)
                return result;

            result.reserve(boundaries.size() - 1);
            for (std::size_t index = 1; index < boundaries.size(); ++index)
            {
                result.emplace_back(
                    Text(std::string(value_.substr(
                        boundaries[index - 1],
                        boundaries[index] - boundaries[index - 1])),
                        ValidatedTag{})
                );
            }
            return result;
        }

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

template <>
struct std::hash<wio::runtime::Text>
{
    std::size_t operator()(const wio::runtime::Text& value) const noexcept
    {
        return std::hash<std::string_view>{}(value.Utf8());
    }
};
