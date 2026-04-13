#include <cstdint>
#include <cstdlib>
#include <cstring>
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
        std::cerr << "Generic export host expected a library path." << '\n';
        return EXIT_FAILURE;
    }

    ModuleHandle moduleHandle = openModule(argv[1]);
    if (moduleHandle == nullptr)
    {
        std::cerr << "Failed to open generic export library." << '\n';
        return EXIT_FAILURE;
    }

    WioModuleGetApiFn moduleGetApi = loadSymbol<WioModuleGetApiFn>(moduleHandle, "WioModuleGetApi");
    const WioModuleApi* api = moduleGetApi ? moduleGetApi() : nullptr;
    if (api == nullptr ||
        api->descriptorVersion != WIO_MODULE_API_DESCRIPTOR_VERSION ||
        api->capabilities != 0 ||
        api->stateSchemaVersion != 0 ||
        api->apiVersion != nullptr ||
        api->load != nullptr ||
        api->update != nullptr ||
        api->saveState != nullptr ||
        api->restoreState != nullptr ||
        api->unload != nullptr ||
        api->exportCount != 2 ||
        api->commandCount != 0 ||
        api->eventHookCount != 0)
    {
        std::cerr << "Generic export module API metadata is invalid." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    const WioModuleExport* echoI32 = WioFindModuleExport(api, "Echo<i32>");
    const WioModuleExport* echoBool = WioFindModuleExport(api, "Echo<bool>");
    if (echoI32 == nullptr ||
        echoBool == nullptr ||
        echoI32->invoke == nullptr ||
        echoBool->invoke == nullptr ||
        echoI32->returnType != WIO_ABI_I32 ||
        echoBool->returnType != WIO_ABI_BOOL ||
        echoI32->parameterCount != 1 ||
        echoBool->parameterCount != 1 ||
        echoI32->parameterTypes == nullptr ||
        echoBool->parameterTypes == nullptr ||
        echoI32->parameterTypes[0] != WIO_ABI_I32 ||
        echoBool->parameterTypes[0] != WIO_ABI_BOOL)
    {
        std::cerr << "Failed to resolve generic export metadata." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    WioValue intArgs[1]{};
    intArgs[0].type = WIO_ABI_I32;
    intArgs[0].value.v_i32 = 41;

    WioValue intResult{};
    if (WioInvokeModuleExport(api, "Echo<i32>", intArgs, 1, &intResult) != WIO_INVOKE_OK ||
        intResult.type != WIO_ABI_I32)
    {
        std::cerr << "Failed to invoke Echo<i32>." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    WioValue boolArgs[1]{};
    boolArgs[0].type = WIO_ABI_BOOL;
    boolArgs[0].value.v_bool = true;

    WioValue boolResult{};
    if (WioInvokeModuleExport(api, "Echo<bool>", boolArgs, 1, &boolResult) != WIO_INVOKE_OK ||
        boolResult.type != WIO_ABI_BOOL)
    {
        std::cerr << "Failed to invoke Echo<bool>." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    std::cout << "Generic export instantiate: table=1 exports=" << api->exportCount
              << " i32=" << intResult.value.v_i32
              << " bool=" << (boolResult.value.v_bool ? "true" : "false")
              << '\n';

    closeModule(moduleHandle);
    return EXIT_SUCCESS;
}
