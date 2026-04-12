#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <module_api.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
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

    WioModuleGetApiFn moduleGetApi = loadSymbol<WioModuleGetApiFn>(moduleHandle, "WioModuleGetApi");
    GetCounterFn getCounter = loadSymbol<GetCounterFn>(moduleHandle, "WioGetCounter");
    const WioModuleApi* api = moduleGetApi ? moduleGetApi() : nullptr;

    if (api == nullptr ||
        api->descriptorVersion != WIO_MODULE_API_DESCRIPTOR_VERSION ||
        api->capabilities != (WIO_MODULE_CAP_API_VERSION | WIO_MODULE_CAP_LOAD | WIO_MODULE_CAP_UPDATE | WIO_MODULE_CAP_UNLOAD) ||
        api->stateSchemaVersion != 0 ||
        api->apiVersion == nullptr ||
        api->load == nullptr ||
        api->update == nullptr ||
        api->unload == nullptr ||
        api->saveState != nullptr ||
        api->restoreState != nullptr ||
        getCounter == nullptr)
    {
        std::cerr << "Failed to load one or more module symbols." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    std::uint32_t version = api->apiVersion();
    std::int32_t loadResult = api->load();
    api->update(2.0f);
    std::int32_t counter = getCounter();
    api->unload();

    std::cout << "Module lifecycle: table=1 caps=" << api->capabilities << " schema=" << api->stateSchemaVersion << " v" << version << " load=" << loadResult << " counter=" << counter << '\n';
    closeModule(moduleHandle);
    return EXIT_SUCCESS;
}
