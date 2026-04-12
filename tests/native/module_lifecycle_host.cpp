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

    bool invokeI32Command(const WioModuleApi* api, const char* commandName, const WioValue* args, std::uint32_t argCount, std::int32_t& outValue)
    {
        WioValue result{};
        const std::int32_t status = WioInvokeModuleCommand(api, commandName, args, argCount, &result);
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
        api->exportCount != 3 ||
        api->commandCount != 2 ||
        api->eventHookCount != 1)
    {
        std::cerr << "Failed to load one or more module API entries." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    const WioModuleCommand* getCommand = WioFindModuleCommand(api, "counter.get");
    const WioModuleCommand* addCommand = WioFindModuleCommand(api, "counter.add");
    const WioModuleEventHook* tickHook = WioFindModuleEventHook(api, "ApplyScriptTick");
    if (getCommand == nullptr ||
        addCommand == nullptr ||
        tickHook == nullptr ||
        !expectI32Export(getCommand->exportEntry, "WioGetCounter", 0) ||
        !expectI32Export(addCommand->exportEntry, "WioAddToCounter", 1) ||
        addCommand->exportEntry->parameterTypes[0] != WIO_ABI_I32 ||
        tickHook->exportEntry == nullptr ||
        std::strcmp(tickHook->eventName, "game.tick") != 0 ||
        std::strcmp(tickHook->exportEntry->symbolName, "WioApplyScriptTick") != 0 ||
        tickHook->exportEntry->returnType != WIO_ABI_VOID ||
        tickHook->exportEntry->parameterCount != 1 ||
        tickHook->exportEntry->parameterTypes == nullptr ||
        tickHook->exportEntry->parameterTypes[0] != WIO_ABI_F32)
    {
        std::cerr << "Failed to resolve command or event hook metadata." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    std::uint32_t version = api->apiVersion();
    std::int32_t loadResult = api->load();
    api->update(2.0f);

    WioValue tickArgs[1]{};
    tickArgs[0].type = WIO_ABI_F32;
    tickArgs[0].value.v_f32 = 5.0f;
    if (WioInvokeModuleEventHook(api, "ApplyScriptTick", tickArgs, 1, nullptr) != WIO_INVOKE_OK)
    {
        std::cerr << "Failed to invoke ApplyScriptTick through the module event registry." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    std::int32_t counter = 0;
    if (!invokeI32Command(api, "counter.get", nullptr, 0, counter))
    {
        std::cerr << "Failed to invoke counter.get through the module command registry." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    WioValue addArgs[1]{};
    addArgs[0].type = WIO_ABI_I32;
    addArgs[0].value.v_i32 = 3;

    std::int32_t add = 0;
    if (!invokeI32Command(api, "counter.add", addArgs, 1, add))
    {
        std::cerr << "Failed to invoke counter.add through the module command registry." << '\n';
        closeModule(moduleHandle);
        return EXIT_FAILURE;
    }

    api->unload();

    std::cout << "Module lifecycle: table=1 caps=" << api->capabilities << " schema=" << api->stateSchemaVersion << " exports=" << api->exportCount << " commands=" << api->commandCount << " events=" << api->eventHookCount << " v" << version << " load=" << loadResult << " counter=" << counter << " add=" << add << '\n';
    closeModule(moduleHandle);
    return EXIT_SUCCESS;
}
