#include <cstdint>
#include <cstdlib>
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using AddNumbersFn = std::int32_t(*)(std::int32_t, std::int32_t);

namespace
{
#if defined(_WIN32)
    using ModuleHandle = HMODULE;
#else
    using ModuleHandle = void*;
#endif

    ModuleHandle openModule(const char* libraryPath)
    {
#if defined(_WIN32)
        ModuleHandle moduleHandle = LoadLibraryA(libraryPath);
        if (moduleHandle == nullptr)
        {
            std::cerr << "LoadLibrary failed for: " << libraryPath << '\n';
            return nullptr;
        }
#else
        ModuleHandle moduleHandle = dlopen(libraryPath, RTLD_NOW);
        if (moduleHandle == nullptr)
        {
            std::cerr << "dlopen failed for: " << libraryPath << '\n';
            return nullptr;
        }
#endif

        return moduleHandle;
    }

    void closeModule(ModuleHandle moduleHandle)
    {
        if (moduleHandle == nullptr)
            return;

#if defined(_WIN32)
        FreeLibrary(moduleHandle);
#else
        dlclose(moduleHandle);
#endif
    }

    AddNumbersFn loadAddNumbersSymbol(ModuleHandle moduleHandle)
    {
#if defined(_WIN32)
        auto* rawSymbol = GetProcAddress(moduleHandle, "WioAddNumbers");
#else
        void* rawSymbol = dlsym(moduleHandle, "WioAddNumbers");
#endif

        if (rawSymbol == nullptr)
        {
#if defined(_WIN32)
            std::cerr << "GetProcAddress failed for symbol: WioAddNumbers" << '\n';
#else
            std::cerr << "dlsym failed for symbol: WioAddNumbers" << '\n';
#endif
            return nullptr;
        }

        return reinterpret_cast<AddNumbersFn>(rawSymbol);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Shared host expected a library path." << '\n';
        return EXIT_FAILURE;
    }

    ModuleHandle moduleHandle = openModule(argv[1]);
    if (moduleHandle == nullptr)
        return EXIT_FAILURE;

    AddNumbersFn addNumbers = loadAddNumbersSymbol(moduleHandle);
    if (addNumbers == nullptr)
    {
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    std::cout << "Shared export sum: " << addNumbers(19, 23) << '\n';
    closeModule(moduleHandle);
    return EXIT_SUCCESS;
}
