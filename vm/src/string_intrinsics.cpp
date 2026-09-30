#include "string_intrinsics.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <limits>
#include <system_error>
#include <utility>
#include <vector>

namespace wio::vm::detail
{
    namespace
    {
        constexpr std::uint8_t TypeI8 = 3;
        constexpr std::uint8_t TypeI16 = 4;
        constexpr std::uint8_t TypeI32 = 5;
        constexpr std::uint8_t TypeI64 = 6;
        constexpr std::uint8_t TypeISize = 7;
        constexpr std::uint8_t TypeU8 = 8;
        constexpr std::uint8_t TypeU16 = 9;
        constexpr std::uint8_t TypeU32 = 10;
        constexpr std::uint8_t TypeU64 = 11;
        constexpr std::uint8_t TypeUSize = 12;
        StringIntrinsicResult success(Value value = {})
        {
            return {.recognized = true, .succeeded = true, .value = std::move(value)};
        }

        StringIntrinsicResult failure(std::string code, std::string message)
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

        bool charValue(const Value* value, char& output) noexcept
        {
            if (!value || value->kind() != Value::Kind::UnsignedInteger || value->asUnsignedInteger() > 0xffu)
                return false;
            output = static_cast<char>(value->asUnsignedInteger());
            return true;
        }

        bool stringValue(const Value* value, std::string_view& output) noexcept
        {
            if (!value || value->kind() != Value::Kind::String)
                return false;
            output = value->asString();
            return true;
        }

        bool isSpace(const char value) noexcept
        {
            return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f' || value == '\v';
        }

        std::string_view trimView(std::string_view value) noexcept
        {
            while (!value.empty() && isSpace(value.front()))
                value.remove_prefix(1);
            while (!value.empty() && isSpace(value.back()))
                value.remove_suffix(1);
            return value;
        }

        std::string trimCopy(const std::string_view value)
        {
            return std::string{trimView(value)};
        }

        std::string trimStartCopy(std::string_view value)
        {
            while (!value.empty() && isSpace(value.front()))
                value.remove_prefix(1);
            return std::string{value};
        }

        std::string trimEndCopy(std::string_view value)
        {
            while (!value.empty() && isSpace(value.back()))
                value.remove_suffix(1);
            return std::string{value};
        }

        char lowerChar(const char value) noexcept
        {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
        }

        char upperChar(const char value) noexcept
        {
            return static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
        }

        std::string replaceCopy(std::string value, const std::string_view oldValue, const std::string_view newValue,
                                const bool firstOnly)
        {
            if (oldValue.empty())
                return value;
            std::size_t position = 0;
            while ((position = value.find(oldValue, position)) != std::string::npos)
            {
                value.replace(position, oldValue.size(), newValue);
                if (firstOnly)
                    break;
                position += newValue.size();
            }
            return value;
        }

        bool equalsIgnoreCase(const std::string_view left, const std::string_view right) noexcept
        {
            if (left.size() != right.size())
                return false;
            for (std::size_t index = 0; index < left.size(); ++index)
            {
                if (lowerChar(left[index]) != lowerChar(right[index]))
                    return false;
            }
            return true;
        }

        struct IntegerInput final
        {
            std::string_view digits;
            int base = 10;
            bool negative = false;
            bool valid = false;
        };

        IntegerInput normalizeInteger(std::string_view text, int base) noexcept
        {
            text = trimView(text);
            if (text.empty() || (base != 0 && (base < 2 || base > 36)))
                return {};
            bool negative = false;
            if (text.front() == '+' || text.front() == '-')
            {
                negative = text.front() == '-';
                text.remove_prefix(1);
                if (text.empty())
                    return {};
            }
            auto consumePrefix = [&](const char prefix, const int detected)
            {
                if (text.size() >= 2 && text[0] == '0' && lowerChar(text[1]) == prefix)
                {
                    text.remove_prefix(2);
                    base = detected;
                    return true;
                }
                return false;
            };
            if (base == 0)
            {
                if (!consumePrefix('x', 16) && !consumePrefix('b', 2) && !consumePrefix('o', 8))
                    base = 10;
            }
            else if ((base == 16 && text.size() >= 2 && text[0] == '0' && lowerChar(text[1]) == 'x') ||
                     (base == 2 && text.size() >= 2 && text[0] == '0' && lowerChar(text[1]) == 'b') ||
                     (base == 8 && text.size() >= 2 && text[0] == '0' && lowerChar(text[1]) == 'o'))
                text.remove_prefix(2);
            return {.digits = text, .base = base, .negative = negative, .valid = !text.empty()};
        }

