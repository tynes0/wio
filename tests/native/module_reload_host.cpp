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
using ModuleSaveStateFn = std::int32_t(*)();
using ModuleRestoreStateFn = std::int32_t(*)(std::int32_t);
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

    ModuleApiVersionFn apiVersionA = loadSymbol<ModuleApiVersionFn>(moduleA, "WioModuleApiVersion");
    ModuleLoadFn moduleLoadA = loadSymbol<ModuleLoadFn>(moduleA, "WioModuleLoad");
    ModuleUpdateFn moduleUpdateA = loadSymbol<ModuleUpdateFn>(moduleA, "WioModuleUpdate");
    ModuleSaveStateFn moduleSaveStateA = loadSymbol<ModuleSaveStateFn>(moduleA, "WioModuleSaveState");
    ModuleUnloadFn moduleUnloadA = loadSymbol<ModuleUnloadFn>(moduleA, "WioModuleUnload");
    GetCounterFn getCounterA = loadSymbol<GetCounterFn>(moduleA, "WioGetCounter");

    if (apiVersionA == nullptr || moduleLoadA == nullptr || moduleUpdateA == nullptr || moduleSaveStateA == nullptr || moduleUnloadA == nullptr || getCounterA == nullptr)
    {
        std::cerr << "Failed to load one or more symbols from the first module." << '\n';
        closeModule(moduleA);
        return EXIT_FAILURE;
    }

    std::uint32_t version = apiVersionA();
    std::int32_t loadResultA = moduleLoadA();
    moduleUpdateA(2.0f);
    std::int32_t snapshot = moduleSaveStateA();
    std::int32_t counterBeforeReload = getCounterA();
    moduleUnloadA();
    closeModule(moduleA);

    ModuleHandle moduleB = openModule(argv[2]);
    if (moduleB == nullptr)
    {
        std::cerr << "Failed to open second module library." << '\n';
        return EXIT_FAILURE;
    }

    ModuleRestoreStateFn moduleRestoreStateB = loadSymbol<ModuleRestoreStateFn>(moduleB, "WioModuleRestoreState");
    ModuleUpdateFn moduleUpdateB = loadSymbol<ModuleUpdateFn>(moduleB, "WioModuleUpdate");
    GetCounterFn getCounterB = loadSymbol<GetCounterFn>(moduleB, "WioGetCounter");

    if (moduleRestoreStateB == nullptr || moduleUpdateB == nullptr || getCounterB == nullptr)
    {
        std::cerr << "Failed to load one or more symbols from the second module." << '\n';
        closeModule(moduleB);
        return EXIT_FAILURE;
    }

    std::int32_t restoreResult = moduleRestoreStateB(snapshot);
    std::int32_t counterAfterRestore = getCounterB();
    moduleUpdateB(3.0f);
    std::int32_t counterAfterRetick = getCounterB();

    std::cout
        << "Module reload: v" << version
        << " load=" << loadResultA
        << " restore=" << restoreResult
        << " before=" << counterBeforeReload
        << " after=" << counterAfterRestore
        << " retick=" << counterAfterRetick
        << '\n';

    closeModule(moduleB);
    return EXIT_SUCCESS;
}
