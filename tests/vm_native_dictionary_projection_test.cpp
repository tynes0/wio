#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include <wio_native_any.h>
#include <wio_native_object.h>
#include <wio_native_dictionary.h>
#include <wio_native_pod.h>
#include <wio_native_sequence.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

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

    WioNativeAbiStatus corrupt(const WioNativeAbiValue* input, std::uint64_t count, WioNativeAbiValue* result,
                               WioNativeAbiFailure* failure) noexcept
    {
        if (count != 2 || !(input[1].flags & WIO_NATIVE_ABI_VALUE_CONTAINER_PROJECTED))
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        std::array<WioNativeAbiValue, 2> arguments{input[0], input[1]};
        auto projection = *static_cast<const WioNativeAbiContainerProjection*>(input[1].payload.mutableSlice.data);
        std::array<WioNativeAbiContainerStep, 2> steps{projection.steps[0], {}};
        projection.steps = steps.data();
        switch (corruption)
        {
        case 0:
            projection.count = 0;
            break;
        case 1:
            projection.count = 65;
            break;
        case 2:
            projection.steps = nullptr;
            break;
        case 3:
            projection.steps = reinterpret_cast<WioNativeAbiContainerStep*>(std::uintptr_t{1});
            break;
        case 4:
            projection.root = reinterpret_cast<WioNativeAbiValue*>(std::uintptr_t{1});
            break;
        case 5:
            projection.rootTypeId ^= 1;
            break;
        case 6:
            projection.typeId ^= 1;
            break;
        case 7:
            arguments[1].typeId ^= 1;
            break;
        case 8:
            arguments[1].payload.mutableSlice.size -= 1;
            break;
        case 9:
            arguments[1].flags |= WIO_NATIVE_ABI_VALUE_SEQUENCE_PROJECTED;
            break;
        case 10:
            steps[0].reserved = 1;
            break;
        case 11:
            steps[0].kind = 99;
            break;
        case 12:
            steps[0].containerTypeId ^= 1;
            break;
        case 13:
            steps[0].index = 1;
            break;
        case 14:
            steps[0].key.typeId ^= 1;
            break;
        case 15:
            steps[0].key.kind = WIO_NATIVE_ABI_BOOL;
            break;
        case 16:
            steps[0].key.payload.slice = {"missing", 7};
            break;
        case 17:
            projection.count = 2;
            break;
        case 18:
            projection.offset = 1;
            break;
        case 19:
            projection.podTypeId = 1;
            projection.size = 4;
            break;
        case 20:
            projection.size = 1;
            break;
        case 21:
            arguments[1].flags &= ~WIO_NATIVE_ABI_VALUE_MUTABLE;
            break;
        default:
            arguments[1].payload.mutableSlice.data = reinterpret_cast<void*>(std::uintptr_t{1});
            return original(arguments.data(), count, result, failure);
        }
        arguments[1].payload.mutableSlice.data = &projection;
        return original(arguments.data(), count, result, failure);
    }

    WioNativeAbiThunk mixedOriginal;
    WioNativeAbiStatus corruptMixed(const WioNativeAbiValue* input, std::uint64_t count, WioNativeAbiValue* result,
                                    WioNativeAbiFailure* failure) noexcept
    {
        if (count != 3 || !(input[2].flags & WIO_NATIVE_ABI_VALUE_CONTAINER_PROJECTED))
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        std::array<WioNativeAbiValue, 3> arguments{input[0], input[1], input[2]};
        auto projection = *static_cast<const WioNativeAbiContainerProjection*>(input[2].payload.mutableSlice.data);
        std::array<WioNativeAbiContainerStep, 2> steps{projection.steps[0], projection.steps[1]};
        projection.steps = steps.data();
        switch (corruption)
        {
        case 0:
            steps[0].index = 999;
            break;
        case 1:
            steps[0].kind = WIO_NATIVE_ABI_CONTAINER_VALUE;
            break;
        case 2:
            steps[0].key.kind = WIO_NATIVE_ABI_BOOL;
            break;
        case 3:
            steps[0].key.typeId = 1;
            break;
        case 4:
            steps[0].containerTypeId ^= 1;
            break;
        case 5:
            steps[1].containerTypeId ^= 1;
            break;
        case 6:
            steps[1].kind = WIO_NATIVE_ABI_CONTAINER_INDEX;
            break;
        case 7:
            projection.count = 1;
            break;
        default:
            steps[0].key.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            break;
        }
        arguments[2].payload.mutableSlice.data = &projection;
        return mixedOriginal(arguments.data(), count, result, failure);
    }

    void host(const char* bytecode, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        const auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "dictionary projection bytecode decode failed");
        const auto& module = decoded.module;
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(module.functions, [&](const auto& candidate)
                                                    { return module.string(candidate.name) == name; });
            check(found != module.functions.end(), name);
            return found->id;
        };
        Library library{producer};
        check(library.handle != nullptr, "dictionary projection producer load failed");
        const auto registry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry")();
        const auto any = library.symbol<WioGetNativeAnyApiFn>("WioGetNativeAnyApi")();
        const auto objects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi")();
        const auto dictionaries = library.symbol<WioGetNativeDictionaryApiFn>("WioGetNativeDictionaryApi")();
        const auto sequences = library.symbol<WioGetNativeSequenceApiFn>("WioGetNativeSequenceApi")();
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
        check(entry.value().asSignedInteger() == 0, "dictionary projection host entry failed");
        const auto borrowed = machine.invoke(function("BorrowProbe"));
        check(borrowed.succeeded() && borrowed.value().asSignedInteger() == 7,
              "qualified dictionary borrowed result failed");
        for (const auto& [name, code] : std::array<std::pair<const char*, const char*>, 2>{
                 {{"ExpiredProbe", "WVM1043"}, {"FailureProbe", "WVM1222"}}})
        {
            const auto failed = machine.invoke(function(name));
            check(!failed.succeeded() && failed.error().code == code, name);
            check(machine.invoke(function("SafePoint")).succeeded(), "dictionary projection recovery failed");
        }
        const auto cleanup = machine.invoke(function("CleanupProbe"));
        check(!cleanup.succeeded() && cleanup.error().code == "WVM1211" && !cleanup.error().cleanupFailures.empty() &&
                  cleanup.error().message.find("Native copy-back cleanup") != std::string::npos,
              "dictionary projection swallowed copy-back destructor failure");
        const auto balanced = machine.invoke(function("LifecycleBalanced"));
        check(balanced.succeeded() && balanced.value().asBoolean(), "dictionary projection leaked an object claim");
        std::vector<WioNativeAbiFunctionDescriptor> descriptors(registry->functions,
                                                                registry->functions + registry->functionCount);
        const auto& binding = module.functions[function("Mutate")].nativeBinding;
        const auto selected =
            std::ranges::find_if(descriptors, [&](const auto& descriptor)
                                 { return descriptor.stableKey == module.string(binding.stableKey); });
        check(selected != descriptors.end(), "dictionary projection thunk missing");
        original = selected->thunk;
        auto changed = *registry;
        changed.functions = descriptors.data();
        selected->flags &= ~WIO_NATIVE_ABI_FUNCTION_CONTAINER_PROJECTIONS;
        {
            wio::vm::Machine old{module};
            install(old, changed);
            const auto before = old.invoke(function("ReadCalls"));
            const auto failed = old.invoke(function("Probe"));
            const auto after = old.invoke(function("ReadCalls"));
            check(!failed.succeeded() && failed.error().code == "WVM1225", "old producer capability ignored");
            check(before.succeeded() && after.succeeded() && before.value() == after.value(),
                  "old producer reached native body");
        }
        selected->flags |= WIO_NATIVE_ABI_FUNCTION_CONTAINER_PROJECTIONS;
        selected->thunk = &corrupt;
        for (corruption = 0; corruption < 23; ++corruption)
        {
            wio::vm::Machine invalid{module};
            install(invalid, changed);
            const auto before = invalid.invoke(function("ReadCalls"));
            const auto failed = invalid.invoke(function("Probe"));
            if (failed.succeeded() || failed.error().code != "WVM1222")
                throw std::runtime_error(
                    "corruption " + std::to_string(corruption) + ": " +
                    (failed.succeeded() ? "accepted" : failed.error().code + ": " + failed.error().message));
            const auto after = invalid.invoke(function("ReadCalls"));
            check(before.succeeded() && after.succeeded() && before.value() == after.value(),
                  "forged dictionary projection reached native body");
            check(invalid.invoke(function("SafePoint")).succeeded(), "forged dictionary recovery failed");
        }
        const auto& mixedBinding = module.functions[function("Batches")].nativeBinding;
        const auto mixed =
            std::ranges::find_if(descriptors, [&](const auto& descriptor)
                                 { return descriptor.stableKey == module.string(mixedBinding.stableKey); });
        check(mixed != descriptors.end(), "mixed-path thunk missing");
        selected->thunk = original;
        mixedOriginal = mixed->thunk;
        mixed->thunk = &corruptMixed;
        for (corruption = 0; corruption < 9; ++corruption)
        {
            wio::vm::Machine invalid{module};
            install(invalid, changed);
            const auto before = invalid.invoke(function("ReadCalls"));
            const auto failed = invalid.invoke(function("MixedProbe"));
            check(!failed.succeeded() && failed.error().code == "WVM1222", "forged mixed projection was accepted");
            const auto after = invalid.invoke(function("ReadCalls"));
            check(before.succeeded() && after.succeeded() && before.value() == after.value(),
                  "forged mixed projection reached native body");
            check(invalid.invoke(function("SafePoint")).succeeded(), "forged mixed recovery failed");
        }
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "dictionary projection host requires bytecode and producer paths");
        host(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
