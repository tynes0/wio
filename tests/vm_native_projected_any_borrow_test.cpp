#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include <wio_native_any.h>
#include <wio_native_object.h>
#include <wio_native_dictionary.h>
#include <wio_native_pod.h>
#include <wio_native_sequence.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{
    void check(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Library final
    {
#if defined(_WIN32)
        HMODULE handle;
        explicit Library(const char* path) : handle(LoadLibraryW(std::filesystem::u8path(path).c_str()))
        {
        }
        ~Library()
        {
            if (handle)
                FreeLibrary(handle);
        }
        template <class T> T symbol(const char* name)
        {
            const auto address = GetProcAddress(handle, name);
#else
        void* handle;
        explicit Library(const char* path) : handle(dlopen(path, RTLD_NOW | RTLD_LOCAL))
        {
        }
        ~Library()
        {
            if (handle)
                dlclose(handle);
        }
        template <class T> T symbol(const char* name)
        {
            const auto address = dlsym(handle, name);
#endif
            check(address != nullptr, name);
            return reinterpret_cast<T>(address);
        }
        Library(const Library&) = delete;
        Library& operator=(const Library&) = delete;
    };

    WioNativeAbiThunk original;
    unsigned corruption;
    WioNativeAbiValue foreign;

    WioNativeAbiStatus upgradeReadonly(const WioNativeAbiValue* arguments, std::uint64_t count,
                                       WioNativeAbiValue* result, WioNativeAbiFailure* failure) noexcept
    {
        const auto status = original(arguments, count, result, failure);
        if (status == WIO_NATIVE_ABI_OK && count == 3)
            result->payload.mutableSlice.data = const_cast<WioNativeAbiValue*>(&arguments[1]);
        return status;
    }

    WioNativeAbiStatus corrupt(const WioNativeAbiValue* arguments, std::uint64_t count, WioNativeAbiValue* result,
                               WioNativeAbiFailure* failure) noexcept
    {
        const auto status = original(arguments, count, result, failure);
        if (status != WIO_NATIVE_ABI_OK || count != 3)
            return status;
        switch (corruption)
        {
        case 0:
            result->payload.mutableSlice.data = reinterpret_cast<void*>(std::uintptr_t{1});
            break;
        case 1:
            result->payload.mutableSlice.data = const_cast<WioNativeAbiValue*>(&arguments[0]);
            break;
        case 2:
            result->payload.mutableSlice.size -= 1;
            break;
        case 3:
            result->typeId ^= 1;
            break;
        case 4:
            result->kind = WIO_NATIVE_ABI_POD;
            break;
        case 5:
            result->owner.generation = 1;
            break;
        case 6:
            result->flags |= WIO_NATIVE_ABI_VALUE_CONSUMED;
            break;
        case 7:
            result->flags |= WIO_NATIVE_ABI_VALUE_SEQUENCE_PROJECTED;
            break;
        case 8:
            result->flags &= ~WIO_NATIVE_ABI_VALUE_BORROWED;
            break;
        case 9:
            foreign = arguments[1]; // A byte-for-byte copy is not the caller's argument wire.
            result->payload.mutableSlice.data = &foreign;
            break;
        case 10:
            result->owner.state = reinterpret_cast<void*>(std::uintptr_t{1});
            break;
        case 11:
            result->flags &= ~WIO_NATIVE_ABI_VALUE_ARGUMENT_ALIAS;
            break;
        case 12:
            result->flags &= ~WIO_NATIVE_ABI_VALUE_MUTABLE;
            break;
        case 13:
            result->payload.mutableSlice.data = const_cast<WioNativeAbiValue*>(&arguments[2]);
            break;
        case 14:
            result->kind = WIO_NATIVE_ABI_RUNTIME_VALUE;
            break;
        case 15:
            result->payload.mutableSlice.data = arguments[1].payload.mutableSlice.data;
            break;
        case 16:
            result->flags |= WIO_NATIVE_ABI_VALUE_OWNED;
            break;
        }
        return status;
    }

    void host(const char* bytecode, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        const auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "projected Any borrow bytecode decode failed");
        const auto& module = decoded.module;
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(module.functions, [&](const auto& candidate)
                                                    { return module.string(candidate.name) == name; });
            check(found != module.functions.end(), name);
            return found->id;
        };
        Library library{producer};
        check(library.handle != nullptr, "projected Any borrow producer load failed");
        const auto registry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry")();
        const auto any = library.symbol<WioGetNativeAnyApiFn>("WioGetNativeAnyApi")();
        const auto objects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi")();
        const auto sequences = library.symbol<WioGetNativeSequenceApiFn>("WioGetNativeSequenceApi")();
        const auto dictionaries = library.symbol<WioGetNativeDictionaryApiFn>("WioGetNativeDictionaryApi")();
        const auto pods = library.symbol<WioGetNativePodLayoutApiFn>("WioGetNativePodLayoutApi")();
        const auto defaults = library.symbol<WioGetNativePodDefaultApiFn>("WioGetNativePodDefaultApi")();
        const auto install = [&](wio::vm::Machine& machine, const WioNativeAbiRegistry& selected)
        {
            std::string error;
            check(machine.registerNativeRegistry(selected, *objects, &error, any, dictionaries, sequences, pods,
                                                 defaults),
                  error.c_str());
        };
        wio::vm::Machine machine{module};
        install(machine, *registry);
        const auto entry = machine.invoke(function("Entry"));
        if (!entry.succeeded())
            throw std::runtime_error(entry.error().code + ": " + entry.error().message);
        check(entry.value().asSignedInteger() == 0, "projected Any borrow host entry failed");
        for (const auto& [name, code] : std::array<std::pair<const char*, const char*>, 2>{
                 {{"RemovedProbe", "WVM1222"}, {"ClearedProbe", "WVM1043"}}})
        {
            const auto failed = machine.invoke(function(name));
            check(!failed.succeeded() && failed.error().code == code, name);
            check(machine.invoke(function("SafePoint")).succeeded(), "Any borrow failure recovery failed");
        }
        const std::array<const char*, 6> lifetimeCodes{"WVM1043", "WVM1043", "WVM1224",
                                                       "WVM1222", "WVM1222", "WVM1222"};
        for (std::int64_t action = 0; action < 6; ++action)
        {
            const std::array arguments{wio::vm::Value::signedInteger(action)};
            const auto failed = machine.invoke(function("LifetimeProbe"), arguments);
            if (failed.succeeded() || failed.error().code != lifetimeCodes[action])
                throw std::runtime_error(
                    "Any lifetime action " + std::to_string(action) + ": " +
                    (failed.succeeded() ? "accepted" : failed.error().code + ": " + failed.error().message));
            check(machine.invoke(function("SafePoint")).succeeded(), "Any lifetime recovery failed");
        }
        const auto cleanup = machine.invoke(function("CleanupProbe"));
        check(!cleanup.succeeded() && cleanup.error().code == "WVM1211" && !cleanup.error().cleanupFailures.empty(),
              "Any copy-back destructor failure was lost");
        const auto balanced = machine.invoke(function("LifecycleBalanced"));
        check(balanced.succeeded() && balanced.value().asBoolean(), "Any cleanup leaked an object claim");
        check(machine.invoke(function("SafePoint")).succeeded(), "Any cleanup recovery failed");
        std::vector<WioNativeAbiFunctionDescriptor> descriptors(registry->functions,
                                                                registry->functions + registry->functionCount);
        const auto& binding = module.functions[function("Array")].nativeBinding;
        const auto selected =
            std::ranges::find_if(descriptors, [&](const auto& descriptor)
                                 { return descriptor.stableKey == module.string(binding.stableKey); });
        check(selected != descriptors.end(), "projected Any borrow thunk missing");
        original = selected->thunk;
        auto changed = *registry;
        changed.functions = descriptors.data();
        selected->flags &= ~WIO_NATIVE_ABI_FUNCTION_PROJECTED_BORROWED_RESULTS;
        {
            wio::vm::Machine old{module};
            install(old, changed);
            const auto failed = old.invoke(function("Probe"));
            check(!failed.succeeded() && failed.error().code == "WVM1224", "unadvertised result capability accepted");
            check(old.invoke(function("SafePoint")).succeeded(), "capability failure recovery failed");
        }
        selected->flags |= WIO_NATIVE_ABI_FUNCTION_PROJECTED_BORROWED_RESULTS;
        selected->thunk = &corrupt;
        for (corruption = 0; corruption < 17; ++corruption)
        {
            wio::vm::Machine invalid{module};
            install(invalid, changed);
            const auto failed = invalid.invoke(function("Probe"));
            if (failed.succeeded() || failed.error().code != "WVM1224")
                throw std::runtime_error(
                    "result corruption " + std::to_string(corruption) + ": " +
                    (failed.succeeded() ? "accepted" : failed.error().code + ": " + failed.error().message));
            check(invalid.invoke(function("SafePoint")).succeeded(), "malformed result recovery failed");
        }
        selected->thunk = original;
        const auto& repeated = module.functions[function("Repeat")].nativeBinding;
        const auto readonly =
            std::ranges::find_if(descriptors, [&](const auto& descriptor)
                                 { return descriptor.stableKey == module.string(repeated.stableKey); });
        check(readonly != descriptors.end(), "repeated projection thunk missing");
        original = readonly->thunk;
        readonly->thunk = &upgradeReadonly;
        wio::vm::Machine invalid{module};
        install(invalid, changed);
        const auto failed = invalid.invoke(function("AliasProbe"));
        check(!failed.succeeded() && failed.error().code == "WVM1224",
              "readonly alias upgraded through a shared mutable token");
        check(invalid.invoke(function("SafePoint")).succeeded(), "readonly upgrade recovery failed");
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "projected Any borrow host requires bytecode and producer paths");
        host(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
