#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include <wio_native_object.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
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
            if (callback.ops && callback.ops->release)
                callback.ops->release(callback.userdata);
            callback = {};
            input.clear();
        }
        ~Captured()
        {
            clear();
        }
    };
    Captured* selectedCapture;
    WioNativeAbiStatus capture(const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* result,
                               WioNativeAbiFailure*) noexcept
    {
        if (count != 2 || args[0].kind != WIO_NATIVE_ABI_CALLBACK || !selectedCapture)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        auto& target = *selectedCapture;
        target.clear();
        target.callback = args[0].payload.callback;
        target.callback.ops->retain(target.callback.userdata);
        target.input.value = args[1];
        if (args[1].kind == WIO_NATIVE_ABI_OBJECT)
        {
            target.input.value.payload.handle.ops->retain(target.input.value.payload.handle.state);
            target.input.value.flags |= WIO_NATIVE_ABI_VALUE_OWNED;
        }
        *result = {};
        return WIO_NATIVE_ABI_OK;
    }
    WioNativeAbiStatus invoke(Captured& captured, const WioNativeAbiValue& input, Wire& output)
    {
        WioNativeAbiFailure failure;
        return captured.callback.ops->invoke(captured.callback.userdata, &input, 1, &output.value, &failure);
    }
    void host(const char* path, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        const auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "object callback bytecode decode failed");
        const auto& module = decoded.module;
        const auto function = [&](const char* name)
        {
            const auto found =
                std::ranges::find_if(module.functions, [&](const auto& f) { return module.string(f.name) == name; });
            check(found != module.functions.end(), name);
            return found->id;
        };
        const auto reflection = [&](const char* name) -> const wio::bytecode::Reflection&
        {
            const auto found = std::ranges::find_if(module.contract.reflection, [&](const auto& entry)
                                                    { return module.string(entry.logicalName) == name; });
            check(found != module.contract.reflection.end(), name);
            return *found;
        };
        Library library{producer};
        check(library.handle != nullptr, "object callback producer load failed");
        const auto registry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry")();
        const auto objects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi")();
        std::vector<WioNativeAbiFunctionDescriptor> descriptors(registry->functions,
                                                                registry->functions + registry->functionCount);
        for (const auto* name : {"Capture", "CaptureNullable", "CaptureCaller"})
        {
            const auto& binding = module.functions[function(name)].nativeBinding;
            const auto found =
                std::ranges::find_if(descriptors, [&](const auto& descriptor)
                                     { return descriptor.stableKey == module.string(binding.stableKey); });
            check(found != descriptors.end(), "object callback capture thunk missing");
            found->thunk = &capture;
        }
        auto changed = *registry;
        changed.functions = descriptors.data();
        const auto install = [&](wio::vm::Machine& machine)
        {
            std::string error;
            check(machine.registerNativeRegistry(changed, *objects, &error), error.c_str());
        };
        const auto safe = [&](wio::vm::Machine& machine)
        { check(machine.invoke(function("SafePoint")).succeeded(), "object callback recovery failed"); };
        const auto balanced = [&](wio::vm::Machine& machine)
        {
            const auto result = machine.invoke(function("LifecycleBalanced"));
            check(result.succeeded() && result.value().asBoolean(), "object callback leaked Wio lifecycle ownership");
        };
        {
            wio::vm::Machine missing{module};
            std::string error;
            check(missing.registerNativeRegistry(changed, &error), error.c_str());
            const auto rejected = missing.invoke(function("HostNullable"));
            check(!rejected.succeeded() && rejected.error().code == "WVM1225",
                  "object callback admitted without proxy contracts");
        }
        Captured captured;
        Wire escaped;
        {
            wio::vm::Machine machine{module};
            install(machine);
            const auto entry = machine.invoke(function("Entry"));
            if (!entry.succeeded())
                throw std::runtime_error(entry.error().code + ": " + entry.error().message);
            check(entry.value().asSignedInteger() == 0, "object callback host entry failed");
            balanced(machine);
            selectedCapture = &captured;
            check(machine.invoke(function("HostCapture")).succeeded(), "retained object capture failed");
            Wire badCount;
            WioNativeAbiFailure badCountFailure;
            check(captured.callback.ops->invoke(captured.callback.userdata, nullptr, 0, &badCount.value,
                                                &badCountFailure) == WIO_NATIVE_ABI_INVALID_ARGUMENT,
                  "object callback accepted a missing argument");
            check(invoke(captured, captured.input.value, escaped) == WIO_NATIVE_ABI_OK,
                  "retained object callback failed");
            const auto adapter = std::ranges::find(objects->types, objects->types + objects->typeCount,
                                                   escaped.value.typeId, &WioNativeObjectDescriptor::typeId);
            check(adapter != objects->types + objects->typeCount, "callback result has no concrete adapter");
            WioNativeObjectHost token;
            WioNativeAbiFailure failure;
            check(adapter->host(&escaped.value, &token, &failure) == WIO_NATIVE_ABI_OK, "callback result host missing");
            const auto& type = reflection("Item");
            const auto read = std::ranges::find_if(type.methods, [&](const auto& method)
                                                   { return module.string(method.name) == "Read"; });
            check(read != type.methods.end(), "callback result Read metadata missing");
            Wire value;
            check(token.invoke(&token, read->stableId, nullptr, 0, &value.value, &failure) == WIO_NATIVE_ABI_OK &&
                      value.value.kind == WIO_NATIVE_ABI_I64 && value.value.payload.signedInteger == 50,
                  "callback result identity or method dispatch lost");
            for (unsigned variant = 0; variant < 6; ++variant)
            {
                auto malformed = captured.input.value;
                switch (variant)
                {
                case 0:
                    malformed.typeId = reflection("Other").stableTypeId;
                    break;
                case 1:
                    malformed.payload.handle.generation = 0;
                    break;
                case 2:
                    malformed.payload.handle.ops = nullptr;
                    break;
                case 3:
                    malformed.kind = WIO_NATIVE_ABI_U64;
                    break;
                case 4:
                    malformed.kind = WIO_NATIVE_ABI_VOID;
                    malformed.flags = WIO_NATIVE_ABI_VALUE_NULLABLE;
                    break;
                case 5:
                    malformed.typeId = 0;
                    break;
                }
                Wire result;
                check(invoke(captured, malformed, result) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                      "malformed object callback argument accepted");
                safe(machine);
            }
            {
                Captured foreign;
                wio::vm::Machine other{module};
                install(other);
                selectedCapture = &foreign;
                check(other.invoke(function("HostCapture")).succeeded(), "second machine capture failed");
                Wire result;
                check(invoke(captured, foreign.input.value, result) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                      "callback accepted an object hosted by another machine");
                foreign.clear();
                safe(other);
            }
            escaped.clear();
            // A second native lease must keep the capture alive after the
            // original lease is released. Final foreign-thread release queues
            // Wio cleanup; only a machine safe point executes destructors.
            Captured copy;
            copy.callback = captured.callback;
            copy.callback.ops->retain(copy.callback.userdata);
            copy.input.value = captured.input.value;
            copy.input.value.payload.handle.ops->retain(copy.input.value.payload.handle.state);
            copy.input.value.flags |= WIO_NATIVE_ABI_VALUE_OWNED;
            captured.clear();
            Wire copiedResult;
            check(invoke(copy, copy.input.value, copiedResult) == WIO_NATIVE_ABI_OK,
                  "releasing one native lease expired the remaining callback");
            copiedResult.clear();
            std::thread releaser([&] { copy.clear(); });
            releaser.join();
            balanced(machine);
            selectedCapture = &captured;
            check(machine.invoke(function("HostNullable")).succeeded(), "nullable callback capture failed");
            Wire nullResult;
            check(invoke(captured, captured.input.value, nullResult) == WIO_NATIVE_ABI_OK &&
                      nullResult.value.kind == WIO_NATIVE_ABI_VOID &&
                      nullResult.value.flags == WIO_NATIVE_ABI_VALUE_NULLABLE,
                  "nullable callback failed null round trip");
            auto invalidNull = captured.input.value;
            invalidNull.flags = 0;
            Wire failedNull;
            check(invoke(captured, invalidNull, failedNull) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                  "nullable callback accepted untagged null");
            captured.clear();
            balanced(machine);
            check(machine.invoke(function("HostCaller")).succeeded(), "caller-affine object capture failed");
            WioNativeAbiStatus foreignStatus = WIO_NATIVE_ABI_OK;
            std::thread wrongThread(
                [&]
                {
                    Wire result;
                    foreignStatus = invoke(captured, captured.input.value, result);
                });
            wrongThread.join();
            check(foreignStatus == WIO_NATIVE_ABI_WRONG_THREAD,
                  "caller-affine object callback entered on a worker thread");
            Wire callerResult;
            check(invoke(captured, captured.input.value, callerResult) == WIO_NATIVE_ABI_OK,
                  "wrong-thread rejection poisoned the caller callback");
            callerResult.clear();
            captured.clear();
            balanced(machine);
            check(machine.invoke(function("PanicCapture")).succeeded(), "panic capture failed");
            Wire panic;
            check(invoke(captured, captured.input.value, panic) == WIO_NATIVE_ABI_PANIC,
                  "object callback failure crossed native boundary");
            captured.clear();
            balanced(machine);
            check(machine.invoke(function("FailingCleanup")).succeeded(), "failing capture setup failed");
            const auto cleanup = machine.invoke(function("ReleaseStored"));
            check(!cleanup.succeeded() && cleanup.error().code == "WVM1211" && !cleanup.error().cleanupFailures.empty(),
                  "callback capture destructor failure was lost");
            balanced(machine);
            check(machine.invoke(function("HostCapture")).succeeded(), "shutdown capture failed");
            check(invoke(captured, captured.input.value, escaped) == WIO_NATIVE_ABI_OK,
                  "shutdown escaped object callback failed");
        }
        Wire stale;
        check(invoke(captured, captured.input.value, stale) == WIO_NATIVE_ABI_STALE_HANDLE,
              "callback entered a destroyed object owner machine");
        escaped.clear();
        captured.clear();
        wio::vm::Machine observer{module};
        install(observer);
        balanced(observer);
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "object callback host requires bytecode and producer paths");
        host(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
