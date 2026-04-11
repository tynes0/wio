#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <format>
#include <iostream>
#include <functional>
#include <map>
#include <unordered_map>
#include <exception.h>
#include <ref.h>

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

#include "io.h"

int32_t _WF_GetValue();


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
        wio::runtime::io::bPrint(std::format("Value: {}", _WF_GetValue()));
        return 0;
    }
    catch (const wio::runtime::RuntimeException& ex)
    {
        std::cout << "Runtime Error: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
