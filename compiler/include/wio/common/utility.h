#pragma once
#include "location.h"
#include "general/result_values.h"

#include <dtlog.h>
#include <string_view>

namespace wio
{
    namespace common
    {
        [[nodiscard]] bool isNewline(char c);
        [[nodiscard]] char getEscapeSeq(char ch, Location loc = Location());
        [[nodiscard]] std::string wioStringToEscapedCppString(const std::string& str);
        [[nodiscard]] bool hasIntegerLiteralTypeSuffix(std::string_view data);
        [[nodiscard]] bool hasFloatLiteralTypeSuffix(std::string_view data);
        [[nodiscard]] std::string stripIntegerLiteralTypeSuffix(std::string_view data);
        [[nodiscard]] std::string stripFloatLiteralTypeSuffix(std::string_view data);
        [[nodiscard]] IntegerResult getInteger(const std::string& data);
        [[nodiscard]] IntegerResult getIntegerAsType(std::string_view data, IntegerType type);
        [[nodiscard]] FloatResult getFloat(const std::string& data);
        [[nodiscard]] FloatResult getFloatAsType(std::string_view data, FloatType type);
        [[nodiscard]] std::string wioPrimitiveTypeToCppType(const std::string& wioTypeStr);
        
        [[nodiscard]] constexpr uint64_t fnv1a(const char* str)
        {
            uint64_t hash = 1469598103934665603ull;
            while (*str)
            {
                hash ^= static_cast<uint64_t>(*str++);
                hash *= 1099511628211ull;
            }
            return hash;
        }

        template <typename ...Args>
        [[nodiscard]] std::string formatString(const std::string_view& fmt, Args&&... args)
        {
            return dtlog::formatter::format(fmt, std::forward<Args>(args)...);
        }
    }
}
