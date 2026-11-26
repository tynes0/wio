#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include <wio_native_pod.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{
    void check(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    struct Library
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
    };
    struct Callback
    {
        WioNativeAbiCallback value{};
        void clear()
        {
            if (value.ops)
                value.ops->release(value.userdata);
            value = {};
        }
        ~Callback()
        {
            clear();
        }
    } captured;
    WioNativeAbiStatus capture(const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* result,
                               WioNativeAbiFailure*) noexcept
    {
        if (count != 1 || args[0].kind != WIO_NATIVE_ABI_CALLBACK)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        captured.clear();
        captured.value = args[0].payload.callback;
        captured.value.ops->retain(captured.value.userdata);
        *result = {};
        return WIO_NATIVE_ABI_OK;
    }
    struct Wire
    {
        WioNativeAbiValue value{};
        ~Wire()
        {
            WioNativeAbiReleaseValue(&value);
        }
    };
    void host(const char* path, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        const auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "borrow bytecode decode failed");
        const auto& module = decoded.module;
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(module.functions,
                                                    [&](const auto& item) { return module.string(item.name) == name; });
            check(found != module.functions.end(), name);
            return found->id;
        };
        Library library{producer};
        check(library.handle != nullptr, "borrow producer load failed");
        const auto registry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry")();
        const auto pods = library.symbol<WioGetNativePodLayoutApiFn>("WioGetNativePodLayoutApi")();
        std::vector<WioNativeAbiFunctionDescriptor> descriptors(registry->functions,
                                                                registry->functions + registry->functionCount);
        for (const auto* name : {"Capture", "CaptureMixed"})
        {
            const auto key = module.string(module.functions[function(name)].nativeBinding.stableKey);
            const auto found =
                std::ranges::find_if(descriptors, [&](const auto& item) { return item.stableKey == key; });
            check(found != descriptors.end(), "borrow capture descriptor missing");
            found->thunk = &capture;
        }
        auto changed = *registry;
        changed.functions = descriptors.data();
        Wire token;
        token.value.kind = WIO_NATIVE_ABI_I64;
        token.value.payload.signedInteger = 7;
        WioNativeAbiValue arguments[2]{};
        for (auto& argument : arguments)
        {
            argument.kind = WIO_NATIVE_ABI_REFERENCE;
            argument.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            argument.payload.mutableSlice = {&token.value, sizeof(WioNativeAbiValue)};
        }
        arguments[1].flags |= WIO_NATIVE_ABI_VALUE_MUTABLE;
        const auto invoke = [&](WioNativeAbiValue* args, std::uint64_t count, Wire& result)
        {
            WioNativeAbiFailure failure{};
            return captured.value.ops->invoke(captured.value.userdata, args, count, &result.value, &failure);
        };
        {
            wio::vm::Machine machine{module};
            std::string error;
            check(machine.registerNativeRegistry(changed, *pods, nullptr, nullptr, &error), error.c_str());
            check(machine.invoke(function("HostCapture")).succeeded(), "borrow capture failed");
            {
                Wire result;
                check(invoke(arguments, 2, result) == WIO_NATIVE_ABI_OK, "valid borrow rejected");
                check(result.value.kind == WIO_NATIVE_ABI_I64 && result.value.payload.signedInteger == 12 &&
                          token.value.payload.signedInteger == 12,
                      "readonly/mutable alias or copy-back lost");
            }
            for (unsigned variant = 0; variant < 8; ++variant)
            {
                WioNativeAbiValue invalid[2]{arguments[0], arguments[1]};
                switch (variant)
                {
                case 0:
                    invalid[1].kind = WIO_NATIVE_ABI_I64;
                    break;
                case 1:
                    invalid[1].flags = WIO_NATIVE_ABI_VALUE_BORROWED;
                    break;
                case 2:
                    invalid[1].payload.mutableSlice.size -= 1;
                    break;
                case 3:
                    invalid[1].payload.mutableSlice.data = nullptr;
                    break;
                case 4:
                    invalid[1].payload.mutableSlice.data = reinterpret_cast<char*>(&token.value) + 1;
                    break;
                case 5:
                    invalid[1].flags |= WIO_NATIVE_ABI_VALUE_OWNED;
                    break;
                case 6:
                    invalid[1].typeId = 999;
                    break;
                case 7:
                    token.value.kind = WIO_NATIVE_ABI_BOOL;
                    break;
                }
                Wire result;
                check(invoke(invalid, 2, result) == WIO_NATIVE_ABI_TYPE_MISMATCH, "malformed borrow accepted");
                check(result.value.kind == WIO_NATIVE_ABI_VOID, "failed admission published result");
                token.value.kind = WIO_NATIVE_ABI_I64;
                check(token.value.payload.signedInteger == 12, "failed admission modified input");
                check(machine.invoke(function("SafePoint")).succeeded(), "borrow rejection poisoned machine");
            }
            {
                Wire result;
                check(invoke(arguments, 1, result) == WIO_NATIVE_ABI_INVALID_ARGUMENT, "wrong arity accepted");
            }
            check(machine.invoke(function("HostFailure")).succeeded(), "panic callback capture failed");
            {
                Wire result;
                check(invoke(arguments, 2, result) == WIO_NATIVE_ABI_PANIC, "Wio panic not contained");
                check(token.value.payload.signedInteger == 23, "panic discarded completed borrow writes");
            }
            check(machine.invoke(function("SafePoint")).succeeded(), "panic poisoned machine");
            check(machine.invoke(function("HostEscape")).succeeded(), "escape callback capture failed");
            {
                Wire result;
                check(invoke(arguments, 2, result) == WIO_NATIVE_ABI_PANIC, "callback borrow escaped in a closure");
                check(token.value.payload.signedInteger == 23, "rejected borrow capture modified caller cell");
            }
            check(machine.invoke(function("SafePoint")).succeeded(), "capture rejection poisoned machine");
            check(machine.invoke(function("HostMixed")).succeeded(), "mixed callback capture failed");
            {
                Wire result;
                WioNativeAbiValue mixed[2]{arguments[1], arguments[0]};
                check(invoke(mixed, 2, result) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                      "aliased scalar cells with different concrete types accepted");
                check(token.value.payload.signedInteger == 23, "mixed alias rejection ran callback body");
            }
            check(machine.invoke(function("HostCapture")).succeeded(), "callback recapture failed");
            token.value.payload.signedInteger = std::numeric_limits<std::int64_t>::max();
            {
                Wire result;
                check(invoke(arguments, 2, result) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                      "out-of-range scalar borrow accepted");
                check(token.value.payload.signedInteger == std::numeric_limits<std::int64_t>::max(),
                      "range rejection modified caller cell");
            }
        }
        Wire stale;
        check(invoke(arguments, 2, stale) == WIO_NATIVE_ABI_STALE_HANDLE, "retired callback kept borrowed VM alive");
        captured.clear();
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "expected bytecode and producer paths");
        host(argv[1], argv[2]);
        std::cout << "callback-borrow-host-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