        StringIntrinsicResult parseInteger(const std::string_view text, const int base, const std::uint8_t target)
        {
            const IntegerInput input = normalizeInteger(text, base);
            if (!input.valid)
                return failure("WVM1128", "String integer conversion has invalid input or base");
            std::uint64_t magnitude = 0;
            const auto [end, error] =
                std::from_chars(input.digits.data(), input.digits.data() + input.digits.size(), magnitude, input.base);
            if (error != std::errc{} || end != input.digits.data() + input.digits.size())
                return failure("WVM1128", "String integer conversion failed");
            const bool signedTarget = target >= TypeI8 && target <= TypeISize;
            const std::uint32_t bits = target == TypeI8 || target == TypeU8     ? 8
                                       : target == TypeI16 || target == TypeU16 ? 16
                                       : target == TypeI32 || target == TypeU32 ? 32
                                                                                : 64;
            if (!signedTarget)
            {
                const std::uint64_t maximum =
                    bits == 64 ? (std::numeric_limits<std::uint64_t>::max)() : (std::uint64_t{1} << bits) - 1;
                if (input.negative || magnitude > maximum)
                    return failure("WVM1128", "String unsigned conversion is out of range");
                return success(Value::unsignedInteger(magnitude));
            }
            const std::uint64_t maximum = bits == 64
                                              ? static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())
                                              : (std::uint64_t{1} << (bits - 1)) - 1;
            const std::uint64_t negativeMaximum = maximum + 1;
            if ((!input.negative && magnitude > maximum) || (input.negative && magnitude > negativeMaximum))
                return failure("WVM1128", "String signed conversion is out of range");
            if (input.negative && magnitude == (std::uint64_t{1} << 63))
                return success(Value::signedInteger((std::numeric_limits<std::int64_t>::min)()));
            const std::int64_t value =
                input.negative ? -static_cast<std::int64_t>(magnitude) : static_cast<std::int64_t>(magnitude);
            return success(Value::signedInteger(value));
        }

