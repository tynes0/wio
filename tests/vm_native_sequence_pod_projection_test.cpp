#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include "native/vm_sequence_pod_projection_fixture.h"
#include <wio_native_pod.h>
#include <wio_native_sequence.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
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
        if (count != 2 || !(input[1].flags & WIO_NATIVE_ABI_VALUE_SEQUENCE_POD_PROJECTED))
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        std::array<WioNativeAbiValue, 2> arguments{input[0], input[1]};
        auto projection = *static_cast<const WioNativeAbiSequencePodProjection*>(input[1].payload.mutableSlice.data);
        std::uint64_t index = 9999;
        switch (corruption)
        {
        case 0:
            projection.sequence.count = 65;
            break;
        case 1:
            projection.sequence.indices = &index;
            break;
        case 2:
            projection.sequence.typeId ^= 1;
            break;
        case 3:
            projection.sequence.rootTypeId ^= 1;
            break;
        case 4:
            projection.sequence.root = reinterpret_cast<WioNativeAbiValue*>(std::uintptr_t{1});
            break;
        case 5:
            projection.sequence.indices = nullptr;
            break;
        case 6:
            arguments[1].flags |= WIO_NATIVE_ABI_VALUE_PROJECTED;
            break;
        case 7:
            arguments[1].flags |= WIO_NATIVE_ABI_VALUE_SEQUENCE_PROJECTED;
            break;
        case 8:
            arguments[1].typeId ^= 1;
            break;
        case 9:
            projection.sequence.count = 0;
            break;
        case 10:
            projection.podTypeId ^= 1;
            break;
        case 11:
            projection.offset = (std::numeric_limits<std::uint64_t>::max)();
            break;
        case 12:
            projection.offset += 1;
            break;
        case 13:
            projection.size += 1;
            break;
        case 14:
            arguments[1].flags &= ~WIO_NATIVE_ABI_VALUE_MUTABLE;
            break;
        case 15:
            arguments[1].payload.mutableSlice.size -= 1;
            break;
        case 16:
            // Packet::hidden is valid C++ storage but is not a declared Wio field.
            projection.offset = offsetof(vm_sequence_pod_projection::Packet, hidden);
            break;
        default:
            arguments[1].payload.mutableSlice.data = reinterpret_cast<void*>(std::uintptr_t{1});
            return original(arguments.data(), count, result, failure);
        }
        arguments[1].payload.mutableSlice.data = &projection;
        return original(arguments.data(), count, result, failure);
    }

    void host(const char* bytecode, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        const auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "POD projection bytecode decode failed");
        const auto& module = decoded.module;
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(module.functions, [&](const auto& candidate)
                                                    { return module.string(candidate.name) == name; });
            check(found != module.functions.end(), name);
            return found->id;
        };
        Library library{producer};
        check(library.handle != nullptr, "POD projection producer load failed");
        const auto registry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry")();
        const auto sequences = library.symbol<WioGetNativeSequenceApiFn>("WioGetNativeSequenceApi")();
        const auto pods = library.symbol<WioGetNativePodLayoutApiFn>("WioGetNativePodLayoutApi")();
        const auto defaults = library.symbol<WioGetNativePodDefaultApiFn>("WioGetNativePodDefaultApi")();
        const auto install = [&](wio::vm::Machine& machine, const WioNativeAbiRegistry& selected)
        {
            std::string error;
            check(machine.registerNativeRegistry(selected, *sequences, &error, pods, defaults), error.c_str());
        };
        wio::vm::Machine machine{module};
        install(machine, *registry);
        const auto entry = machine.invoke(function("Entry"));
        if (!entry.succeeded())
            throw std::runtime_error(entry.error().code + ": " + entry.error().message);
        check(entry.value().asSignedInteger() == 0, "POD projection host entry failed");
        const auto borrowed = machine.invoke(function("BorrowProbe"));
        check(borrowed.succeeded() && borrowed.value().asSignedInteger() == 7,
              "qualified sequence/POD borrowed result failed");
        for (const auto& [name, code] : std::array<std::pair<const char*, const char*>, 3>{
                 {{"ExpiredProbe", "WVM1043"}, {"FailureProbe", "WVM1222"}, {"RowOnlyProbe", "WVM1223"}}})
        {
            const auto failed = machine.invoke(function(name));
            check(!failed.succeeded() && failed.error().code == code, name);
            check(machine.invoke(function("SafePoint")).succeeded(), "POD projection recovery failed");
        }
        std::vector<WioNativeAbiFunctionDescriptor> descriptors(registry->functions,
                                                                registry->functions + registry->functionCount);
        const auto& binding = module.functions[function("Mutate")].nativeBinding;
        const auto selected =
            std::ranges::find_if(descriptors, [&](const auto& descriptor)
                                 { return descriptor.stableKey == module.string(binding.stableKey); });
        check(selected != descriptors.end(), "POD projection thunk missing");
        original = selected->thunk;
        auto changed = *registry;
        changed.functions = descriptors.data();
        selected->flags &= ~WIO_NATIVE_ABI_FUNCTION_SEQUENCE_POD_PROJECTIONS;
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
        selected->flags |= WIO_NATIVE_ABI_FUNCTION_SEQUENCE_POD_PROJECTIONS;
        selected->thunk = &corrupt;
        for (corruption = 0; corruption < 18; ++corruption)
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
                  "forged POD projection reached native body");
            check(invalid.invoke(function("SafePoint")).succeeded(), "forged POD projection recovery failed");
        }
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "POD projection host requires bytecode and producer paths");
        host(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
