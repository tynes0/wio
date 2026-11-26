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

    WioNativeAbiStatus wrongCount(const WioNativeAbiValue*, std::uint64_t* count, WioNativeAbiFailure*) noexcept
    {
        *count = 2;
        return WIO_NATIVE_ABI_OK;
    }

    void host(const char* path, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "fixed-array bytecode decode failed");
        auto module = std::move(decoded.module);
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(module.functions, [&](const auto& value)
                                                    { return module.string(value.name) == name; });
            check(found != module.functions.end(), name);
            return found->id;
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
        auto& binding = module.functions[function("Capture")].nativeBinding;
        const auto found = std::ranges::find_if(descriptors, [&](const auto& item)
                                                { return item.stableKey == module.string(binding.stableKey); });
        check(found != descriptors.end(), "capture thunk missing");
        module.strings.push_back(std::string(module.string(binding.stableKey)) + ":fixed-host");
        binding.stableKey = static_cast<std::uint32_t>(module.strings.size() - 1);
        found->stableKey = module.strings[binding.stableKey].c_str();
        found->thunk = &capture;
        auto changed = *registry;
        changed.functions = descriptors.data();
        const auto tripleType = module.functions[function("Capture")].parameters[1].type;
        const auto reflected =
            std::ranges::find(module.contract.reflection, tripleType, &wio::bytecode::Reflection::type);
        check(reflected != module.contract.reflection.end(), "fixed-array identity missing");
        const auto adapter = std::ranges::find(sequences->types, sequences->types + sequences->typeCount,
                                               reflected->stableTypeId, &WioNativeSequenceDescriptor::typeId);
        check(adapter != sequences->types + sequences->typeCount, "fixed-array adapter missing");
        const auto stableAdapter = *adapter;
        const auto readTriple = [&](const WioNativeAbiValue& wire, std::array<std::int64_t, 3> expected)
        {
            std::uint64_t count = 0;
            WioNativeAbiFailure failure{};
            check(stableAdapter.count(&wire, &count, &failure) == WIO_NATIVE_ABI_OK && count == 3,
                  "fixed-array extent changed");
            for (std::uint64_t index = 0; index < count; ++index)
            {
                Wire element;
                check(stableAdapter.read(&wire, index, &element.value, &failure) == WIO_NATIVE_ABI_OK &&
                          element.value.kind == WIO_NATIVE_ABI_I64 &&
                          element.value.payload.signedInteger == expected[index],
                      "fixed-array payload changed");
            }
        };
        const auto install = [&](wio::vm::Machine& machine)
        {
            std::string error;
            check(machine.loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
            check(machine.registerNativeRegistry(changed, *objects, &error, any, dictionaries, sequences, pods),
                  error.c_str());
        };
        {
            wio::vm::Machine missing{module};
            std::string error;
            check(missing.registerNativeRegistry(changed, &error), error.c_str());
            const auto result = missing.invoke(function("HostCapture"));
            check(!result.succeeded() && result.error().code == "WVM1225", "fixed callback admitted without adapter");
        }
        Captured captured;
        Wire escaped;
        {
            wio::vm::Machine machine{module};
            install(machine);
            selected = &captured;
            const auto entry = machine.invoke(function("Entry"));
            if (!entry.succeeded())
                throw std::runtime_error(entry.error().code + ": " + entry.error().message);
            check(entry.value().asSignedInteger() == 0, "fixed host entry failed");
            check(machine.invoke(function("HostCapture")).succeeded(), "fixed callback capture failed");
            for (unsigned round = 0; round < 30; ++round)
            {
                Wire cell, result;
                copy(cell, captured.input.value);
                check(invoke(captured, cell, result) == WIO_NATIVE_ABI_OK, "fixed callback invocation failed");
                readTriple(cell.value, {9, 23, 29});
                readTriple(result.value, {9, 23, 29});
                readTriple(captured.input.value, {7, 11, 13});
                copy(escaped, result.value);
            }
            for (unsigned variant = 0; variant < 6; ++variant)
            {
                auto bad = captured.input.value;
                if (variant == 0)
                    bad.typeId ^= 1;
                if (variant == 1)
                    bad.kind = WIO_NATIVE_ABI_POD;
                if (variant == 2)
                    bad.payload.handle.state = nullptr;
                if (variant == 3)
                    bad.payload.handle.ops = nullptr;
                if (variant == 4)
                    bad.typeId = 0;
                if (variant == 5)
                    bad.kind = WIO_NATIVE_ABI_VOID;
                Wire cell, rejected;
                copy(cell, captured.input.value);
                auto args = references(cell);
                args[0].payload.mutableSlice.data = &bad;
                args[1].payload.mutableSlice.data = &bad;
                WioNativeAbiFailure failure{};
                check(captured.callback.ops->invoke(captured.callback.userdata, args.data(), args.size(),
                                                    &rejected.value, &failure) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                      "malformed fixed wire admitted");
                readTriple(cell.value, {7, 11, 13});
            }
            const std::array badArguments{
                wio::vm::Value::array({wio::vm::Value::signedInteger(7), wio::vm::Value::signedInteger(11)})};
            const auto rejected = machine.invoke(function("HostEcho"), badArguments);
            check(!rejected.succeeded(), "VM accepted a fixed-array value with the wrong extent");
            if (rejected.error().message.find("extent") == std::string::npos)
                throw std::runtime_error(rejected.error().code + ": " + rejected.error().message);
            check(machine.invoke(function("Round")).succeeded(), "recovery after bad fixed extent failed");
            captured.clear();
        }
        // A malicious producer reporting a different count is rejected before
        // element decoding, even with an otherwise valid owning array handle.
        {
            std::vector<WioNativeSequenceDescriptor> broken(sequences->types, sequences->types + sequences->typeCount);
            auto target = std::ranges::find(broken, stableAdapter.typeId, &WioNativeSequenceDescriptor::typeId);
            target->count = &wrongCount;
            auto badApi = *sequences;
            badApi.types = broken.data();
            wio::vm::Machine machine{module};
            std::string error;
            check(machine.registerNativeRegistry(changed, *objects, &error, any, dictionaries, &badApi, pods),
                  error.c_str());
            selected = &captured;
            check(machine.invoke(function("HostCapture")).succeeded(), "malformed-count capture failed");
            Wire cell, result;
            copy(cell, captured.input.value);
            check(invoke(captured, cell, result) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                  "producer fixed-array count mismatch admitted");
            readTriple(cell.value, {7, 11, 13});
            captured.clear();
        }
        selected = nullptr;
        library.close();
        // The returned wire owns a producer-code lease after both Machine and
        // the explicitly loaded DLL have gone away.
        readTriple(escaped.value, {9, 23, 29});
        escaped.clear();
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "expected bytecode and native producer");
        host(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
