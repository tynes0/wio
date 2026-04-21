#include "wio/common/utility.h"
#include "wio/common/exception.h"

#include <array>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>

namespace wio::common
{
    namespace
    {
        struct IntegerSuffixEntry
        {
            std::string_view suffix;
            IntegerType type;
        };

        struct FloatSuffixEntry
        {
            std::string_view suffix;
            FloatType type;
        };

        constexpr std::array<IntegerSuffixEntry, 14> kIntegerLiteralSuffixes = {{
            {"isize", IntegerType::isize},
            {"usize", IntegerType::usize},
            {"i64", IntegerType::i64},
            {"u64", IntegerType::u64},
            {"i32", IntegerType::i32},
            {"u32", IntegerType::u32},
            {"i16", IntegerType::i16},
            {"u16", IntegerType::u16},
            {"isz", IntegerType::isize},
            {"usz", IntegerType::usize},
            {"i8", IntegerType::i8},
            {"u8", IntegerType::u8},
            {"i", IntegerType::i32},
            {"u", IntegerType::u32}
        }};

        constexpr std::array<FloatSuffixEntry, 3> kFloatLiteralSuffixes = {{
            {"f32", FloatType::f32},
            {"f64", FloatType::f64},
            {"f", FloatType::f32}
        }};

        [[nodiscard]] auto splitIntegerLiteralSuffix(std::string_view data) -> std::pair<std::string_view, IntegerType>
        {
            for (const auto& entry : kIntegerLiteralSuffixes)
            {
                if (data.ends_with(entry.suffix))
                    return {data.substr(0, data.size() - entry.suffix.size()), entry.type};
            }

            return {data, IntegerType::Unknown};
        }

        [[nodiscard]] auto splitFloatLiteralSuffix(std::string_view data) -> std::pair<std::string_view, FloatType>
        {
            for (const auto& entry : kFloatLiteralSuffixes)
            {
                if (data.ends_with(entry.suffix))
                    return {data.substr(0, data.size() - entry.suffix.size()), entry.type};
            }

            return {data, FloatType::Unknown};
        }

        [[nodiscard]] IntegerResult invalidIntegerResult()
        {
            IntegerResult result{};
            result.type = IntegerType::Unknown;
            result.isValid = false;
            return result;
        }

        [[nodiscard]] FloatResult invalidFloatResult()
        {
            FloatResult result{};
            result.type = FloatType::Unknown;
            result.isValid = false;
            std::memset(&result.value, 0, sizeof(result.value));
            return result;
        }

