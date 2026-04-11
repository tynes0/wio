#include <cstdint>
#include <cstdlib>
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using ModuleApiVersionFn = std::uint32_t(*)();
using ModuleLoadFn = std::int32_t(*)();
using ModuleUpdateFn = void(*)(float);
using ModuleUnloadFn = void(*)();
using GetCounterFn = std::int32_t(*)();

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
        std::cerr << "Module lifecycle host expected a library path." << '\n';
        return EXIT_FAILURE;
    }

    ModuleHandle moduleHandle = openModule(argv[1]);
    if (moduleHandle == nullptr)
    {
        std::cerr << "Failed to open module library." << '\n';
        return EXIT_FAILURE;
    }

    ModuleApiVersionFn moduleApiVersion = loadSymbol<ModuleApiVersionFn>(moduleHandle, "WioModuleApiVersion");
    ModuleLoadFn moduleLoad = loadSymbol<ModuleLoadFn>(moduleHandle, "WioModuleLoad");
    ModuleUpdateFn moduleUpdate = loadSymbol<ModuleUpdateFn>(moduleHandle, "WioModuleUpdate");
    ModuleUnloadFn moduleUnload = loadSymbol<ModuleUnloadFn>(moduleHandle, "WioModuleUnload");
    GetCounterFn getCounter = loadSymbol<GetCounterFn>(moduleHandle, "WioGetCounter");

    if (moduleApiVersion == nullptr || moduleLoad == nullptr || moduleUpdate == nullptr || moduleUnload == nullptr || getCounter == nullptr)
    {
        std::cerr << "Failed to load one or more module symbols." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    std::uint32_t version = moduleApiVersion();
    std::int32_t loadResult = moduleLoad();
    moduleUpdate(2.0f);
    std::int32_t counter = getCounter();
    moduleUnload();

    std::cout << "Module lifecycle: v" << version << " load=" << loadResult << " counter=" << counter << '\n';
    closeModule(moduleHandle);
    return EXIT_SUCCESS;
}
