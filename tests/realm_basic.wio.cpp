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


template <typename T>
int32_t _WF_std_io_Print_T(T value);


template <typename T>
int32_t _WF_std_io_PrintLine_T(T value);


int32_t _WF_alpha_Value();


int32_t _WF_beta_Value();


template <typename T>
int32_t _WF_std_io_Print_T(T value)
{
    return wio::runtime::std_io::WriteValue(value);
}

template int32_t _WF_std_io_Print_T<wio::String>(wio::String);
template int32_t _WF_std_io_Print_T<bool>(bool);
template int32_t _WF_std_io_Print_T<char>(char);
template int32_t _WF_std_io_Print_T<int8_t>(int8_t);
template int32_t _WF_std_io_Print_T<int16_t>(int16_t);
template int32_t _WF_std_io_Print_T<int32_t>(int32_t);
template int32_t _WF_std_io_Print_T<int64_t>(int64_t);
template int32_t _WF_std_io_Print_T<uint8_t>(uint8_t);
template int32_t _WF_std_io_Print_T<uint16_t>(uint16_t);
template int32_t _WF_std_io_Print_T<uint32_t>(uint32_t);
template int32_t _WF_std_io_Print_T<uint64_t>(uint64_t);
template int32_t _WF_std_io_Print_T<float>(float);
template int32_t _WF_std_io_Print_T<double>(double);

template <typename T>
int32_t _WF_std_io_PrintLine_T(T value)
{
    _WF_std_io_Print_T(value);
    _WF_std_io_Print_T("\n");
    return 0;
}

int32_t _WF_alpha_Value()
{
    return 1;
}

int32_t _WF_beta_Value()
{
    return 2;
}

int main(int argc, char** argv) {
    wio::DArray<wio::String> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        args.push_back(wio::String(argv[i]));
    }
    
    try {
        _WF_std_io_Print_T(std::format("Realm sum: {}", (_WF_alpha_Value() + _WF_beta_Value())));
        return 0;
    }
    catch (const wio::runtime::RuntimeException& ex)
    {
        std::cout << "Runtime Error: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
