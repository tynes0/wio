#pragma once

#include <cstdint>
#include <iostream>
#include <string>

namespace wio::runtime::std_io
{
    template <typename T>
    inline std::int32_t WriteValue(const T& value)
    {
        std::cout << value;
        return 0;
    }

    inline std::int32_t PrintString(const std::string& value) { return WriteValue(value); }
    inline std::int32_t PrintBool(bool value) { return WriteValue(value ? "true" : "false"); }
    inline std::int32_t PrintChar(char value) { return WriteValue(value); }
    inline std::int32_t PrintI8(std::int8_t value) { return WriteValue(static_cast<std::int32_t>(value)); }
    inline std::int32_t PrintI16(std::int16_t value) { return WriteValue(value); }
    inline std::int32_t PrintI32(std::int32_t value) { return WriteValue(value); }
    inline std::int32_t PrintI64(std::int64_t value) { return WriteValue(value); }
    inline std::int32_t PrintU8(std::uint8_t value) { return WriteValue(static_cast<std::uint32_t>(value)); }
    inline std::int32_t PrintU16(std::uint16_t value) { return WriteValue(value); }
    inline std::int32_t PrintU32(std::uint32_t value) { return WriteValue(value); }
    inline std::int32_t PrintU64(std::uint64_t value) { return WriteValue(value); }
    inline std::int32_t PrintISize(std::intptr_t value) { return WriteValue(value); }
    inline std::int32_t PrintUSize(std::uintptr_t value) { return WriteValue(value); }
    inline std::int32_t PrintF32(float value) { return WriteValue(value); }
    inline std::int32_t PrintF64(double value) { return WriteValue(value); }
}
