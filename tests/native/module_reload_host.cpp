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

    bool expectVoidExport(const WioModuleExport* exportEntry, const char* symbolName, std::uint32_t parameterCount)
    {
        if (exportEntry == nullptr ||
            std::strcmp(exportEntry->symbolName, symbolName) != 0 ||
            exportEntry->returnType != WIO_ABI_VOID ||
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
        apiA->exportCount != 4 ||
        apiA->commandCount != 2 ||
        apiA->eventHookCount != 2)
    {
        std::cerr << "Failed to load one or more API entries from the first module." << '\n';
        closeModule(moduleA);
        return EXIT_FAILURE;
    }

    const WioModuleCommand* getCommandA = WioFindModuleCommand(apiA, "counter.get");
    const WioModuleCommand* addCommandA = WioFindModuleCommand(apiA, "counter.add");
    const WioModuleEventHook* tickHookA = WioFindFirstModuleEventHookForEvent(apiA, "game.tick");
    const WioModuleEventHook* tickBonusHookA = WioFindModuleEventHook(apiA, "ApplyScriptTickBonus");
    if (getCommandA == nullptr ||
        addCommandA == nullptr ||
        tickHookA == nullptr ||
        tickBonusHookA == nullptr ||
        WioCountModuleEventHooksForEvent(apiA, "game.tick") != 2 ||
        !expectI32Export(getCommandA->exportEntry, "WioGetCounter", 0) ||
        !expectI32Export(addCommandA->exportEntry, "WioAddToCounter", 1) ||
        addCommandA->exportEntry->parameterTypes[0] != WIO_ABI_I32 ||
        tickHookA->exportEntry == nullptr ||
        tickBonusHookA->exportEntry == nullptr ||
        std::strcmp(tickHookA->hookName, "ApplyScriptTick") != 0 ||
        std::strcmp(tickHookA->exportEntry->symbolName, "WioApplyScriptTick") != 0 ||
        std::strcmp(tickBonusHookA->exportEntry->symbolName, "WioApplyScriptTickBonus") != 0 ||
        !expectVoidExport(tickHookA->exportEntry, "WioApplyScriptTick", 1) ||
        !expectVoidExport(tickBonusHookA->exportEntry, "WioApplyScriptTickBonus", 1) ||
        tickHookA->exportEntry->parameterTypes[0] != WIO_ABI_F32 ||
        tickBonusHookA->exportEntry->parameterTypes[0] != WIO_ABI_F32)
    {
        std::cerr << "Failed to resolve command or event metadata from the first module." << '\n';
        closeModule(moduleA);
        return EXIT_FAILURE;
    }

    std::uint32_t version = apiA->apiVersion();
    std::int32_t loadResultA = apiA->load();
    apiA->update(2.0f);
    std::int32_t snapshot = apiA->saveState();
    std::int32_t counterBeforeReload = 0;
    if (!invokeI32Command(apiA, "counter.get", nullptr, 0, counterBeforeReload))
    {
        std::cerr << "Failed to invoke counter.get from the first module." << '\n';
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
        apiB->exportCount != 4 ||
        apiB->commandCount != 2 ||
        apiB->eventHookCount != 2)
    {
        std::cerr << "Failed to load one or more API entries from the second module." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    const WioModuleCommand* getCommandB = WioFindModuleCommand(apiB, "counter.get");
    const WioModuleCommand* addCommandB = WioFindModuleCommand(apiB, "counter.add");
    const WioModuleEventHook* tickHookB = WioFindFirstModuleEventHookForEvent(apiB, "game.tick");
    const WioModuleEventHook* tickBonusHookB = WioFindModuleEventHook(apiB, "ApplyScriptTickBonus");
    if (getCommandB == nullptr ||
        addCommandB == nullptr ||
        tickHookB == nullptr ||
        tickBonusHookB == nullptr ||
        WioCountModuleEventHooksForEvent(apiB, "game.tick") != 2 ||
        !expectI32Export(getCommandB->exportEntry, "WioGetCounter", 0) ||
        !expectI32Export(addCommandB->exportEntry, "WioAddToCounter", 1) ||
        addCommandB->exportEntry->parameterTypes[0] != WIO_ABI_I32 ||
        tickHookB->exportEntry == nullptr ||
        tickBonusHookB->exportEntry == nullptr ||
        std::strcmp(tickHookB->hookName, "ApplyScriptTick") != 0 ||
        std::strcmp(tickHookB->exportEntry->symbolName, "WioApplyScriptTick") != 0 ||
        std::strcmp(tickBonusHookB->exportEntry->symbolName, "WioApplyScriptTickBonus") != 0 ||
        !expectVoidExport(tickHookB->exportEntry, "WioApplyScriptTick", 1) ||
        !expectVoidExport(tickBonusHookB->exportEntry, "WioApplyScriptTickBonus", 1) ||
        tickHookB->exportEntry->parameterTypes[0] != WIO_ABI_F32 ||
        tickBonusHookB->exportEntry->parameterTypes[0] != WIO_ABI_F32)
    {
        std::cerr << "Failed to resolve command or event metadata from the second module." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    std::int32_t restoreResult = apiB->restoreState(snapshot);
    std::int32_t counterAfterRestore = 0;
    if (!invokeI32Command(apiB, "counter.get", nullptr, 0, counterAfterRestore))
    {
        std::cerr << "Failed to invoke counter.get from the second module." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    WioValue tickArgs[1]{};
    tickArgs[0].type = WIO_ABI_F32;
    tickArgs[0].value.v_f32 = 5.0f;
    if (WioBroadcastModuleEvent(apiB, "game.tick", tickArgs, 1) != WIO_INVOKE_OK)
    {
        std::cerr << "Failed to broadcast game.tick from the second module." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    std::int32_t counterAfterTick = 0;
    if (!invokeI32Command(apiB, "counter.get", nullptr, 0, counterAfterTick))
    {
        std::cerr << "Failed to read counter.get after ApplyScriptTick." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    WioValue addArgs[1]{};
    addArgs[0].type = WIO_ABI_I32;
    addArgs[0].value.v_i32 = 3;

    std::int32_t addResult = 0;
    if (!invokeI32Command(apiB, "counter.add", addArgs, 1, addResult))
    {
        std::cerr << "Failed to invoke counter.add from the second module." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    apiB->unload();

    std::cout
        << "Module reload: table=1 caps=" << apiA->capabilities << " schema=" << apiA->stateSchemaVersion << " exports=" << apiA->exportCount << " commands=" << apiA->commandCount << " events=" << apiA->eventHookCount << " v" << version
        << " load=" << loadResultA
        << " restore=" << restoreResult
        << " before=" << counterBeforeReload
        << " after=" << counterAfterRestore
        << " tick=" << counterAfterTick
        << " add=" << addResult
        << '\n';

    closeModule(moduleB);
    return EXIT_SUCCESS;
}
