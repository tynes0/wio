#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include "native/vm_callback_pod_fixture.h"
#include <wio_native_pod.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
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
        WioNativeAbiValue value;
        ~Wire()
        {
            WioNativeAbiReleaseValue(&value);
        }
    };
    struct Callback final
    {
        WioNativeAbiCallback value;
        void clear()
        {
            if (value.ops && value.ops->release)
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
    WioNativeAbiStatus wrongResult(void* userdata, const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue* result,
                                   WioNativeAbiFailure*) noexcept
    {
        *result = *static_cast<WioNativeAbiValue*>(userdata);
        return WIO_NATIVE_ABI_OK;
    }
    const WioNativeAbiCallbackOps WrongResultOps{WIO_NATIVE_ABI_VERSION, 0, nullptr, nullptr, &wrongResult};

    void host(const char* path, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        const auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "callback POD bytecode decode failed");
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
            const auto found = std::ranges::find_if(module.contract.reflection, [&](const auto& type)
                                                    { return module.string(type.logicalName) == name; });
            check(found != module.contract.reflection.end(), name);
            return *found;
        };
        Wire escaped;
        {
            Library library{producer};
            check(library.handle != nullptr, "callback POD producer load failed");
            const auto registry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry")();
            const auto layouts = library.symbol<WioGetNativePodLayoutApiFn>("WioGetNativePodLayoutApi")();
            std::vector<WioNativeAbiFunctionDescriptor> descriptors(registry->functions,
                                                                    registry->functions + registry->functionCount);
            const auto& binding = module.functions[function("Capture")].nativeBinding;
            const auto selected =
                std::ranges::find_if(descriptors, [&](const auto& descriptor)
                                     { return descriptor.stableKey == module.string(binding.stableKey); });
            check(selected != descriptors.end(), "POD callback capture thunk missing");
            selected->thunk = &capture;
            auto changed = *registry;
            changed.functions = descriptors.data();
            std::string error;
            const auto& callKey = module.functions[function("StoreCall")].nativeBinding.stableKey;
            const auto& callerKey = module.functions[function("StoreCaller")].nativeBinding.stableKey;
            const auto& anyKey = module.functions[function("StoreAny")].nativeBinding.stableKey;
            check(callKey != callerKey && callKey != anyKey && callerKey != anyKey,
                  "same-symbol callback lease/thread contracts collapsed into one thunk identity");
            {
                wio::vm::Machine missing{module};
                check(missing.registerNativeRegistry(changed, &error), error.c_str());
                const auto rejected = missing.invoke(function("HostCapture"));
                check(!rejected.succeeded() && rejected.error().code == "WVM1225",
                      "POD callback admitted without qualified layout sidecar");
                check(!captured.value.ops, "unsupported POD callback entered native code");
            }
            vm_callback_pod::Packet packet = vm_callback_pod::Make();
            WioNativeAbiValue argument;
            argument.kind = WIO_NATIVE_ABI_POD;
            argument.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            argument.typeId = reflection("Packet").stableTypeId;
            argument.payload.slice = {&packet, sizeof(packet)};
            {
                wio::vm::Machine machine{module};
                check(machine.registerNativeRegistry(changed, *layouts, nullptr, nullptr, &error), error.c_str());
                for (const auto* name : {"RefProbe", "ViewProbe"})
                    check(machine.invoke(function(name)).succeeded(), "qualified POD callback borrow rejected");
                for (const auto* name : {"NullableProbe"})
                {
                    const auto rejected = machine.invoke(function(name));
                    check(!rejected.succeeded() && rejected.error().code == "WVM1225",
                          "unqualified borrowed/nullable callback signature accepted");
                    check(machine.invoke(function("SafePoint")).succeeded(), "signature rejection poisoned owner");
                }
                check(machine.invoke(function("HostCapture")).succeeded(), "retained POD capture failed");
                const auto invoke = [&](const WioNativeAbiValue* args, std::uint64_t count, Wire& result)
                {
                    WioNativeAbiFailure failure;
                    return captured.value.ops->invoke(captured.value.userdata, args, count, &result.value, &failure);
                };
                check(invoke(&argument, 1, escaped) == WIO_NATIVE_ABI_OK, "valid retained POD callback rejected");
                check(escaped.value.kind == WIO_NATIVE_ABI_POD && escaped.value.typeId == argument.typeId &&
                          (escaped.value.flags & WIO_NATIVE_ABI_VALUE_OWNED) && escaped.value.owner.ops &&
                          escaped.value.owner.ops->typeId(escaped.value.owner.state) == argument.typeId,
                      "callback POD result lost its stable identity or owner");
                for (unsigned variant = 0; variant < 6; ++variant)
                {
                    auto invalid = argument;
                    switch (variant)
                    {
                    case 0:
                        invalid.typeId = reflection("Twin").stableTypeId;
                        break;
                    case 1:
                        invalid.payload.slice.size -= 1;
                        break;
                    case 2:
                        invalid.payload.slice.data = nullptr;
                        break;
                    case 3:
                        invalid.kind = WIO_NATIVE_ABI_U64;
                        break;
                    case 4:
                        invalid.kind = WIO_NATIVE_ABI_VOID;
                        break;
                    case 5:
                        invalid.typeId = 0;
                        break;
                    }
                    Wire result;
                    check(invoke(&invalid, 1, result) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                          "malformed POD callback input accepted");
                    check(result.value.kind == WIO_NATIVE_ABI_VOID, "failed callback published a value");
                    check(machine.invoke(function("SafePoint")).succeeded(), "POD callback recovery failed");
                }
                Wire countResult;
                check(invoke(&argument, 0, countResult) == WIO_NATIVE_ABI_INVALID_ARGUMENT,
                      "callback accepted incorrect argument count");
                for (int round = 0; round < 20; ++round)
                {
                    Wire result;
                    check(invoke(&argument, 1, result) == WIO_NATIVE_ABI_OK, "POD callback reuse failed");
                }
                // Native's typed callback codec must reject a same-layout POD
                // from a different Wio declaration, in the opposite direction.
                const auto call = std::ranges::find_if(
                    module.functions,
                    [&](const auto& f)
                    {
                        return f.hasNativeBinding &&
                               module.string(f.nativeBinding.symbol).find("::Call") != std::string::npos &&
                               f.returnType == reflection("Packet").type;
                    });
                check(call != module.functions.end(), "typed POD callback Call specialization missing");
                const auto thunk = std::ranges::find_if(
                    descriptors, [&](const auto& descriptor)
                    { return descriptor.stableKey == module.string(call->nativeBinding.stableKey); });
                check(thunk != descriptors.end(), "typed POD callback thunk missing");
                auto wrong = argument;
                wrong.typeId = reflection("Twin").stableTypeId;
                WioNativeAbiValue fake;
                fake.kind = WIO_NATIVE_ABI_CALLBACK;
                fake.payload.callback = {&wrong, &WrongResultOps};
                const std::array args{fake, argument};
                Wire result;
                WioNativeAbiFailure failure;
                check(thunk->thunk(args.data(), args.size(), &result.value, &failure) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                      "native callback codec erased the POD result identity");
                check(machine.invoke(function("PanicCapture")).succeeded(), "panic callback capture failed");
                Wire panic;
                check(invoke(&argument, 1, panic) == WIO_NATIVE_ABI_PANIC,
                      "Wio callback failure crossed native boundary");
                check(machine.invoke(function("SafePoint")).succeeded(), "callback panic poisoned owner machine");
            }
            Wire stale;
            check(captured.value.ops->invoke(captured.value.userdata, &argument, 1, &stale.value, nullptr) ==
                      WIO_NATIVE_ABI_STALE_HANDLE,
                  "retained POD callback re-entered a destroyed machine");
            captured.clear();
        }
        // Producer, callback and machine are gone; the result still owns bytes
        // through VM operations, not through the unloaded producer's Box ops.
        check(escaped.value.payload.slice.size == sizeof(vm_callback_pod::Packet), "escaped POD result size changed");
        vm_callback_pod::Packet result{};
        std::memcpy(&result, escaped.value.payload.slice.data, sizeof(result));
        check(result.cell.value == 16 && result.samples[0] == 18 && result.hidden == 97,
              "callback result storage expired or lost hidden native fields");
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "POD callback host requires bytecode and producer paths");
        host(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
