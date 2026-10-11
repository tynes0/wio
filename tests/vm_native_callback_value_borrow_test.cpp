#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include <wio_native_any.h>
#include <wio_native_dictionary.h>
#include <wio_native_object.h>
#include <wio_native_pod.h>
#include <wio_native_sequence.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
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
    struct Library
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
            auto address = GetProcAddress(handle, name);
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
            auto address = dlsym(handle, name);
#endif
            check(address != nullptr, name);
            return reinterpret_cast<T>(address);
        }
        ~Library()
        {
            close();
        }
    };
    struct Wire
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
    struct Captured
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
    Captured* selected = nullptr;
    WioNativeAbiStatus capture(const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* result,
                               WioNativeAbiFailure*) noexcept
    {
        if (count != 2 || !selected)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        selected->clear();
        selected->callback = args[0].payload.callback;
        selected->callback.ops->retain(selected->callback.userdata);
        selected->input.value = args[1];
        WioNativeAbiRetain(selected->input.value.payload.handle);
        WioNativeAbiRetain(selected->input.value.owner);
        if (selected->input.value.kind != WIO_NATIVE_ABI_VOID)
        {
            selected->input.value.flags |= WIO_NATIVE_ABI_VALUE_OWNED;
            selected->input.value.flags &= ~WIO_NATIVE_ABI_VALUE_BORROWED;
        }
        *result = {};
        return WIO_NATIVE_ABI_OK;
    }
    void copy(Wire& target, const WioNativeAbiValue& value)
    {
        target.clear();
        target.value = value;
        if (value.kind == WIO_NATIVE_ABI_RUNTIME_VALUE || value.kind == WIO_NATIVE_ABI_OBJECT)
            WioNativeAbiRetain(target.value.payload.handle);
        WioNativeAbiRetain(target.value.owner);
        if (value.kind != WIO_NATIVE_ABI_VOID)
        {
            target.value.flags |= WIO_NATIVE_ABI_VALUE_OWNED;
            target.value.flags &= ~WIO_NATIVE_ABI_VALUE_BORROWED;
        }
    }
    std::array<WioNativeAbiValue, 2> references(Wire& cell)
    {
        std::array<WioNativeAbiValue, 2> args{};
        for (auto& arg : args)
        {
            arg.kind = WIO_NATIVE_ABI_REFERENCE;
            arg.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            arg.typeId = cell.value.typeId;
            arg.payload.mutableSlice = {&cell.value, sizeof(WioNativeAbiValue)};
        }
        args[1].flags |= WIO_NATIVE_ABI_VALUE_MUTABLE;
        return args;
    }
    WioNativeAbiStatus invoke(Captured& captured, Wire& cell, Wire& result)
    {
        auto args = references(cell);
        WioNativeAbiFailure failure{};
        return captured.callback.ops->invoke(captured.callback.userdata, args.data(), args.size(), &result.value,
                                             &failure);
    }
    struct NativePair
    {
        unsigned claims = 1;
        unsigned mode = 0;
        Wire replacement;
        static const WioNativeAbiCallbackOps Ops;
    };
    const WioNativeAbiCallbackOps NativePair::Ops{
        WIO_NATIVE_ABI_VERSION, WIO_NATIVE_ABI_CALLBACK_CALLER_THREAD,
        [](void* state) noexcept { ++static_cast<NativePair*>(state)->claims; },
        [](void* state) noexcept { --static_cast<NativePair*>(state)->claims; },
        [](void* state, const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* result,
           WioNativeAbiFailure*) noexcept
        {
            if (count != 2)
                return WIO_NATIVE_ABI_INVALID_ARGUMENT;
            auto& pair = *static_cast<NativePair*>(state);
            auto* first = static_cast<WioNativeAbiValue*>(args[0].payload.mutableSlice.data);
            auto* second = static_cast<WioNativeAbiValue*>(args[1].payload.mutableSlice.data);
            WioNativeAbiReleaseValue(first);
            *first = pair.replacement.value;
            WioNativeAbiRetain(first->payload.handle);
            WioNativeAbiRetain(first->owner);
            if (pair.mode == 0)
            {
                WioNativeAbiReleaseValue(second);
                second->kind = WIO_NATIVE_ABI_U64;
                second->payload.unsignedInteger = 3;
            }
            if (pair.mode == 2)
                first->typeId ^= 1;
            *result = {};
            result->kind = WIO_NATIVE_ABI_I64;
            result->payload.signedInteger = 9;
            return WIO_NATIVE_ABI_OK;
        }};

    void host(const char* path, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "value borrow bytecode decode failed");
        auto module = std::move(decoded.module);
        const auto function = [&](const char* name)
        {
            auto found = std::ranges::find_if(module.functions,
                                              [&](const auto& item) { return module.string(item.name) == name; });
            check(found != module.functions.end(), name);
            return found->id;
        };
        const auto identity = [&](std::uint32_t type)
        {
            auto found = std::ranges::find(module.contract.reflection, type, &wio::bytecode::Reflection::type);
            check(found != module.contract.reflection.end(), "reflection identity missing");
            return found->stableTypeId;
        };
        const auto method = [&](const char* name) -> const wio::bytecode::ReflectedMethod&
        {
            for (const auto& type : module.contract.reflection)
                for (const auto& method : type.methods)
                    if (module.string(method.name) == name)
                        return method;
            throw std::runtime_error(name);
        };
        Library library{producer};
        check(library.handle != nullptr, "producer load failed");
        const auto registry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry")();
        const auto objects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi")();
        const auto sequences = library.symbol<WioGetNativeSequenceApiFn>("WioGetNativeSequenceApi")();
        const auto dictionaries = library.symbol<WioGetNativeDictionaryApiFn>("WioGetNativeDictionaryApi")();
        const auto any = library.symbol<WioGetNativeAnyApiFn>("WioGetNativeAnyApi")();
        const auto pods = library.symbol<WioGetNativePodLayoutApiFn>("WioGetNativePodLayoutApi")();
        std::vector<WioNativeAbiFunctionDescriptor> descriptors(registry->functions,
                                                                registry->functions + registry->functionCount);
        std::vector<std::size_t> aliases;
        for (const auto* name : {"CaptureArray", "CaptureMap", "CaptureAny", "CaptureObject", "CaptureNullable"})
        {
            auto& binding = module.functions[function(name)].nativeBinding;
            auto found = std::ranges::find_if(descriptors, [&](const auto& item)
                                              { return item.stableKey == module.string(binding.stableKey); });
            check(found != descriptors.end(), "capture thunk missing");
            aliases.push_back(found - descriptors.begin());
            module.strings.push_back(std::string(module.string(binding.stableKey)) + ":borrow-host");
            binding.stableKey = static_cast<std::uint32_t>(module.strings.size() - 1);
        }
        std::size_t index = 0;
        for (const auto* name : {"CaptureArray", "CaptureMap", "CaptureAny", "CaptureObject", "CaptureNullable"})
        {
            auto& descriptor = descriptors[aliases[index++]];
            descriptor.stableKey = module.strings[module.functions[function(name)].nativeBinding.stableKey].c_str();
            descriptor.thunk = &capture;
        }
        auto changed = *registry;
        changed.functions = descriptors.data();
        const auto install = [&](wio::vm::Machine& machine)
        {
            std::string error;
            check(machine.loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
            check(machine.registerNativeRegistry(changed, *objects, &error, any, dictionaries, sequences, pods),
                  error.c_str());
        };
        const auto readArray = [&](const WioNativeAbiValue& wire, std::int64_t first, std::uint64_t expectedCount)
        {
            auto adapter = std::ranges::find(sequences->types, sequences->types + sequences->typeCount, wire.typeId,
                                             &WioNativeSequenceDescriptor::typeId);
            check(adapter != sequences->types + sequences->typeCount, "array adapter missing");
            std::uint64_t count = 0;
            WioNativeAbiFailure failure{};
            Wire element;
            check(adapter->count(&wire, &count, &failure) == WIO_NATIVE_ABI_OK && count == expectedCount,
                  "array count changed");
            check(adapter->read(&wire, 0, &element.value, &failure) == WIO_NATIVE_ABI_OK &&
                      element.value.payload.signedInteger == first,
                  "array payload changed");
        };
        for (auto name : {"HostArray", "HostMap", "HostAny"})
        {
            wio::vm::Machine missing{module};
            std::string error;
            check(missing.registerNativeRegistry(changed, &error), error.c_str());
            auto result = missing.invoke(function(name));
            check(!result.succeeded() && result.error().code == "WVM1225", "borrow accepted without sidecar");
        }
        Captured captured;
        Wire escapedArray, escapedMap, escapedAny;
        {
            wio::vm::Machine machine{module};
            install(machine);
            selected = &captured;
            auto entry = machine.invoke(function("Entry"));
            if (!entry.succeeded())
                throw std::runtime_error(entry.error().code + ": " + entry.error().message);
            check(entry.value().asSignedInteger() == 0, "host entry failed");
            check(machine.invoke(function("HostArray")).succeeded(), "array capture failed");
            Wire cell;
            copy(cell, captured.input.value);
            check(invoke(captured, cell, escapedArray) == WIO_NATIVE_ABI_OK, "array borrow failed");
            readArray(cell.value, 9, 2);
            readArray(escapedArray.value, 9, 2);
            readArray(captured.input.value, 7, 1);
            for (unsigned variant = 0; variant < 6; ++variant)
            {
                auto bad = captured.input.value;
                if (variant == 0)
                    bad.typeId ^= 1;
                if (variant == 1)
                    bad.kind = WIO_NATIVE_ABI_U64;
                if (variant == 2)
                    bad.payload.handle.state = nullptr;
                if (variant == 3)
                    bad.payload.handle.ops = nullptr;
                if (variant == 4)
                    bad.typeId = 0;
                if (variant == 5)
                    bad.kind = WIO_NATIVE_ABI_VOID;
                auto args = references(cell);
                args[0].payload.mutableSlice.data = &bad;
                args[1].payload.mutableSlice.data = &bad;
                Wire rejected;
                WioNativeAbiFailure failure{};
                check(captured.callback.ops->invoke(captured.callback.userdata, args.data(), 2, &rejected.value,
                                                    &failure) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                      "malformed borrowed container admitted");
                readArray(cell.value, 9, 2);
            }
            NativePair pair;
            copy(pair.replacement, cell.value);
            const auto receiver = machine.invoke(function("HostReceiver"));
            check(receiver.succeeded(), "host receiver failed");
            auto saved = *reinterpret_cast<const WioNativeObjectHost*>(
                static_cast<std::uintptr_t>(receiver.value().asUnsignedInteger()));
            WioNativeAbiRetain(saved.owner);
            WioNativeAbiValue callback{};
            callback.kind = WIO_NATIVE_ABI_CALLBACK;
            callback.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            callback.typeId = identity(method("Pair").parameterTypes[0]);
            callback.payload.callback = {&pair, &NativePair::Ops};
            const auto dispatch = [&](const char* name, const WioNativeAbiValue* args, unsigned count, Wire& result)
            {
                WioNativeAbiFailure failure{};
                return saved.invoke(&saved, method(name).stableId, args, count, &result.value, &failure);
            };
            for (auto mode : {0u, 2u})
            {
                pair.mode = mode;
                Wire rejected, first, second;
                check(dispatch("Pair", &callback, 1, rejected) == WIO_NATIVE_ABI_PANIC,
                      "invalid native container copy-back accepted");
                check(dispatch("First", nullptr, 0, first) == WIO_NATIVE_ABI_OK &&
                          first.value.payload.signedInteger == 7,
                      "partial first-cell copy-back");
                check(dispatch("Second", nullptr, 0, second) == WIO_NATIVE_ABI_OK &&
                          second.value.payload.signedInteger == 19,
                      "partial second-cell copy-back");
            }
            pair.mode = 1;
            Wire accepted;
            check(dispatch("Pair", &callback, 1, accepted) == WIO_NATIVE_ABI_OK, "native container recovery failed");
            check(pair.claims == 1, "native callback leaked claim");
            WioNativeAbiRelease(&saved.owner);
            check(machine.invoke(function("HostClear")).succeeded(), "receiver clear failed");
            captured.clear();
            cell.clear();
            pair.replacement.clear();
            check(machine.invoke(function("HostMap")).succeeded(), "map capture failed");
            copy(cell, captured.input.value);
            check(invoke(captured, cell, escapedMap) == WIO_NATIVE_ABI_OK, "map borrow failed");
            captured.clear();
            cell.clear();
            check(machine.invoke(function("HostAny")).succeeded(), "Any capture failed");
            copy(cell, captured.input.value);
            check(invoke(captured, cell, escapedAny) == WIO_NATIVE_ABI_OK, "Any borrow failed");
            captured.clear();
            cell.clear();
            check(machine.invoke(function("HostObject")).succeeded(), "object capture failed");
            copy(cell, captured.input.value);
            {
                Captured otherCapture;
                wio::vm::Machine other{module};
                install(other);
                selected = &otherCapture;
                check(other.invoke(function("HostObject")).succeeded(), "second machine capture failed");
                Wire foreign;
                copy(foreign, otherCapture.input.value);
                Wire rejected;
                check(invoke(captured, foreign, rejected) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                      "cross-machine object reference admitted");
                foreign.clear();
                otherCapture.clear();
                selected = &captured;
            }
            {
                Wire result;
                check(invoke(captured, cell, result) == WIO_NATIVE_ABI_OK &&
                          cell.value.payload.handle.state != captured.input.value.payload.handle.state,
                      "object handle replacement failed");
            }
            cell.clear();
            captured.clear();
            check(machine.invoke(function("HostNullable")).succeeded(), "nullable capture failed");
            copy(cell, captured.input.value);
            {
                Wire result;
                check(invoke(captured, cell, result) == WIO_NATIVE_ABI_OK && cell.value.kind == WIO_NATIVE_ABI_OBJECT,
                      "null-to-object borrow failed");
            }
            cell.clear();
            captured.clear();
            check(machine.invoke(function("HostPanic")).succeeded(), "panic capture failed");
            copy(cell, captured.input.value);
            {
                Wire result;
                check(invoke(captured, cell, result) == WIO_NATIVE_ABI_PANIC, "panic escaped callback boundary");
                readArray(cell.value, 99, 1);
            }
            cell.clear();
            captured.clear();
            check(machine.invoke(function("SafePoint")).succeeded(), "recovery failed");
            check(machine.invoke(function("HostBorrowCapture")).succeeded(), "borrow escape capture setup failed");
            copy(cell, captured.input.value);
            {
                Wire rejected;
                check(invoke(captured, cell, rejected) == WIO_NATIVE_ABI_PANIC,
                      "invocation-scoped container borrow escaped into a closure");
                readArray(cell.value, 99, 1);
            }
            cell.clear();
            captured.clear();
            check(machine.invoke(function("SafePoint")).succeeded(), "borrow capture recovery failed");
            auto balanced = machine.invoke(function("LifecycleBalanced"));
            check(balanced.succeeded() && balanced.value().asBoolean(), "borrowed object lifecycle unbalanced");
            check(machine.invoke(function("HostArray")).succeeded(), "stale capture failed");
            for (auto wire : {&escapedArray, &escapedMap, &escapedAny})
                check(wire->value.owner.ops &&
                          wire->value.owner.ops->typeId(wire->value.owner.state) == wire->value.typeId,
                      "escaped borrow result has no code lease");
        }
        Wire stale;
        copy(stale, captured.input.value);
        Wire result;
        check(invoke(captured, stale, result) == WIO_NATIVE_ABI_STALE_HANDLE, "destroyed machine callback entered");
        stale.clear();
        captured.clear();
        wio::vm::Machine::collectNativeObjectRetirements();
        library.close();
        readArray(escapedArray.value, 9, 2);
        auto map = std::ranges::find(dictionaries->types, dictionaries->types + dictionaries->typeCount,
                                     escapedMap.value.typeId, &WioNativeDictionaryDescriptor::typeId);
        check(map != dictionaries->types + dictionaries->typeCount, "escaped map adapter missing");
        WioNativeAbiValue entries[2]{};
        WioNativeAbiFailure failure{};
        check(map->read(&escapedMap.value, entries, 1, &failure) == WIO_NATIVE_ABI_OK, "escaped map unreadable");
        Wire key, value;
        key.value = entries[0];
        value.value = entries[1];
        readArray(value.value, 9, 2);
        bool found = false;
        for (std::uint32_t i = 0; i < any->payloadCount; ++i)
        {
            Wire payload;
            bool matches = false;
            check(any->payloads[i].read(&escapedAny.value, &payload.value, &matches, &failure) == WIO_NATIVE_ABI_OK,
                  "escaped Any unreadable");
            if (matches && payload.value.kind == WIO_NATIVE_ABI_BYTES)
                found = std::string(static_cast<const char*>(payload.value.payload.slice.data),
                                    payload.value.payload.slice.size) == std::string("changed\0\xF0\x9F\x8C\x8D", 12);
        }
        check(found, "escaped Any bytes changed");
        key.clear();
        value.clear();
        escapedArray.clear();
        escapedMap.clear();
        escapedAny.clear();
        wio::vm::Machine::collectNativeObjectRetirements();
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "host requires bytecode and producer");
        host(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
