#include "entry_args.h"

#if defined(_WIN32)
    #include <windows.h>
    #include <shellapi.h>
#endif

#include <string_view>

namespace wio::runtime
{
    namespace
    {
#if defined(_WIN32)
        struct LocalArgumentBlock final
        {
            wchar_t** value = nullptr;
            ~LocalArgumentBlock() { if (value != nullptr) LocalFree(value); }
        };

        std::string narrowEntryArgument(const std::wstring_view value)
        {
            if (value.empty()) return {};
            const int size = WideCharToMultiByte(
                CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                nullptr, 0, nullptr, nullptr);
            if (size <= 0) return {};
            std::string result(static_cast<std::size_t>(size), '\0');
            if (WideCharToMultiByte(
                    CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                    result.data(), size, nullptr, nullptr) <= 0)
                return {};
            return result;
        }
#endif
    }

    std::vector<std::string> CollectEntryArguments(const int argc, char* const argv[])
    {
#if defined(_WIN32)
        int wideCount = 0;
        wchar_t** wideArguments = CommandLineToArgvW(GetCommandLineW(), &wideCount);
        if (wideArguments != nullptr)
        {
            const LocalArgumentBlock argumentBlock{wideArguments};
            std::vector<std::string> result;
            result.reserve(static_cast<std::size_t>(wideCount));
            for (int index = 0; index < wideCount; ++index)
                result.push_back(narrowEntryArgument(wideArguments[index]));
            return result;
        }
#endif
        std::vector<std::string> result;
        result.reserve(argc > 0 ? static_cast<std::size_t>(argc) : 0u);
        for (int index = 0; index < argc; ++index)
            result.emplace_back(argv != nullptr && argv[index] != nullptr ? argv[index] : "");
        return result;
    }
}
