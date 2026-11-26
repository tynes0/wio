#include "text_intrinsics.h"

#include "wio/vm/unicode.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace wio::vm::detail
{
    namespace
    {
        TextIntrinsicResult success(Value value)
        {
            return {.recognized = true, .succeeded = true, .value = std::move(value)};
        }

        TextIntrinsicResult failure(std::string code, std::string message)
        {
            return {.recognized = true, .succeeded = false, .code = std::move(code), .message = std::move(message)};
        }

        const Value* argument(const std::span<const Value* const> arguments, const std::size_t index) noexcept
        {
            return index < arguments.size() ? arguments[index] : nullptr;
        }

        bool indexValue(const Value* value, std::size_t& output) noexcept
        {
            if (!value)
                return false;
            if (value->kind() == Value::Kind::UnsignedInteger &&
                value->asUnsignedInteger() <= (std::numeric_limits<std::size_t>::max)())
            {
                output = static_cast<std::size_t>(value->asUnsignedInteger());
                return true;
            }
            if (value->kind() == Value::Kind::SignedInteger && value->asSignedInteger() >= 0 &&
                static_cast<std::uint64_t>(value->asSignedInteger()) <=
                    static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))
            {
                output = static_cast<std::size_t>(value->asSignedInteger());
                return true;
            }
            return false;
        }

        bool isCombiningMark(const char32_t scalar) noexcept
        {
            return (scalar >= 0x0300 && scalar <= 0x036f) || (scalar >= 0x1ab0 && scalar <= 0x1aff) ||
                   (scalar >= 0x1dc0 && scalar <= 0x1dff) || (scalar >= 0x20d0 && scalar <= 0x20ff) ||
                   (scalar >= 0xfe20 && scalar <= 0xfe2f);
        }

        bool isVariationSelector(const char32_t scalar) noexcept
        {
            return (scalar >= 0xfe00 && scalar <= 0xfe0f) || (scalar >= 0xe0100 && scalar <= 0xe01ef);
        }

        bool isEmojiModifier(const char32_t scalar) noexcept
        {
            return scalar >= 0x1f3fb && scalar <= 0x1f3ff;
        }

        bool isRegionalIndicator(const char32_t scalar) noexcept
        {
            return scalar >= 0x1f1e6 && scalar <= 0x1f1ff;
        }

        std::vector<std::size_t> graphemeBoundaries(const std::u32string_view text)
        {
            std::vector<std::size_t> boundaries{0};
            if (text.empty())
                return boundaries;
            char32_t previous = 0;
            std::size_t regionalRun = 0;
            for (std::size_t index = 0; index < text.size(); ++index)
            {
                const char32_t current = text[index];
                bool breaks = index != 0;
                if (isCombiningMark(current) || isVariationSelector(current) || isEmojiModifier(current) ||
                    current == 0x200d || previous == 0x200d)
                    breaks = false;
                if (isRegionalIndicator(current))
                {
                    if (isRegionalIndicator(previous) && regionalRun % 2u == 1u)
                        breaks = false;
                    ++regionalRun;
                }
                else
                    regionalRun = 0;
                if (breaks)
                    boundaries.push_back(index);
                previous = current;
            }
            boundaries.push_back(text.size());
            return boundaries;
        }

        std::size_t displayWidth(const std::u32string_view text) noexcept
        {
            std::size_t width = 0;
            for (const char32_t scalar : text)
            {
                if (isCombiningMark(scalar) || isVariationSelector(scalar) || scalar == 0x200d || scalar < 0x20 ||
                    (scalar >= 0x7f && scalar < 0xa0))
                    continue;
                const bool wide = (scalar >= 0x1100 && scalar <= 0x115f) || scalar == 0x2329 || scalar == 0x232a ||
                                  (scalar >= 0x2e80 && scalar <= 0xa4cf) || (scalar >= 0xac00 && scalar <= 0xd7a3) ||
                                  (scalar >= 0xf900 && scalar <= 0xfaff) || (scalar >= 0xfe10 && scalar <= 0xfe6f) ||
                                  (scalar >= 0xff00 && scalar <= 0xff60) || (scalar >= 0x1f300 && scalar <= 0x1faff) ||
                                  (scalar >= 0x20000 && scalar <= 0x3ffff);
                width += wide ? 2u : 1u;
            }
            return width;
        }

        char32_t lowerScalar(const char32_t scalar) noexcept
        {
            if (scalar >= U'A' && scalar <= U'Z')
                return scalar + 0x20;
            if ((scalar >= 0x00c0 && scalar <= 0x00d6) || (scalar >= 0x00d8 && scalar <= 0x00de))
                return scalar + 0x20;
            return scalar;
        }

        std::u32string caseFold(const std::u32string_view text)
        {
            std::u32string folded;
            folded.reserve(text.size());
            for (char32_t scalar : text)
            {
                if (scalar == 0x00df || scalar == 0x1e9e)
                {
                    folded.append(U"ss");
                    continue;
                }
                if (scalar == 0x03c2)
                    scalar = 0x03c3;
                folded.push_back(lowerScalar(scalar));
            }
            return folded;
        }
    } // namespace

    TextIntrinsicResult executeTextIntrinsic(const std::string_view selector, const Value& receiver,
                                             const std::span<const Value* const> arguments)
    {
        if (receiver.kind() != Value::Kind::Text)
            return failure("WVM1136", "Text intrinsic receiver is not Unicode text");
        const std::u32string_view text = receiver.asText();
        if (selector == "Count")
            return success(Value::unsignedInteger(text.size()));
        if (selector == "ByteCount")
            return success(Value::unsignedInteger(encodeUtf8(text).size()));
        if (selector == "Empty")
            return success(Value::boolean(text.empty()));
        if (selector == "ToString" || selector == "Utf8")
            return success(Value::string(encodeUtf8(text)));
        if (selector == "Slice")
        {
            std::size_t start = 0;
            if (!indexValue(argument(arguments, 0), start))
                return failure("WVM1060", "Unicode text slice requires a non-negative start");
            start = (std::min)(start, text.size());
            std::size_t count = text.size() - start;
            if (arguments.size() > 1 && !indexValue(argument(arguments, 1), count))
                return failure("WVM1060", "Unicode text slice requires a non-negative count");
            count = (std::min)(count, text.size() - start);
            return success(Value::text(std::u32string{text.substr(start, count)}));
        }
        if (selector == "Get" || selector == "At")
        {
            std::size_t index = 0;
            if (!indexValue(argument(arguments, 0), index) || index >= text.size())
                return failure("WVM1059", "Unicode text index is outside the scalar bounds");
            return success(Value::unsignedInteger(text[index]));
        }
        if (selector == "Contains" || selector == "StartsWith" || selector == "EndsWith")
        {
            const Value* other = argument(arguments, 0);
            if (!other || other->kind() != Value::Kind::Text)
                return failure("WVM1137", "Unicode text query requires another text value");
            const std::u32string_view needle = other->asText();
            if (selector == "Contains")
                return success(Value::boolean(text.find(needle) != std::u32string_view::npos));
            return success(
                Value::boolean(selector == "StartsWith" ? text.starts_with(needle) : text.ends_with(needle)));
        }

        if (selector == "GraphemeCount" || selector == "SliceGraphemes" || selector == "Graphemes")
        {
            const std::vector<std::size_t> boundaries = graphemeBoundaries(text);
            const std::size_t count = boundaries.empty() ? 0 : boundaries.size() - 1;
            if (selector == "GraphemeCount")
                return success(Value::unsignedInteger(count));
            if (selector == "SliceGraphemes")
            {
                std::size_t start = 0;
                std::size_t length = 0;
                if (!indexValue(argument(arguments, 0), start) || !indexValue(argument(arguments, 1), length))
                    return failure("WVM1138", "Grapheme slicing requires non-negative start and count values");
                if (start >= count)
                    return success(Value::text({}));
                const std::size_t end = start + (std::min)(length, count - start);
                return success(
                    Value::text(std::u32string{text.substr(boundaries[start], boundaries[end] - boundaries[start])}));
            }
            std::vector<Value> graphemes;
            graphemes.reserve(count);
            for (std::size_t index = 0; index < count; ++index)
                graphemes.push_back(Value::text(
                    std::u32string{text.substr(boundaries[index], boundaries[index + 1] - boundaries[index])}));
            return success(Value::array(std::move(graphemes)));
        }
        if (selector == "DisplayWidth")
            return success(Value::unsignedInteger(displayWidth(text)));
        if (selector == "CaseFold")
            return success(Value::text(caseFold(text)));
        if (selector == "CodePoints")
        {
            std::vector<Value> scalars;
            scalars.reserve(text.size());
            for (const char32_t scalar : text)
                scalars.push_back(Value::unsignedInteger(scalar));
            return success(Value::array(std::move(scalars)));
        }
        return {};
    }
} // namespace wio::vm::detail
