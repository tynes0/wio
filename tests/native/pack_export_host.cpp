#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <module_api.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

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
        return LoadLibraryA(libraryPath);
#else
        return dlopen(libraryPath, RTLD_NOW);
#endif
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

    template <typename T>
    T loadSymbol(ModuleHandle moduleHandle, const char* symbolName)
    {
#if defined(_WIN32)
        auto* rawSymbol = GetProcAddress(moduleHandle, symbolName);
#else
        void* rawSymbol = dlsym(moduleHandle, symbolName);
#endif

        return reinterpret_cast<T>(rawSymbol);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Pack export host expected a library path." << '\n';
        return EXIT_FAILURE;
    }

    ModuleHandle moduleHandle = openModule(argv[1]);
    if (moduleHandle == nullptr)
    {
        std::cerr << "Failed to open pack export library." << '\n';
        return EXIT_FAILURE;
    }

    WioModuleGetApiFn moduleGetApi = loadSymbol<WioModuleGetApiFn>(moduleHandle, "WioModuleGetApi");
    const WioModuleApi* api = moduleGetApi ? moduleGetApi() : nullptr;
    if (api == nullptr || api->exportCount != 2)
    {
        std::cerr << "Pack export module API metadata is invalid." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    WioValue pairArgs[2]{};
    pairArgs[0].type = WIO_ABI_I32;
    pairArgs[0].value.v_i32 = 11;
    pairArgs[1].type = WIO_ABI_BOOL;
    pairArgs[1].value.v_bool = true;

    WioValue pairResult{};
    if (WioInvokeModuleExport(api, "CountExport<i32, bool>", pairArgs, 2, &pairResult) != WIO_INVOKE_OK ||
        pairResult.type != WIO_ABI_I32)
    {
        std::cerr << "Failed to invoke CountExport<i32, bool>." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    WioValue tripleArgs[3]{};
    tripleArgs[0].type = WIO_ABI_I32;
    tripleArgs[0].value.v_i32 = 1;
    tripleArgs[1].type = WIO_ABI_I32;
    tripleArgs[1].value.v_i32 = 2;
    tripleArgs[2].type = WIO_ABI_I32;
    tripleArgs[2].value.v_i32 = 3;

    WioValue tripleResult{};
    if (WioInvokeModuleExport(api, "CountExport<i32, i32, i32>", tripleArgs, 3, &tripleResult) != WIO_INVOKE_OK ||
        tripleResult.type != WIO_ABI_I32)
    {
        std::cerr << "Failed to invoke CountExport<i32, i32, i32>." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    std::cout << "Pack export instantiate: exports=" << api->exportCount
              << " pair=" << pairResult.value.v_i32
              << " triple=" << tripleResult.value.v_i32
              << '\n';

    closeModule(moduleHandle);
    return EXIT_SUCCESS;
}