        StringIntrinsicResult mutateString(Value* receiver, std::string value)
        {
            if (!receiver)
                return failure("WVM1135", "String mutation requires a mutable receiver");
            *receiver = Value::string(std::move(value));
            return success();
        }
    } // namespace

    StringIntrinsicResult executeStringIntrinsic(const std::string_view selector, const Value& receiver,
                                                 Value* mutableReceiver, const std::span<const Value* const> arguments,
                                                 const std::uint8_t resultType)
    {
        if (receiver.kind() != Value::Kind::String)
            return failure("WVM1116", "String intrinsic receiver is not a string");
        const std::string_view value = receiver.asString();
        if (selector == "Count")
            return success(Value::unsignedInteger(value.size()));
        if (selector == "Empty")
            return success(Value::boolean(value.empty()));
        if (selector == "Contains" || selector == "ContainsChar")
        {
            std::string_view needle;
            char character = 0;
            if (stringValue(argument(arguments, 0), needle))
                return success(Value::boolean(value.find(needle) != std::string_view::npos));
            if (charValue(argument(arguments, 0), character))
                return success(Value::boolean(value.find(character) != std::string_view::npos));
            return failure("WVM1117", "String Contains requires a string or byte-sized char");
        }
        if (selector == "StartsWith" || selector == "EndsWith")
        {
            std::string_view needle;
            if (!stringValue(argument(arguments, 0), needle))
                return failure("WVM1118", "String prefix/suffix query requires a string");
            return success(
                Value::boolean(selector == "StartsWith" ? value.starts_with(needle) : value.ends_with(needle)));
        }
        if (selector == "IndexOf" || selector == "LastIndexOf" || selector == "IndexOfChar" ||
            selector == "LastIndexOfChar")
        {
            std::size_t position = std::string_view::npos;
            std::string_view needle;
            char character = 0;
            const bool reverse = selector == "LastIndexOf" || selector == "LastIndexOfChar";
            if (stringValue(argument(arguments, 0), needle))
                position = reverse ? value.rfind(needle) : value.find(needle);
            else if (charValue(argument(arguments, 0), character))
                position = reverse ? value.rfind(character) : value.find(character);
            else
                return failure("WVM1119", "String index query requires a string or byte-sized char");
            return success(
                Value::signedInteger(position == std::string_view::npos ? -1 : static_cast<std::int64_t>(position)));
        }
        if (selector == "First" || selector == "Last")
        {
            if (value.empty())
                return failure("WVM1120", "String endpoint intrinsic requires a non-empty string");
            const unsigned char character =
                static_cast<unsigned char>(selector == "First" ? value.front() : value.back());
            return success(Value::unsignedInteger(character));
        }
        if (selector == "Get" || selector == "At" || selector == "GetOr")
        {
            std::size_t index = 0;
            if (!indexValue(argument(arguments, 0), index))
                return failure("WVM1121", "String indexing requires a non-negative index");
            if (index >= value.size())
            {
                char fallback = 0;
                if (selector == "GetOr" && charValue(argument(arguments, 1), fallback))
                    return success(Value::unsignedInteger(static_cast<unsigned char>(fallback)));
                return failure("WVM1122", "String index is outside the byte bounds");
            }
            return success(Value::unsignedInteger(static_cast<unsigned char>(value[index])));
        }
        if (selector == "Slice" || selector == "SliceFrom" || selector == "Take" || selector == "Skip" ||
            selector == "Left" || selector == "Right")
        {
            std::size_t first = 0;
            if (!indexValue(argument(arguments, 0), first))
                return failure("WVM1123", "String slicing requires a non-negative index or count");
            if (selector == "Take" || selector == "Left")
                return success(Value::string(std::string{value.substr(0, (std::min)(first, value.size()))}));
            if (selector == "Skip" || selector == "SliceFrom")
                return success(Value::string(std::string{value.substr((std::min)(first, value.size()))}));
            if (selector == "Right")
                return success(Value::string(first >= value.size() ? std::string{value}
                                                                   : std::string{value.substr(value.size() - first)}));
            const std::size_t start = (std::min)(first, value.size());
            if (arguments.size() == 1)
                return success(Value::string(std::string{value.substr(start)}));
            std::size_t count = 0;
            if (!indexValue(argument(arguments, 1), count))
                return failure("WVM1123", "String Slice requires a non-negative count");
            return success(Value::string(std::string{value.substr(start, (std::min)(count, value.size() - start))}));
        }
        if (selector == "Trim" || selector == "TrimStart" || selector == "TrimEnd")
            return success(Value::string(selector == "Trim"        ? trimCopy(value)
                                         : selector == "TrimStart" ? trimStartCopy(value)
                                                                   : trimEndCopy(value)));
        if (selector == "ToLower" || selector == "ToUpper" || selector == "Reversed")
        {
            std::string transformed{value};
            if (selector == "Reversed")
                std::reverse(transformed.begin(), transformed.end());
            else
                std::transform(transformed.begin(), transformed.end(), transformed.begin(),
                               selector == "ToLower" ? lowerChar : upperChar);
            return success(Value::string(std::move(transformed)));
        }
        if (selector == "Replace" || selector == "ReplaceFirst")
        {
            std::string_view oldValue;
            std::string_view newValue;
            if (!stringValue(argument(arguments, 0), oldValue) || !stringValue(argument(arguments, 1), newValue))
                return failure("WVM1124", "String replacement requires old and new string values");
            return success(
                Value::string(replaceCopy(std::string{value}, oldValue, newValue, selector == "ReplaceFirst")));
        }
        if (selector == "Repeat")
        {
            std::size_t count = 0;
            if (!indexValue(argument(arguments, 0), count) ||
                (!value.empty() && count > (std::numeric_limits<std::size_t>::max)() / value.size()))
                return failure("WVM1125", "String Repeat count exceeds runtime limits");
            std::string repeated;
            repeated.reserve(value.size() * count);
            for (std::size_t index = 0; index < count; ++index)
                repeated.append(value);
            return success(Value::string(std::move(repeated)));
        }
        if (selector == "Split" || selector == "Lines")
        {
            std::vector<Value> parts;
            if (selector == "Split")
            {
                std::string_view separator;
                if (!stringValue(argument(arguments, 0), separator))
                    return failure("WVM1126", "String Split requires a separator");
                if (separator.empty())
                {
                    parts.reserve(value.size());
                    for (const char character : value)
                        parts.push_back(Value::string(std::string(1, character)));
                }
                else
                {
                    std::size_t start = 0;
                    while (true)
                    {
                        const std::size_t position = value.find(separator, start);
                        if (position == std::string_view::npos)
                        {
                            parts.push_back(Value::string(std::string{value.substr(start)}));
                            break;
                        }
                        parts.push_back(Value::string(std::string{value.substr(start, position - start)}));
                        start = position + separator.size();
                    }
                }
            }
            else
            {
                std::size_t start = 0;
                while (start <= value.size())
                {
                    std::size_t end = start;
                    while (end < value.size() && value[end] != '\n' && value[end] != '\r')
                        ++end;
                    parts.push_back(Value::string(std::string{value.substr(start, end - start)}));
                    if (end >= value.size())
                        break;
                    start = value[end] == '\r' && end + 1 < value.size() && value[end + 1] == '\n' ? end + 2 : end + 1;
                }
            }
            return success(Value::array(std::move(parts)));
        }
        if (selector == "PadLeft" || selector == "PadRight")
        {
            std::size_t width = 0;
            char fill = ' ';
            if (!indexValue(argument(arguments, 0), width) ||
                (arguments.size() == 2 && !charValue(argument(arguments, 1), fill)))
                return failure("WVM1127", "String padding requires a width and optional byte-sized char");
            if (width <= value.size())
                return success(Value::string(std::string{value}));
            const std::string padding(width - value.size(), fill);
            return success(
                Value::string(selector == "PadLeft" ? padding + std::string{value} : std::string{value} + padding));
        }
        if (selector == "ToI8" || selector == "ToI16" || selector == "ToI32" || selector == "ToI64" ||
            selector == "ToISize" || selector == "ToU8" || selector == "ToU16" || selector == "ToU32" ||
            selector == "ToU64" || selector == "ToUSize")
        {
            int base = 10;
            if (!arguments.empty())
            {
                const Value* baseValue = argument(arguments, 0);
                if (!baseValue || baseValue->kind() != Value::Kind::SignedInteger ||
                    baseValue->asSignedInteger() < (std::numeric_limits<int>::min)() ||
                    baseValue->asSignedInteger() > (std::numeric_limits<int>::max)())
                    return failure("WVM1128", "String integer conversion base is invalid");
                base = static_cast<int>(baseValue->asSignedInteger());
            }
            return parseInteger(value, base, resultType);
        }
        if (selector == "ToF32" || selector == "ToF64")
        {
            std::string_view source = trimView(value);
            if (!source.empty() && source.front() == '+')
                source.remove_prefix(1);
            if (selector == "ToF32")
            {
                float parsed = 0.0f;
                const auto [end, error] =
                    std::from_chars(source.data(), source.data() + source.size(), parsed, std::chars_format::general);
                if (source.empty() || error != std::errc{} || end != source.data() + source.size() ||
                    !std::isfinite(parsed))
                    return failure("WVM1129", "String f32 conversion failed");
                return success(Value::floating(parsed));
            }
            double parsed = 0.0;
            const auto [end, error] =
                std::from_chars(source.data(), source.data() + source.size(), parsed, std::chars_format::general);
            if (source.empty() || error != std::errc{} || end != source.data() + source.size() ||
                !std::isfinite(parsed))
                return failure("WVM1129", "String floating-point conversion failed");
            return success(Value::floating(parsed));
        }
        if (selector == "ToBool")
        {
            const std::string_view source = trimView(value);
            if (source == "1" || equalsIgnoreCase(source, "true") || equalsIgnoreCase(source, "yes") ||
                equalsIgnoreCase(source, "on"))
                return success(Value::boolean(true));
            if (source == "0" || equalsIgnoreCase(source, "false") || equalsIgnoreCase(source, "no") ||
                equalsIgnoreCase(source, "off"))
                return success(Value::boolean(false));
            return failure("WVM1130", "String boolean conversion failed");
        }

        std::string mutated{value};
        if (selector == "Append")
        {
            std::string_view suffix;
            if (!stringValue(argument(arguments, 0), suffix))
                return failure("WVM1131", "String Append requires a string suffix");
            mutated.append(suffix);
        }
        else if (selector == "Push")
        {
            char character = 0;
            if (!charValue(argument(arguments, 0), character))
                return failure("WVM1132", "String Push requires a byte-sized char");
            mutated.push_back(character);
        }
        else if (selector == "Insert")
        {
            std::size_t index = 0;
            std::string_view fragment;
            if (!indexValue(argument(arguments, 0), index) || !stringValue(argument(arguments, 1), fragment))
                return failure("WVM1133", "String Insert requires an index and fragment");
            mutated.insert((std::min)(index, mutated.size()), fragment);
        }
        else if (selector == "Erase")
        {
            std::size_t index = 0;
            std::size_t count = 0;
            if (!indexValue(argument(arguments, 0), index) || !indexValue(argument(arguments, 1), count))
                return failure("WVM1134", "String Erase requires index and count operands");
            if (index < mutated.size())
                mutated.erase(index, count);
        }
        else if (selector == "Clear")
            mutated.clear();
        else if (selector == "Reverse")
            std::reverse(mutated.begin(), mutated.end());
        else if (selector == "ReplaceInPlace")
        {
            std::string_view oldValue;
            std::string_view newValue;
            if (!stringValue(argument(arguments, 0), oldValue) || !stringValue(argument(arguments, 1), newValue))
                return failure("WVM1124", "String replacement requires old and new string values");
            mutated = replaceCopy(std::move(mutated), oldValue, newValue, false);
        }
        else if (selector == "TrimInPlace")
            mutated = trimCopy(mutated);
        else if (selector == "ToLowerInPlace" || selector == "ToUpperInPlace")
            std::transform(mutated.begin(), mutated.end(), mutated.begin(),
                           selector == "ToLowerInPlace" ? lowerChar : upperChar);
        else
            return {};
        return mutateString(mutableReceiver, std::move(mutated));
    }
} // namespace wio::vm::detail