        bool parseIntegerLiteralBody(std::string_view rawLiteral,
                                     bool& isNegative,
                                     int64_t& signedVal,
                                     uint64_t& unsignedVal)
        {
            if (rawLiteral.empty())
                return false;

            std::string_view rawNumStr = rawLiteral;
            signedVal = 0;
            unsignedVal = 0;
            isNegative = !rawNumStr.empty() && rawNumStr.front() == '-';

            if (isNegative)
                rawNumStr.remove_prefix(1);

            int base = 10;
            if (rawNumStr.starts_with("0x") || rawNumStr.starts_with("0X"))
            {
                base = 16;
                rawNumStr.remove_prefix(2);
            }
            else if (rawNumStr.starts_with("0b") || rawNumStr.starts_with("0B"))
            {
                base = 2;
                rawNumStr.remove_prefix(2);
            }
            else if (rawNumStr.starts_with("0o") || rawNumStr.starts_with("0O"))
            {
                base = 8;
                rawNumStr.remove_prefix(2);
            }

            if (rawNumStr.empty())
                return false;

            if (isNegative)
            {
                auto res = std::from_chars(rawNumStr.data(), rawNumStr.data() + rawNumStr.size(), signedVal, base);
                return res.ec == std::errc() && res.ptr == rawNumStr.data() + rawNumStr.size();
            }

            auto res = std::from_chars(rawNumStr.data(), rawNumStr.data() + rawNumStr.size(), unsignedVal, base);
            if (res.ec != std::errc() || res.ptr != rawNumStr.data() + rawNumStr.size())
                return false;

            if (unsignedVal <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
                signedVal = static_cast<int64_t>(unsignedVal);

            return true;
        }

        void setIntegerResultForType(IntegerType targetType,
                                     bool isNegative,
                                     int64_t signedVal,
                                     uint64_t unsignedVal,
                                     IntegerResult& result)
        {
            if (isNegative && (targetType == IntegerType::u8 || targetType == IntegerType::u16 ||
                               targetType == IntegerType::u32 || targetType == IntegerType::u64 ||
                               targetType == IntegerType::usize))
            {
                return;
            }

            auto checkAndSetSigned = [&]<typename Dest, typename Value>(Value value, Value minValue, Value maxValue, Dest& destination)
            {
                if (value < minValue || value > maxValue)
                    return;

                destination = static_cast<Dest>(value);
                result.type = targetType;
                result.isValid = true;
            };

            auto checkAndSetUnsigned = [&]<typename Dest, typename Value>(Value value, Value maxValue, Dest& destination)
            {
                if (value > maxValue)
                    return;

                destination = static_cast<Dest>(value);
                result.type = targetType;
                result.isValid = true;
            };

            switch (targetType)
            {
            case IntegerType::i8:
                checkAndSetSigned(static_cast<int64_t>(signedVal),
                                  static_cast<int64_t>(std::numeric_limits<int8_t>::min()),
                                  static_cast<int64_t>(std::numeric_limits<int8_t>::max()),
                                  result.value.v_i8);
                break;
            case IntegerType::i16:
                checkAndSetSigned(static_cast<int64_t>(signedVal),
                                  static_cast<int64_t>(std::numeric_limits<int16_t>::min()),
                                  static_cast<int64_t>(std::numeric_limits<int16_t>::max()),
                                  result.value.v_i16);
                break;
            case IntegerType::i32:
                checkAndSetSigned(static_cast<int64_t>(signedVal),
                                  static_cast<int64_t>(std::numeric_limits<int32_t>::min()),
                                  static_cast<int64_t>(std::numeric_limits<int32_t>::max()),
                                  result.value.v_i32);
                break;
            case IntegerType::i64:
                result.value.v_i64 = signedVal;
                result.type = IntegerType::i64;
                result.isValid = true;
                break;
            case IntegerType::u8:
                checkAndSetUnsigned(static_cast<uint64_t>(unsignedVal),
                                    static_cast<uint64_t>(std::numeric_limits<uint8_t>::max()),
                                    result.value.v_u8);
                break;
            case IntegerType::u16:
                checkAndSetUnsigned(static_cast<uint64_t>(unsignedVal),
                                    static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()),
                                    result.value.v_u16);
                break;
            case IntegerType::u32:
                checkAndSetUnsigned(static_cast<uint64_t>(unsignedVal),
                                    static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()),
                                    result.value.v_u32);
                break;
            case IntegerType::u64:
                result.value.v_u64 = unsignedVal;
                result.type = IntegerType::u64;
                result.isValid = true;
                break;
            case IntegerType::isize:
                checkAndSetSigned(static_cast<int64_t>(signedVal),
                                  static_cast<int64_t>(std::numeric_limits<isize>::min()),
                                  static_cast<int64_t>(std::numeric_limits<isize>::max()),
                                  result.value.v_isize);
                break;
            case IntegerType::usize:
                checkAndSetUnsigned(static_cast<uint64_t>(unsignedVal),
                                    static_cast<uint64_t>(std::numeric_limits<usize>::max()),
                                    result.value.v_usize);
                break;
            case IntegerType::Unknown:
                break;
            }
        }

