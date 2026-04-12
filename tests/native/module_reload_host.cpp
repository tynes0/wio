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
        apiA->exportCount != 2)
    {
        std::cerr << "Failed to load one or more API entries from the first module." << '\n';
        closeModule(moduleA);
        return EXIT_FAILURE;
    }

    const WioModuleExport* counterExportA = WioFindModuleExport(apiA, "GetCounter");
    const WioModuleExport* addExportA = WioFindModuleExport(apiA, "AddToCounter");
    if (!expectI32Export(counterExportA, "WioGetCounter", 0) ||
        !expectI32Export(addExportA, "WioAddToCounter", 1) ||
        addExportA->parameterTypes[0] != WIO_ABI_I32)
    {
        std::cerr << "Failed to resolve export metadata from the first module." << '\n';
        closeModule(moduleA);
        return EXIT_FAILURE;
    }

    std::uint32_t version = apiA->apiVersion();
    std::int32_t loadResultA = apiA->load();
    apiA->update(2.0f);
    std::int32_t snapshot = apiA->saveState();
    std::int32_t counterBeforeReload = 0;
    if (!invokeI32Export(apiA, "GetCounter", nullptr, 0, counterBeforeReload))
    {
        std::cerr << "Failed to invoke GetCounter from the first module." << '\n';
        closeModule(moduleA);
        return EXIT_FAILURE;
    }
    apiA->unload();
    closeModule(moduleA);

    ModuleHandle moduleB = openModule(argv[2]);
    if (moduleB == nullptr)
    {
        std::cerr << "Failed to open second module library." << '\n';
        return EXIT_FAILURE;
    }

    WioModuleGetApiFn moduleGetApiB = loadSymbol<WioModuleGetApiFn>(moduleB, "WioModuleGetApi");
    const WioModuleApi* apiB = moduleGetApiB ? moduleGetApiB() : nullptr;

    if (apiB == nullptr ||
        apiB->descriptorVersion != WIO_MODULE_API_DESCRIPTOR_VERSION ||
        apiB->capabilities != (WIO_MODULE_CAP_API_VERSION | WIO_MODULE_CAP_LOAD | WIO_MODULE_CAP_UPDATE | WIO_MODULE_CAP_UNLOAD | WIO_MODULE_CAP_SAVE_STATE | WIO_MODULE_CAP_RESTORE_STATE) ||
        apiB->stateSchemaVersion != 1 ||
        apiB->restoreState == nullptr ||
        apiB->update == nullptr ||
        apiB->unload == nullptr ||
        apiB->exportCount != 2)
    {
        std::cerr << "Failed to load one or more API entries from the second module." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    const WioModuleExport* counterExportB = WioFindModuleExport(apiB, "GetCounter");
    const WioModuleExport* addExportB = WioFindModuleExport(apiB, "AddToCounter");
    if (!expectI32Export(counterExportB, "WioGetCounter", 0) ||
        !expectI32Export(addExportB, "WioAddToCounter", 1) ||
        addExportB->parameterTypes[0] != WIO_ABI_I32)
    {
        std::cerr << "Failed to resolve export metadata from the second module." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    std::int32_t restoreResult = apiB->restoreState(snapshot);
    std::int32_t counterAfterRestore = 0;
    if (!invokeI32Export(apiB, "GetCounter", nullptr, 0, counterAfterRestore))
    {
        std::cerr << "Failed to invoke GetCounter from the second module." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    WioValue addArgs[1]{};
    addArgs[0].type = WIO_ABI_I32;
    addArgs[0].value.v_i32 = 3;

    std::int32_t addResult = 0;
    if (!invokeI32Export(apiB, "AddToCounter", addArgs, 1, addResult))
    {
        std::cerr << "Failed to invoke AddToCounter from the second module." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    apiB->unload();

    std::cout
        << "Module reload: table=1 caps=" << apiA->capabilities << " schema=" << apiA->stateSchemaVersion << " exports=" << apiA->exportCount << " v" << version
        << " load=" << loadResultA
        << " restore=" << restoreResult
        << " before=" << counterBeforeReload
        << " after=" << counterAfterRestore
        << " add=" << addResult
        << '\n';

    closeModule(moduleB);
    return EXIT_SUCCESS;
}
