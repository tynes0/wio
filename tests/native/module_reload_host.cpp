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
    if (argc < 3)
    {
        std::cerr << "Module reload host expected two library paths." << '\n';
        return EXIT_FAILURE;
    }

    ModuleHandle moduleA = openModule(argv[1]);
    if (moduleA == nullptr)
    {
        std::cerr << "Failed to open first module library." << '\n';
        return EXIT_FAILURE;
    }

    WioModuleGetApiFn moduleGetApiA = loadSymbol<WioModuleGetApiFn>(moduleA, "WioModuleGetApi");
    GetCounterFn getCounterA = loadSymbol<GetCounterFn>(moduleA, "WioGetCounter");
    const WioModuleApi* apiA = moduleGetApiA ? moduleGetApiA() : nullptr;

    if (apiA == nullptr ||
        apiA->descriptorVersion != WIO_MODULE_API_DESCRIPTOR_VERSION ||
        apiA->capabilities != (WIO_MODULE_CAP_API_VERSION | WIO_MODULE_CAP_LOAD | WIO_MODULE_CAP_UPDATE | WIO_MODULE_CAP_UNLOAD | WIO_MODULE_CAP_SAVE_STATE | WIO_MODULE_CAP_RESTORE_STATE) ||
        apiA->stateSchemaVersion != 1 ||
        apiA->apiVersion == nullptr ||
        apiA->load == nullptr ||
        apiA->update == nullptr ||
        apiA->saveState == nullptr ||
        apiA->restoreState == nullptr ||
        apiA->unload == nullptr ||
        getCounterA == nullptr)
    {
        std::cerr << "Failed to load one or more symbols from the first module." << '\n';
        closeModule(moduleA);
        return EXIT_FAILURE;
    }

    std::uint32_t version = apiA->apiVersion();
    std::int32_t loadResultA = apiA->load();
    apiA->update(2.0f);
    std::int32_t snapshot = apiA->saveState();
    std::int32_t counterBeforeReload = getCounterA();
    apiA->unload();
    closeModule(moduleA);

    ModuleHandle moduleB = openModule(argv[2]);
    if (moduleB == nullptr)
    {
        std::cerr << "Failed to open second module library." << '\n';
        return EXIT_FAILURE;
    }

    WioModuleGetApiFn moduleGetApiB = loadSymbol<WioModuleGetApiFn>(moduleB, "WioModuleGetApi");
    GetCounterFn getCounterB = loadSymbol<GetCounterFn>(moduleB, "WioGetCounter");
    const WioModuleApi* apiB = moduleGetApiB ? moduleGetApiB() : nullptr;

    if (apiB == nullptr ||
        apiB->descriptorVersion != WIO_MODULE_API_DESCRIPTOR_VERSION ||
        apiB->capabilities != (WIO_MODULE_CAP_API_VERSION | WIO_MODULE_CAP_LOAD | WIO_MODULE_CAP_UPDATE | WIO_MODULE_CAP_UNLOAD | WIO_MODULE_CAP_SAVE_STATE | WIO_MODULE_CAP_RESTORE_STATE) ||
        apiB->stateSchemaVersion != 1 ||
        apiB->restoreState == nullptr ||
        apiB->update == nullptr ||
        getCounterB == nullptr)
    {
        std::cerr << "Failed to load one or more symbols from the second module." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    std::int32_t restoreResult = apiB->restoreState(snapshot);
    std::int32_t counterAfterRestore = getCounterB();
    apiB->update(3.0f);
    std::int32_t counterAfterRetick = getCounterB();

    std::cout
        << "Module reload: table=1 caps=" << apiA->capabilities << " schema=" << apiA->stateSchemaVersion << " v" << version
        << " load=" << loadResultA
        << " restore=" << restoreResult
        << " before=" << counterBeforeReload
        << " after=" << counterAfterRestore
        << " retick=" << counterAfterRetick
        << '\n';

    closeModule(moduleB);
    return EXIT_SUCCESS;
}
