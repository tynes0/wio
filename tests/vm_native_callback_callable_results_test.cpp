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
#include <thread>

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
    WioNativeAbiCallback retained{};
    WioNativeAbiStatus capture(const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* result,
                               WioNativeAbiFailure*) noexcept
    {
        if (count != 1 || args[0].kind != WIO_NATIVE_ABI_CALLBACK)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        if (retained.ops)
            retained.ops->release(retained.userdata);
        retained = args[0].payload.callback;
        retained.ops->retain(retained.userdata);
        *result = {};
        return WIO_NATIVE_ABI_OK;
    }
    void host(const char* path, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "callable bytecode decode failed");
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
        std::vector<WioNativeAbiFunctionDescriptor> descriptors(registry->functions,
                                                                registry->functions + registry->functionCount);
        // Reserve before storing c_str pointers in registry descriptors.
        module.strings.reserve(module.strings.size() + 3);
        for (const char* name : {"Capture", "CaptureAny", "CaptureCall"})
        {
            auto& binding = module.functions[function(name)].nativeBinding;
            const auto found = std::ranges::find_if(descriptors, [&](const auto& item)
                                                    { return item.stableKey == module.string(binding.stableKey); });
            check(found != descriptors.end(), "capture thunk missing");
            module.strings.push_back(std::string(module.string(binding.stableKey)) + ":closure-host:" + name);
            binding.stableKey = static_cast<std::uint32_t>(module.strings.size() - 1);
            found->stableKey = module.strings[binding.stableKey].c_str();
            found->thunk = &capture;
        }
        auto changed = *registry;
        changed.functions = descriptors.data();
        WioNativeAbiCallback escaped{};
        {
            wio::vm::Machine machine{module};
            std::string error;
            check(machine.loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
            check(machine.registerNativeRegistry(changed, *objects, &error), error.c_str());
            const auto entry = machine.invoke(function("Entry"));
            if (!entry.succeeded())
                throw std::runtime_error(entry.error().code + ": " + entry.error().message);
            check(entry.value().asSignedInteger() == 0, "host entry failed");
            check(machine.invoke(function("HostCapture")).succeeded(), "callback capture failed");
            WioNativeAbiStatus foreign = WIO_NATIVE_ABI_OK;
            std::thread wrongThread{[&]
                                    {
                                        WioNativeAbiValue output{};
                                        WioNativeAbiFailure failure{};
                                        foreign =
                                            retained.ops->invoke(retained.userdata, nullptr, 0, &output, &failure);
                                        WioNativeAbiReleaseValue(&output);
                                    }};
            wrongThread.join();
            check(foreign == WIO_NATIVE_ABI_WRONG_THREAD, "caller-affine callback entered a foreign thread");
            for (unsigned round = 0; round < 30; ++round)
            {
                WioNativeAbiValue cell{};
                cell.kind = WIO_NATIVE_ABI_I64;
                cell.payload.signedInteger = 7;
                WioNativeAbiValue argument{};
                argument.kind = WIO_NATIVE_ABI_REFERENCE;
                argument.flags = WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_MUTABLE;
                argument.payload.mutableSlice = {&cell, sizeof(cell)};
                Wire result;
                WioNativeAbiFailure failure{};
                check(retained.ops->invoke(retained.userdata, &argument, 1, &result.value, &failure) ==
                          WIO_NATIVE_ABI_OK,
                      "retained borrow-result callback failed");
                check(cell.payload.signedInteger == 9 && result.value.kind == WIO_NATIVE_ABI_REFERENCE &&
                          result.value.payload.mutableSlice.data == &cell,
                      "borrowed result escaped a temporary cell");
                argument.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
                check(retained.ops->invoke(retained.userdata, &argument, 1, &result.value, &failure) !=
                          WIO_NATIVE_ABI_OK,
                      "readonly caller cell upgraded to mutable");
                argument.flags |= WIO_NATIVE_ABI_VALUE_MUTABLE;
                argument.typeId = 123456;
                check(retained.ops->invoke(retained.userdata, &argument, 1, &result.value, &failure) !=
                          WIO_NATIVE_ABI_OK,
                      "wrong borrow identity accepted");
            }
            check(machine.invoke(function("HostCaptureCall")).succeeded(), "call-scoped capture failed");
            {
                Wire result;
                WioNativeAbiFailure failure{};
                check(retained.ops->invoke(retained.userdata, nullptr, 0, &result.value, &failure) ==
                          WIO_NATIVE_ABI_STALE_HANDLE,
                      "call-scoped callback remained live after native return");
            }
            check(machine.invoke(function("HostCaptureAny")).succeeded(), "any-thread capture failed");
            std::thread worker{
                [&]
                {
                    WioNativeAbiValue cell{};
                    cell.kind = WIO_NATIVE_ABI_I64;
                    cell.payload.signedInteger = 7;
                    WioNativeAbiValue argument{};
                    argument.kind = WIO_NATIVE_ABI_REFERENCE;
                    argument.flags = WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_MUTABLE;
                    argument.payload.mutableSlice = {&cell, sizeof(cell)};
                    Wire result;
                    WioNativeAbiFailure failure{};
                    foreign = retained.ops->invoke(retained.userdata, &argument, 1, &result.value, &failure);
                    if (cell.payload.signedInteger != 9 || result.value.payload.mutableSlice.data != &cell)
                        foreign = WIO_NATIVE_ABI_TYPE_MISMATCH;
                }};
            worker.join();
            check(foreign == WIO_NATIVE_ABI_OK, "retained any-thread borrowed result failed");
            const auto ready = machine.invoke(function("HostReady"));
            check(ready.succeeded(), "native owning callable async capture failed");
            const auto completed = machine.wait(ready.value());
            check(completed.succeeded() && completed.value().asSignedInteger() == 18,
                  "owning callable lost producer lifetime across suspension");
            const auto pending = machine.invoke(function("HostPending"));
            check(pending.succeeded() && machine.cancel(pending.value()), "owning callable task cancellation failed");
            const auto cancelled = machine.wait(pending.value());
            check(!cancelled.succeeded() && cancelled.error().code == "WVM1165",
                  "cancelled callback task resumed its owning callable");
            const auto balanced = machine.invoke(function("HostBalanced"));
            check(balanced.succeeded() && balanced.value().asBoolean(),
                  "coroutine completion/cancellation leaked an owning callback capture");
            const auto created = machine.invoke(function("HostMake"));
            check(created.succeeded(), "native callable result conversion failed");
            escaped = created.value().nativeCallback();
            check(escaped.ops != nullptr, "returned callback operations missing");
            escaped.ops->retain(escaped.userdata);
            WioNativeAbiValue argument{};
            argument.kind = WIO_NATIVE_ABI_I64;
            argument.payload.signedInteger = 11;
            Wire result;
            WioNativeAbiFailure failure{};
            check(escaped.ops->invoke(escaped.userdata, &argument, 1, &result.value, &failure) == WIO_NATIVE_ABI_OK &&
                      result.value.payload.signedInteger == 18,
                  "returned callback lost closure captures");
            check(machine.invoke(function("HostClear")).succeeded(), "retained producer cleanup failed");
        }
        WioNativeAbiValue argument{};
        argument.kind = WIO_NATIVE_ABI_I64;
        argument.payload.signedInteger = 11;
        Wire result;
        WioNativeAbiFailure failure{};
        check(escaped.ops->invoke(escaped.userdata, &argument, 1, &result.value, &failure) ==
                  WIO_NATIVE_ABI_STALE_HANDLE,
              "returned callback entered destroyed machine");
        escaped.ops->release(escaped.userdata);
        WioNativeAbiValue cell{};
        cell.kind = WIO_NATIVE_ABI_I64;
        cell.payload.signedInteger = 7;
        argument = {};
        argument.kind = WIO_NATIVE_ABI_REFERENCE;
        argument.flags = WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_MUTABLE;
        argument.payload.mutableSlice = {&cell, sizeof(cell)};
        check(retained.ops->invoke(retained.userdata, &argument, 1, &result.value, &failure) ==
                  WIO_NATIVE_ABI_STALE_HANDLE,
              "retained borrow callback entered destroyed machine");
        retained.ops->release(retained.userdata);
        retained = {};
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "expected bytecode and producer paths");
        host(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
