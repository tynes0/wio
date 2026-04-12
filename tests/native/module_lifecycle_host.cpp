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

    bool expectI32Export(const WioModuleExport* exportEntry, const char* symbolName, std::uint32_t parameterCount)
    {
        if (exportEntry == nullptr ||
            std::strcmp(exportEntry->symbolName, symbolName) != 0 ||
            exportEntry->returnType != WIO_ABI_I32 ||
            exportEntry->parameterCount != parameterCount ||
            exportEntry->invoke == nullptr)
        {
            return false;
        }

        if (parameterCount == 0)
            return exportEntry->parameterTypes == nullptr;

        return exportEntry->parameterTypes != nullptr;
    }

    bool invokeI32Export(const WioModuleApi* api, const char* logicalName, const WioValue* args, std::uint32_t argCount, std::int32_t& outValue)
    {
        WioValue result{};
        const std::int32_t status = WioInvokeModuleExport(api, logicalName, args, argCount, &result);
        if (status != WIO_INVOKE_OK || result.type != WIO_ABI_I32)
            return false;

        outValue = result.value.v_i32;
        return true;
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
        api->exportCount != 2)
    {
        std::cerr << "Failed to load one or more module API entries." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    const WioModuleExport* counterExport = WioFindModuleExport(api, "GetCounter");
    const WioModuleExport* addExport = WioFindModuleExport(api, "AddToCounter");
    if (!expectI32Export(counterExport, "WioGetCounter", 0) ||
        !expectI32Export(addExport, "WioAddToCounter", 1) ||
        addExport->parameterTypes[0] != WIO_ABI_I32)
    {
        std::cerr << "Failed to resolve exported function metadata." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    std::uint32_t version = api->apiVersion();
    std::int32_t loadResult = api->load();
    api->update(2.0f);

    std::int32_t counter = 0;
    if (!invokeI32Export(api, "GetCounter", nullptr, 0, counter))
    {
        std::cerr << "Failed to invoke GetCounter through the module export registry." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    WioValue addArgs[1]{};
    addArgs[0].type = WIO_ABI_I32;
    addArgs[0].value.v_i32 = 3;

    std::int32_t add = 0;
    if (!invokeI32Export(api, "AddToCounter", addArgs, 1, add))
    {
        std::cerr << "Failed to invoke AddToCounter through the module export registry." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    api->unload();

    std::cout << "Module lifecycle: table=1 caps=" << api->capabilities << " schema=" << api->stateSchemaVersion << " exports=" << api->exportCount << " v" << version << " load=" << loadResult << " counter=" << counter << " add=" << add << '\n';
    closeModule(moduleHandle);
    return EXIT_SUCCESS;
}
