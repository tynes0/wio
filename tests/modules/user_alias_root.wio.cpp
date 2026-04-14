#include <cstdint>
#include <limits>
#include <string>
#include <vector>
#include <array>
#include <format>
#include <iostream>
#include <functional>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <exception.h>
#include <fit.h>
#include <module_api.h>
#include <ref.h>

#if defined(_WIN32)
#define WIO_EXPORT __declspec(dllexport)
#else
#define WIO_EXPORT __attribute__((visibility("default")))
#endif

namespace wio
{
    using String = std::string;
    using WString = std::wstring;
}

namespace wio
{
    template <typename T>
    using DArray = std::vector<T>;
    
    template <typename T, size_t N>
    using SArray = std::array<T, N>;
    
    template <typename K, typename V>
    using Dict = std::unordered_map<K, V>;
    
    template <typename K, typename V>
    using Tree = std::map<K, V>;
}

#include "std_io.h"


int32_t _WF_std_io_Print_string(wio::String value);


int32_t _WF_std_io_Print_bool(bool value);


int32_t _WF_std_io_Print_char(char value);


int32_t _WF_std_io_Print_i8(int8_t value);


int32_t _WF_std_io_Print_i16(int16_t value);


int32_t _WF_std_io_Print_i32(int32_t value);


int32_t _WF_std_io_Print_i64(int64_t value);


int32_t _WF_std_io_Print_u8(uint8_t value);


int32_t _WF_std_io_Print_u16(uint16_t value);


int32_t _WF_std_io_Print_u32(uint32_t value);


int32_t _WF_std_io_Print_u64(uint64_t value);


int32_t _WF_std_io_Print_isize(ptrdiff_t value);


int32_t _WF_std_io_Print_usize(size_t value);


int32_t _WF_std_io_Print_f32(float value);


int32_t _WF_std_io_Print_f64(double value);


int32_t _WF_std_io_PrintLine_string(wio::String value);


int32_t _WF_GetValue();


int32_t _WF_std_io_Print_string(wio::String value)
{
    return wio::runtime::std_io::PrintString(value);
}

int32_t _WF_std_io_Print_bool(bool value)
{
    return wio::runtime::std_io::PrintBool(value);
}

int32_t _WF_std_io_Print_char(char value)
{
    return wio::runtime::std_io::PrintChar(value);
}

int32_t _WF_std_io_Print_i8(int8_t value)
{
    return wio::runtime::std_io::PrintI8(value);
}

int32_t _WF_std_io_Print_i16(int16_t value)
{
    return wio::runtime::std_io::PrintI16(value);
}

int32_t _WF_std_io_Print_i32(int32_t value)
{
    return wio::runtime::std_io::PrintI32(value);
}

int32_t _WF_std_io_Print_i64(int64_t value)
{
    return wio::runtime::std_io::PrintI64(value);
}

int32_t _WF_std_io_Print_u8(uint8_t value)
{
    return wio::runtime::std_io::PrintU8(value);
}

int32_t _WF_std_io_Print_u16(uint16_t value)
{
    return wio::runtime::std_io::PrintU16(value);
}

int32_t _WF_std_io_Print_u32(uint32_t value)
{
    return wio::runtime::std_io::PrintU32(value);
}

int32_t _WF_std_io_Print_u64(uint64_t value)
{
    return wio::runtime::std_io::PrintU64(value);
}

int32_t _WF_std_io_Print_isize(ptrdiff_t value)
{
    return wio::runtime::std_io::PrintISize(value);
}

int32_t _WF_std_io_Print_usize(size_t value)
{
    return wio::runtime::std_io::PrintUSize(value);
}

int32_t _WF_std_io_Print_f32(float value)
{
    return wio::runtime::std_io::PrintF32(value);
}

int32_t _WF_std_io_Print_f64(double value)
{
    return wio::runtime::std_io::PrintF64(value);
}

int32_t _WF_std_io_PrintLine_string(wio::String value)
{
    _WF_std_io_Print_string(value);
    return _WF_std_io_Print_string("\n");
}

int32_t _WF_GetValue()
{
    return 42;
}

int main(int argc, char** argv) {
    wio::DArray<wio::String> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        args.push_back(wio::String(argv[i]));
    }
    
    try {
        _WF_std_io_Print_string(std::format("Value: {}", _WF_GetValue()));
        return 0;
    }
    catch (const wio::runtime::RuntimeException& ex)
    {
        std::cout << "Runtime Error: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
