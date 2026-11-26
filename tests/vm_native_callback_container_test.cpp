#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include <wio_native_object.h>
#include <wio_native_any.h>
#include <wio_native_sequence.h>
#include <wio_native_dictionary.h>
#include <wio_native_pod.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
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
        void close()
        {
            if (handle)
                FreeLibrary(handle);
            handle = nullptr;
        }
        template <class T> T symbol(const char* name)
        {
            const auto address = GetProcAddress(handle, name);
#else
        void* handle;
        explicit Library(const char* path) : handle(dlopen(path, RTLD_NOW | RTLD_LOCAL))
        {
        }
        void close()
        {
            if (handle)
                dlclose(handle);
            handle = nullptr;
        }
        template <class T> T symbol(const char* name)
        {
            const auto address = dlsym(handle, name);
#endif
            check(address != nullptr, name);
            return reinterpret_cast<T>(address);
        }
        ~Library()
        {
            close();
        }
        Library(const Library&) = delete;
        Library& operator=(const Library&) = delete;
    };
    struct Wire final
    {
        WioNativeAbiValue value{};
        void clear()
        {
            WioNativeAbiReleaseValue(&value);
        }
        ~Wire()
        {
            clear();
        }
    };
    struct Captured final
    {
        WioNativeAbiCallback callback{};
        Wire input;
        void clear()
        {
            if (callback.ops)
                callback.ops->release(callback.userdata);
            callback = {};
            input.clear();
        }
        ~Captured()
        {
            clear();
        }
    };
    Captured* selectedCapture = nullptr;
    WioNativeSequenceReadFn originalNullableRead = nullptr;
    WioNativeAbiStatus untagNull(const WioNativeAbiValue* input, std::uint64_t index, WioNativeAbiValue* output,
                                 WioNativeAbiFailure* failure) noexcept
    {
        const auto status = originalNullableRead(input, index, output, failure);
        if (status == WIO_NATIVE_ABI_OK && output->kind == WIO_NATIVE_ABI_VOID)
            output->flags = 0;
        return status;
    }
    WioNativeAbiStatus capture(const WioNativeAbiValue* arguments, std::uint64_t count, WioNativeAbiValue* result,
                               WioNativeAbiFailure*) noexcept
    {
        if (!selectedCapture || count != 2 || arguments[0].kind != WIO_NATIVE_ABI_CALLBACK ||
            arguments[1].kind != WIO_NATIVE_ABI_RUNTIME_VALUE)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        auto& captured = *selectedCapture;
        captured.clear();
        captured.callback = arguments[0].payload.callback;
        captured.callback.ops->retain(captured.callback.userdata);
        captured.input.value = arguments[1];
        WioNativeAbiRetain(captured.input.value.payload.handle);
        WioNativeAbiRetain(captured.input.value.owner);
        captured.input.value.flags = WIO_NATIVE_ABI_VALUE_OWNED;
        *result = {};
        return WIO_NATIVE_ABI_OK;
    }
    WioNativeAbiStatus invoke(Captured& captured, const WioNativeAbiValue& input, Wire& result)
    {
        WioNativeAbiFailure failure;
        return captured.callback.ops->invoke(captured.callback.userdata, &input, 1, &result.value, &failure);
    }
    void host(const char* path, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "container callback bytecode decode failed");
        auto module = std::move(decoded.module);
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(module.functions,
                                                    [&](const auto& item) { return module.string(item.name) == name; });
            check(found != module.functions.end(), name);
            return found->id;
        };
        Library library{producer};
        check(library.handle != nullptr, "container callback producer load failed");
        const auto registry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry")();
        const auto objects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi")();
        const auto sequences = library.symbol<WioGetNativeSequenceApiFn>("WioGetNativeSequenceApi")();
        const auto dictionaries = library.symbol<WioGetNativeDictionaryApiFn>("WioGetNativeDictionaryApi")();
        const auto any = library.symbol<WioGetNativeAnyApiFn>("WioGetNativeAnyApi")();
        const auto pods = library.symbol<WioGetNativePodLayoutApiFn>("WioGetNativePodLayoutApi")();
        std::vector<WioNativeAbiFunctionDescriptor> descriptors(registry->functions,
                                                                registry->functions + registry->functionCount);
        std::vector<std::size_t> aliases;
        // Add host-only aliases rather than replace a loaded producer thunk.
        // The real loader still owns all sidecars and their code leases.
        for (const auto* name :
             {"CaptureArray", "CaptureMap", "CaptureAny", "CaptureObjects", "CaptureNullableObjects"})
        {
            auto& binding = module.functions[function(name)].nativeBinding;
            const auto found =
                std::ranges::find_if(descriptors, [&](const auto& descriptor)
                                     { return descriptor.stableKey == module.string(binding.stableKey); });
            check(found != descriptors.end(), "container capture thunk missing");
            aliases.push_back(static_cast<std::size_t>(found - descriptors.begin()));
            module.strings.push_back(std::string(module.string(binding.stableKey)) + ":test-host");
            binding.stableKey = static_cast<std::uint32_t>(module.strings.size() - 1);
        }
        std::size_t alias = 0;
        for (const auto* name :
             {"CaptureArray", "CaptureMap", "CaptureAny", "CaptureObjects", "CaptureNullableObjects"})
        {
            auto& descriptor = descriptors[aliases[alias++]];
            descriptor.stableKey = module.strings[module.functions[function(name)].nativeBinding.stableKey].c_str();
            descriptor.thunk = &capture;
        }
        auto changed = *registry;
        changed.functions = descriptors.data();
        for (const auto* name : {"HostArray", "HostMap", "HostMissingAny"})
        {
            wio::vm::Machine missing{module};
            std::string error;
            check(missing.registerNativeRegistry(changed, &error), error.c_str());
            const auto rejected = missing.invoke(function(name));
            if (!rejected.succeeded() && rejected.error().code != "WVM1225")
                throw std::runtime_error(rejected.error().code + ": " + rejected.error().message);
            check(!rejected.succeeded() && rejected.error().code == "WVM1225",
                  "container callback admitted without its sidecar");
        }
        const auto install = [&](wio::vm::Machine& machine)
        {
            std::string error;
            check(machine.loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
            check(machine.registerNativeRegistry(changed, *objects, &error, any, dictionaries, sequences, pods),
                  error.c_str());
        };
        const auto balanced = [&](wio::vm::Machine& machine)
        {
            const auto result = machine.invoke(function("LifecycleBalanced"));
            check(result.succeeded() && result.value().asBoolean(), "container callback leaked object lifecycle");
        };
        const auto readArray = [&](const WioNativeAbiValue& wire, std::int64_t first, std::int64_t second)
        {
            const auto adapter = std::ranges::find(sequences->types, sequences->types + sequences->typeCount,
                                                   wire.typeId, &WioNativeSequenceDescriptor::typeId);
            check(adapter != sequences->types + sequences->typeCount, "escaped callback array adapter missing");
            WioNativeAbiFailure failure;
            std::uint64_t count = 0;
            check(adapter->count(&wire, &count, &failure) == WIO_NATIVE_ABI_OK && count == 2,
                  "escaped callback array count failed");
            for (unsigned index = 0; index < 2; ++index)
            {
                Wire element;
                check(adapter->read(&wire, index, &element.value, &failure) == WIO_NATIVE_ABI_OK &&
                          element.value.kind == WIO_NATIVE_ABI_I64 &&
                          element.value.payload.signedInteger == (index ? second : first),
                      "escaped callback array payload changed");
            }
        };
        Captured captured;
        Wire escapedArray, escapedMap, escapedAny;
        {
            const auto nullableType =
                module.functions[function("CaptureNullableObjects")].nativeBinding.parameters[1].type;
            const auto reflected =
                std::ranges::find(module.contract.reflection, nullableType, &wio::bytecode::Reflection::type);
            check(reflected != module.contract.reflection.end(), "nullable callback array identity missing");
            std::vector<WioNativeSequenceDescriptor> malformed(sequences->types,
                                                               sequences->types + sequences->typeCount);
            const auto adapter =
                std::ranges::find(malformed, reflected->stableTypeId, &WioNativeSequenceDescriptor::typeId);
            check(adapter != malformed.end(), "nullable callback array adapter missing");
            originalNullableRead = adapter->read;
            adapter->read = &untagNull;
            auto badSequences = *sequences;
            badSequences.types = malformed.data();
            wio::vm::Machine machine{module};
            std::string error;
            check(machine.registerNativeRegistry(changed, *objects, &error, any, dictionaries, &badSequences, pods),
                  error.c_str());
            selectedCapture = &captured;
            check(machine.invoke(function("HostNullableObjects")).succeeded(), "nullable object array capture failed");
            Wire rejected;
            check(invoke(captured, captured.input.value, rejected) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                  "nested object callback accepted untagged nullable null");
            captured.clear();
            balanced(machine);
        }
        {
            wio::vm::Machine machine{module};
            install(machine);
            const auto entry = machine.invoke(function("Entry"));
            if (!entry.succeeded())
                throw std::runtime_error(entry.error().code + ": " + entry.error().message);
            check(entry.value().asSignedInteger() == 0, "container callback host entry failed");
            balanced(machine);
            selectedCapture = &captured;
            check(machine.invoke(function("HostArray")).succeeded(), "array callback capture failed");
            check(invoke(captured, captured.input.value, escapedArray) == WIO_NATIVE_ABI_OK,
                  "array callback invocation failed");
            readArray(escapedArray.value, 9, 21);
            readArray(captured.input.value, 7, 19);
            for (unsigned variant = 0; variant < 6; ++variant)
            {
                auto bad = captured.input.value;
                switch (variant)
                {
                case 0:
                    bad.typeId = any->typeId;
                    break;
                case 1:
                    bad.payload.handle.ops = nullptr;
                    break;
                case 2:
                    bad.kind = WIO_NATIVE_ABI_U64;
                    break;
                case 3:
                    bad.typeId = 0;
                    break;
                case 4:
                    bad.payload.handle.state = nullptr;
                    break;
                case 5:
                    bad.kind = WIO_NATIVE_ABI_VOID;
                    bad.flags = WIO_NATIVE_ABI_VALUE_NULLABLE;
                    break;
                }
                Wire rejected;
                check(invoke(captured, bad, rejected) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                      "malformed container callback argument accepted");
            }
            captured.clear();
            check(machine.invoke(function("HostMap")).succeeded(), "map callback capture failed");
            check(invoke(captured, captured.input.value, escapedMap) == WIO_NATIVE_ABI_OK,
                  "map callback invocation failed");
            captured.clear();
            check(machine.invoke(function("HostAny")).succeeded(), "Any callback capture failed");
            check(invoke(captured, captured.input.value, escapedAny) == WIO_NATIVE_ABI_OK &&
                      (escapedAny.value.flags & WIO_NATIVE_ABI_VALUE_OWNED) &&
                      !(escapedAny.value.flags & WIO_NATIVE_ABI_VALUE_BORROWED),
                  "cached Any callback result did not acquire an owning claim");
            captured.clear();
            check(machine.invoke(function("HostPanic")).succeeded(), "container panic capture failed");
            Wire panic;
            check(invoke(captured, captured.input.value, panic) == WIO_NATIVE_ABI_PANIC,
                  "container callback panic crossed native boundary");
            captured.clear();
            balanced(machine);
            check(machine.invoke(function("HostObjects")).succeeded(), "object array callback capture failed");
            {
                const auto baseline = machine.invoke(function("LifecycleLive"));
                check(baseline.succeeded(), "cross-machine lifecycle baseline failed");
                Captured foreign;
                wio::vm::Machine other{module};
                install(other);
                selectedCapture = &foreign;
                check(other.invoke(function("HostObjects")).succeeded(), "second machine object array failed");
                Wire rejected;
                check(invoke(captured, foreign.input.value, rejected) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                      "container callback accepted another machine's nested object");
                foreign.clear();
                const auto after = other.invoke(function("LifecycleLive"));
                check(after.succeeded() && after.value().asSignedInteger() == baseline.value().asSignedInteger(),
                      "cross-machine rejection leaked nested object ownership");
            }
            selectedCapture = &captured;
            for (auto* wire : {&escapedArray, &escapedMap, &escapedAny})
                check(wire->value.owner.ops &&
                          wire->value.owner.ops->typeId(wire->value.owner.state) == wire->value.typeId,
                      "escaped container result has no matching producer code lease");
        }
        Wire stale;
        check(invoke(captured, captured.input.value, stale) == WIO_NATIVE_ABI_STALE_HANDLE,
              "container callback entered a destroyed owner");
        captured.clear();
        const auto balanceBinding = module.functions[function("Balanced")].nativeBinding;
        const auto balanceDescriptor =
            std::ranges::find_if(descriptors, [&](const auto& descriptor)
                                 { return descriptor.stableKey == module.string(balanceBinding.stableKey); });
        check(balanceDescriptor != descriptors.end(), "native balance counter missing");
        Wire balance;
        WioNativeAbiFailure failure;
        check(balanceDescriptor->thunk(nullptr, 0, &balance.value, &failure) == WIO_NATIVE_ABI_OK &&
                  balance.value.payload.boolean,
              "shutdown did not destroy nested callback objects");
        wio::vm::Machine::collectNativeObjectRetirements();
        library.close();
        // No machine, callback or manual loader keeps the producer alive here.
        // Result owner leases alone must protect the sidecar and handle ops.
        readArray(escapedArray.value, 9, 21);
        const auto mapAdapter = std::ranges::find(dictionaries->types, dictionaries->types + dictionaries->typeCount,
                                                  escapedMap.value.typeId, &WioNativeDictionaryDescriptor::typeId);
        check(mapAdapter != dictionaries->types + dictionaries->typeCount, "escaped map adapter missing");
        Wire key, mapped;
        WioNativeAbiValue entries[2]{};
        check(mapAdapter->read(&escapedMap.value, entries, 1, &failure) == WIO_NATIVE_ABI_OK,
              "escaped map unreadable after machine/loader shutdown");
        key.value = entries[0];
        mapped.value = entries[1];
        readArray(mapped.value, 7, 19);
        const auto innerDescriptor = std::ranges::find(any->payloads, any->payloads + any->payloadCount, any->typeId,
                                                       &WioNativeAnyPayloadDescriptor::typeId);
        check(innerDescriptor != any->payloads + any->payloadCount, "nested Any adapter missing");
        Wire inner;
        bool matches = false;
        check(innerDescriptor->read(&escapedAny.value, &inner.value, &matches, &failure) == WIO_NATIVE_ABI_OK &&
                  matches,
              "escaped cached Any lost its inner cell");
        bool foundString = false;
        for (std::uint32_t index = 0; index < any->payloadCount; ++index)
        {
            Wire payload;
            matches = false;
            check(any->payloads[index].read(&inner.value, &payload.value, &matches, &failure) == WIO_NATIVE_ABI_OK,
                  "escaped Any payload read failed");
            if (matches && payload.value.kind == WIO_NATIVE_ABI_BYTES)
            {
                const std::string actual(static_cast<const char*>(payload.value.payload.slice.data),
                                         static_cast<std::size_t>(payload.value.payload.slice.size));
                check(actual == std::string("a\0\xF0\x9F\x8C\x8D", 6), "escaped Any NUL/Unicode bytes changed");
                foundString = true;
            }
        }
        check(foundString, "escaped Any string payload missing");
        inner.clear();
        key.clear();
        mapped.clear();
        escapedAny.clear();
        escapedMap.clear();
        escapedArray.clear();
        wio::vm::Machine::collectNativeObjectRetirements();
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "container callback host requires bytecode and producer paths");
        host(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
