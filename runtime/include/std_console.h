#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace wio::runtime::std_console
{
    std::int32_t WriteValue(bool value);
    std::int32_t WriteValue(char value);
    std::int32_t WriteValue(std::int8_t value);
    std::int32_t WriteValue(std::int16_t value);
    std::int32_t WriteValue(std::int32_t value);
    std::int32_t WriteValue(std::int64_t value);
    std::int32_t WriteValue(std::uint8_t value);
    std::int32_t WriteValue(std::uint16_t value);
    std::int32_t WriteValue(std::uint32_t value);
    std::int32_t WriteValue(std::uint64_t value);
    std::int32_t WriteValue(float value);
    std::int32_t WriteValue(double value);
    std::int32_t WriteValue(const char* value);
    std::int32_t WriteValue(char* value);
    std::int32_t WriteValue(const std::string& value);
    std::int32_t WriteValue(std::string_view value);

    std::string Input(const std::string& prompt = "");
    std::string InputN(std::size_t count);
    char InputChar(bool isHidden = false);
    std::string InputWord();
    std::string InputUntil(char delimiter, bool includeDelimiter = false);
}