        [[nodiscard]] IntegerResult parseIntegerLiteral(std::string_view data, std::optional<IntegerType> forcedType)
        {
            IntegerResult result = invalidIntegerResult();
            if (data.empty())
                return result;

            auto [rawLiteral, detectedType] = splitIntegerLiteralSuffix(data);
            if (forcedType.has_value())
            {
                if (detectedType != IntegerType::Unknown && detectedType != *forcedType)
                    return result;

                detectedType = *forcedType;
            }

            int64_t signedVal = 0;
            uint64_t unsignedVal = 0;
            bool isNegative = false;
            if (!parseIntegerLiteralBody(rawLiteral, isNegative, signedVal, unsignedVal))
                return result;

            if (detectedType != IntegerType::Unknown)
            {
                setIntegerResultForType(detectedType, isNegative, signedVal, unsignedVal, result);
                return result;
            }

            if (isNegative)
            {
                if (signedVal >= std::numeric_limits<int32_t>::min())
                {
                    result.type = IntegerType::i32;
                    result.value.v_i32 = static_cast<int32_t>(signedVal);
                    result.isValid = true;
                }
                else
                {
                    result.type = IntegerType::i64;
                    result.value.v_i64 = signedVal;
                    result.isValid = true;
                }

                return result;
            }

            if (unsignedVal <= static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
            {
                result.type = IntegerType::i32;
                result.value.v_i32 = static_cast<int32_t>(unsignedVal);
                result.isValid = true;
            }
            else if (unsignedVal <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
            {
                result.type = IntegerType::i64;
                result.value.v_i64 = static_cast<int64_t>(unsignedVal);
                result.isValid = true;
            }
            else
            {
                result.type = IntegerType::u64;
                result.value.v_u64 = unsignedVal;
                result.isValid = true;
            }

            return result;
        }

        [[nodiscard]] FloatResult parseFloatLiteral(std::string_view data, std::optional<FloatType> forcedType)
        {
            FloatResult result = invalidFloatResult();
            if (data.empty())
                return result;

            auto [rawLiteral, detectedType] = splitFloatLiteralSuffix(data);
            if (forcedType.has_value())
            {
                if (detectedType != FloatType::Unknown && detectedType != *forcedType)
                    return result;

                detectedType = *forcedType;
            }

            std::string temp(rawLiteral);
            char* end = nullptr;

            errno = 0;
            const double value = std::strtod(temp.c_str(), &end);
            if (errno != 0 || end != temp.c_str() + temp.size())
                return result;

            if (detectedType == FloatType::Unknown)
                detectedType = FloatType::f64;

            if (detectedType == FloatType::f32)
            {
                if (value < -std::numeric_limits<float>::max() || value > std::numeric_limits<float>::max())
                    return result;

                result.value.v_f32 = static_cast<float>(value);
                result.type = FloatType::f32;
                result.isValid = true;
                return result;
            }

            result.value.v_f64 = value;
            result.type = FloatType::f64;
            result.isValid = true;
            return result;
        }
    }

    bool isNewline(char c)
    {
        return c == '\n' || c == '\r';
    }

    char getEscapeSeq(char ch, Location loc)
    {
        if (ch == '\\' || ch == '\'' || ch == '\"')
            return ch;
        if (ch == 'n') // newline
            return '\n';
        if (ch == 'r') // return
            return '\r';
        if (ch == 'a') // alert
            return '\a';
        if (ch == 'b') // backspace
            return '\b';
        if (ch == 'f') // form feed page break
            return '\f';
        if (ch == 't') // horizontal tab
            return '\t';
        if (ch == 'v') // vertical tab
            return '\v';

        if (loc)
            throw InvalidStringError(("Invalid escape sequence: \\" + std::string(1, ch)).c_str(), loc);
        
        throw InvalidStringError(("Invalid escape sequence: \\" + std::string(1, ch)).c_str());
    }

    std::string wioStringToEscapedCppString(const std::string& str)
    {
        std::string result;
        for (char c : str)
        {
            switch (c)
            {
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\a': result += "\\a"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\v': result += "\\v"; break;
            case '\t': result += "\\t"; break;
            case '\'': result += "\\\'"; break;
            case '\"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            default:   result += c; break;
            }
        }
        return result;
    }

    bool hasIntegerLiteralTypeSuffix(std::string_view data)
    {
        const auto [rawLiteral, detectedType] = splitIntegerLiteralSuffix(data);
        return detectedType != IntegerType::Unknown && rawLiteral.size() != data.size();
    }

    bool hasFloatLiteralTypeSuffix(std::string_view data)
    {
        const auto [rawLiteral, detectedType] = splitFloatLiteralSuffix(data);
        return detectedType != FloatType::Unknown && rawLiteral.size() != data.size();
    }

    std::string stripIntegerLiteralTypeSuffix(std::string_view data)
    {
        const auto [rawLiteral, detectedType] = splitIntegerLiteralSuffix(data);
        if (detectedType == IntegerType::Unknown)
            return std::string(data);

        return std::string(rawLiteral);
    }

    std::string stripFloatLiteralTypeSuffix(std::string_view data)
    {
        const auto [rawLiteral, detectedType] = splitFloatLiteralSuffix(data);
        if (detectedType == FloatType::Unknown)
            return std::string(data);

        return std::string(rawLiteral);
    }

    IntegerResult getInteger(const std::string& data)
    {
        return parseIntegerLiteral(data, std::nullopt);
    }

    IntegerResult getIntegerAsType(std::string_view data, IntegerType type)
    {
        return parseIntegerLiteral(data, type);
    }

    FloatResult getFloat(const std::string& data)
    {
        return parseFloatLiteral(data, std::nullopt);
    }

    FloatResult getFloatAsType(std::string_view data, FloatType type)
    {
        return parseFloatLiteral(data, type);
    }


    std::string wioPrimitiveTypeToCppType(const std::string& wioTypeStr)
    {
        if (wioTypeStr == "void") return "void";
        if (wioTypeStr == "char") return "char";
        if (wioTypeStr == "uchar") return "unsigned char";
        if (wioTypeStr == "i") return "int32_t";
        if (wioTypeStr == "i8") return "int8_t";
        if (wioTypeStr == "i16") return "int16_t";
        if (wioTypeStr == "i32") return "int32_t";
        if (wioTypeStr == "i64") return "int64_t";
        if (wioTypeStr == "u") return "uint32_t";
        if (wioTypeStr == "u8") return "uint8_t";
        if (wioTypeStr == "u16") return "uint16_t";
        if (wioTypeStr == "u32") return "uint32_t";
        if (wioTypeStr == "u64") return "uint64_t";
        if (wioTypeStr == "f") return "float";
        if (wioTypeStr == "f32") return "float";
        if (wioTypeStr == "f64") return "double";
        if (wioTypeStr == "usize") return "size_t";
        if (wioTypeStr == "isize") return "ptrdiff_t";
        if (wioTypeStr == "byte") return "unsigned char";
        if (wioTypeStr == "bool") return "bool";
        if (wioTypeStr == "string") return "wio::String";
        if (wioTypeStr == "duration") return "wio::Duration";
        return {};
    }
}
