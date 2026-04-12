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

    struct ArenaState
    {
        std::int32_t health = 0;
        std::int32_t shield = 0;
        std::int32_t rage = 0;
        std::int32_t phase = 0;
        std::int32_t pulse = 0;
    };

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

    bool queryArenaState(const WioModuleApi* api, ArenaState& outState)
    {
        return invokeI32Command(api, "arena.hp", nullptr, 0, outState.health) &&
               invokeI32Command(api, "arena.shield", nullptr, 0, outState.shield) &&
               invokeI32Command(api, "arena.rage", nullptr, 0, outState.rage) &&
               invokeI32Command(api, "arena.phase", nullptr, 0, outState.phase) &&
               invokeI32Command(api, "arena.pulse", nullptr, 0, outState.pulse);
    }

    bool performHit(const WioModuleApi* api, std::int32_t baseDamage, std::int32_t& outHealth)
    {
        WioValue args[1]{};
        args[0].type = WIO_ABI_I32;
        args[0].value.v_i32 = baseDamage;
        return invokeI32Command(api, "arena.hit", args, 1, outHealth);
    }

    bool broadcastTick(const WioModuleApi* api, float deltaTime)
    {
        WioValue args[1]{};
        args[0].type = WIO_ABI_F32;
        args[0].value.v_f32 = deltaTime;
        return WioBroadcastModuleEvent(api, "arena.tick", args, 1) == WIO_INVOKE_OK;
    }

    bool validateArenaApi(const WioModuleApi* api)
    {
        if (api == nullptr ||
            api->descriptorVersion != WIO_MODULE_API_DESCRIPTOR_VERSION ||
            api->capabilities != (WIO_MODULE_CAP_API_VERSION | WIO_MODULE_CAP_LOAD | WIO_MODULE_CAP_UPDATE | WIO_MODULE_CAP_UNLOAD | WIO_MODULE_CAP_SAVE_STATE | WIO_MODULE_CAP_RESTORE_STATE) ||
            api->stateSchemaVersion != 1 ||
            api->apiVersion == nullptr ||
            api->load == nullptr ||
            api->update == nullptr ||
            api->saveState == nullptr ||
            api->restoreState == nullptr ||
            api->unload == nullptr ||
            api->exportCount != 10 ||
            api->commandCount != 8 ||
            api->eventHookCount != 2)
        {
            return false;
        }

        const WioModuleCommand* hpCommand = WioFindModuleCommand(api, "arena.hp");
        const WioModuleCommand* shieldCommand = WioFindModuleCommand(api, "arena.shield");
        const WioModuleCommand* rageCommand = WioFindModuleCommand(api, "arena.rage");
        const WioModuleCommand* phaseCommand = WioFindModuleCommand(api, "arena.phase");
        const WioModuleCommand* pulseCommand = WioFindModuleCommand(api, "arena.pulse");
        const WioModuleCommand* hitCommand = WioFindModuleCommand(api, "arena.hit");
        const WioModuleCommand* threatCommand = WioFindModuleCommand(api, "arena.threat");
        const WioModuleCommand* aliveCommand = WioFindModuleCommand(api, "arena.alive");

        const WioModuleEventHook* tickCoreHook = WioFindModuleEventHook(api, "OnArenaTickCore");
        const WioModuleEventHook* tickTelemetryHook = WioFindModuleEventHook(api, "OnArenaTickTelemetry");
        const WioModuleEventHook* firstTickHook = WioFindFirstModuleEventHookForEvent(api, "arena.tick");

        return hpCommand != nullptr &&
               shieldCommand != nullptr &&
               rageCommand != nullptr &&
               phaseCommand != nullptr &&
               pulseCommand != nullptr &&
               hitCommand != nullptr &&
               threatCommand != nullptr &&
               aliveCommand != nullptr &&
               tickCoreHook != nullptr &&
               tickTelemetryHook != nullptr &&
               firstTickHook != nullptr &&
               WioCountModuleEventHooksForEvent(api, "arena.tick") == 2 &&
               expectI32Export(hpCommand->exportEntry, "WioArenaGetHealth", 0) &&
               expectI32Export(shieldCommand->exportEntry, "WioArenaGetShield", 0) &&
               expectI32Export(rageCommand->exportEntry, "WioArenaGetRage", 0) &&
               expectI32Export(phaseCommand->exportEntry, "WioArenaGetPhase", 0) &&
               expectI32Export(pulseCommand->exportEntry, "WioArenaGetPulse", 0) &&
               expectI32Export(hitCommand->exportEntry, "WioArenaHit", 1) &&
               expectI32Export(threatCommand->exportEntry, "WioArenaGetThreat", 0) &&
               expectI32Export(aliveCommand->exportEntry, "WioArenaIsAlive", 0) &&
               hitCommand->exportEntry->parameterTypes != nullptr &&
               hitCommand->exportEntry->parameterTypes[0] == WIO_ABI_I32 &&
               std::strcmp(firstTickHook->hookName, "OnArenaTickCore") == 0 &&
               std::strcmp(tickCoreHook->eventName, "arena.tick") == 0 &&
               std::strcmp(tickTelemetryHook->eventName, "arena.tick") == 0 &&
               expectVoidExport(tickCoreHook->exportEntry, "WioArenaTickCore", 1) &&
               expectVoidExport(tickTelemetryHook->exportEntry, "WioArenaTickTelemetry", 1) &&
               tickCoreHook->exportEntry->parameterTypes[0] == WIO_ABI_F32 &&
               tickTelemetryHook->exportEntry->parameterTypes[0] == WIO_ABI_F32;
    }
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "Hybrid arena host expected two module library paths." << '\n';
        return EXIT_FAILURE;
    }

    ModuleHandle moduleA = openModule(argv[1]);
    if (moduleA == nullptr)
    {
        std::cerr << "Failed to open the first hybrid arena module." << '\n';
        return EXIT_FAILURE;
    }

    WioModuleGetApiFn moduleGetApiA = loadSymbol<WioModuleGetApiFn>(moduleA, "WioModuleGetApi");
    const WioModuleApi* apiA = moduleGetApiA ? moduleGetApiA() : nullptr;
    if (!validateArenaApi(apiA))
    {
        std::cerr << "First hybrid arena module metadata validation failed." << '\n';
        closeModule(moduleA);
        return EXIT_FAILURE;
    }

    const std::uint32_t version = apiA->apiVersion();
    const std::int32_t loadResultA = apiA->load();
    apiA->update(0.0f);

    ArenaState startState{};
    if (!queryArenaState(apiA, startState))
    {
        std::cerr << "Failed to query initial hybrid arena state." << '\n';
        closeModule(moduleA);
        return EXIT_FAILURE;
    }

    if (!broadcastTick(apiA, 1.5f))
    {
        std::cerr << "Failed to broadcast the first arena.tick event." << '\n';
        closeModule(moduleA);
        return EXIT_FAILURE;
    }

    ArenaState afterTickA{};
    if (!queryArenaState(apiA, afterTickA))
    {
        std::cerr << "Failed to query hybrid arena state after the first tick." << '\n';
        closeModule(moduleA);
        return EXIT_FAILURE;
    }

    std::int32_t afterHitAHealth = 0;
    if (!performHit(apiA, 60, afterHitAHealth))
    {
        std::cerr << "Failed to invoke arena.hit on the first module." << '\n';
        closeModule(moduleA);
        return EXIT_FAILURE;
    }

    ArenaState afterHitA{};
    if (!queryArenaState(apiA, afterHitA))
    {
        std::cerr << "Failed to query hybrid arena state after the first hit." << '\n';
        closeModule(moduleA);
        return EXIT_FAILURE;
    }

    const std::int32_t packedSnapshot = apiA->saveState();
    apiA->unload();
    closeModule(moduleA);

    ModuleHandle moduleB = openModule(argv[2]);
    if (moduleB == nullptr)
    {
        std::cerr << "Failed to open the second hybrid arena module." << '\n';
        return EXIT_FAILURE;
    }

    WioModuleGetApiFn moduleGetApiB = loadSymbol<WioModuleGetApiFn>(moduleB, "WioModuleGetApi");
    const WioModuleApi* apiB = moduleGetApiB ? moduleGetApiB() : nullptr;
    if (!validateArenaApi(apiB))
    {
        std::cerr << "Second hybrid arena module metadata validation failed." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    const std::int32_t restoreResult = apiB->restoreState(packedSnapshot);

    ArenaState restoredState{};
    if (!queryArenaState(apiB, restoredState))
    {
        std::cerr << "Failed to query restored hybrid arena state." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    if (restoredState.health != afterHitA.health ||
        restoredState.shield != afterHitA.shield ||
        restoredState.rage != afterHitA.rage ||
        restoredState.phase != afterHitA.phase ||
        restoredState.pulse != afterHitA.pulse)
    {
        std::cerr << "Restored hybrid arena state did not match the saved state." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    if (!broadcastTick(apiB, 2.25f))
    {
        std::cerr << "Failed to broadcast the second arena.tick event." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    ArenaState afterTickB{};
    if (!queryArenaState(apiB, afterTickB))
    {
        std::cerr << "Failed to query hybrid arena state after the second tick." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    std::int32_t finalHealth = 0;
    if (!performHit(apiB, 110, finalHealth))
    {
        std::cerr << "Failed to invoke arena.hit on the second module." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    ArenaState finalState{};
    if (!queryArenaState(apiB, finalState))
    {
        std::cerr << "Failed to query final hybrid arena state." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    std::int32_t threat = 0;
    std::int32_t alive = 0;
    if (!invokeI32Command(apiB, "arena.threat", nullptr, 0, threat) ||
        !invokeI32Command(apiB, "arena.alive", nullptr, 0, alive))
    {
        std::cerr << "Failed to query final hybrid arena command outputs." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    apiB->unload();

    std::cout
        << "Hybrid arena summary: start=" << startState.health << "/" << startState.shield << "/" << startState.rage << "/" << startState.phase
        << " afterTickA=" << afterTickA.health << "/" << afterTickA.shield << "/" << afterTickA.rage << "/" << afterTickA.phase
        << " pulseA=" << afterTickA.pulse
        << " afterHitA=" << afterHitAHealth
        << " restore=" << restoreResult
        << " afterTickB=" << afterTickB.health << "/" << afterTickB.shield << "/" << afterTickB.rage << "/" << afterTickB.phase
        << " pulseB=" << afterTickB.pulse
        << " final=" << finalHealth << "/" << finalState.shield << "/" << finalState.rage << "/" << finalState.phase
        << " threat=" << threat
        << " alive=" << alive
        << " v" << version
        << " load=" << loadResultA
        << '\n';

    closeModule(moduleB);
    return EXIT_SUCCESS;
}
