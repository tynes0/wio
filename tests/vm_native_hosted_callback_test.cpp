#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include <wio_native_object.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
    void check(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    struct Wire final
    {
        WioNativeAbiValue value{};
        ~Wire()
        {
            WioNativeAbiReleaseValue(&value);
        }
    };
    struct Callback final
    {
        unsigned references = 1;
        unsigned calls = 0;
        unsigned behavior = 0;
        static const WioNativeAbiCallbackOps Ops;
    };
    const WioNativeAbiCallbackOps Callback::Ops{
        WIO_NATIVE_ABI_VERSION, WIO_NATIVE_ABI_CALLBACK_CALLER_THREAD,
        [](void* state) noexcept { ++static_cast<Callback*>(state)->references; },
        [](void* state) noexcept { --static_cast<Callback*>(state)->references; },
        [](void* state, const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* output,
           WioNativeAbiFailure*) noexcept
        {
            auto& callback = *static_cast<Callback*>(state);
            ++callback.calls;
            if (count != 1 || args[0].kind != WIO_NATIVE_ABI_I64)
                return WIO_NATIVE_ABI_TYPE_MISMATCH;
            if (callback.behavior == 1)
                return WIO_NATIVE_ABI_EXCEPTION;
            *output = {};
            output->kind = callback.behavior == 2 ? WIO_NATIVE_ABI_BOOL : WIO_NATIVE_ABI_I64;
            output->payload.signedInteger = args[0].payload.signedInteger + 2;
            return WIO_NATIVE_ABI_OK;
        }};

    void host(const char* bytecodePath, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(bytecodePath), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        const auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "hosted callback bytecode decode failed");
        const auto& module = decoded.module;
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(module.functions,
                                                    [&](const auto& item) { return module.string(item.name) == name; });
            check(found != module.functions.end(), name);
            return found->id;
        };
        const auto reflected = std::ranges::find_if(module.contract.reflection, [&](const auto& item)
                                                    { return module.string(item.logicalName) == "Processor"; });
        check(reflected != module.contract.reflection.end(), "hosted processor reflection missing");
        const auto method = [&](const char* name) -> const wio::bytecode::ReflectedMethod&
        {
            const auto found = std::ranges::find_if(reflected->methods,
                                                    [&](const auto& item) { return module.string(item.name) == name; });
            check(found != reflected->methods.end(), name);
            return *found;
        };
        const auto callbackType = method("Apply").parameterTypes[0];
        const auto callbackIdentity =
            std::ranges::find(module.contract.reflection, callbackType, &wio::bytecode::Reflection::type);
        check(callbackIdentity != module.contract.reflection.end(), "method-only callback identity missing");
        WioNativeObjectHost saved{};
        {
            wio::vm::Machine machine{module};
            std::string error;
            check(machine.loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
            const auto entry = machine.invoke(function("Entry"));
            if (!entry.succeeded())
                throw std::runtime_error(entry.error().code + ": " + entry.error().message);
            check(entry.value().asSignedInteger() == 0, "hosted callback differential entry failed");
            const auto panic = machine.invoke(function("HostPanic"));
            check(!panic.succeeded() && panic.error().code == "WVM1222", "native callback exception crossed boundary");
            const auto balanced = [&]
            {
                const auto result = machine.invoke(function("LifecycleBalanced"));
                check(result.succeeded() && result.value().asBoolean(), "hosted callback lifecycle leaked");
            };
            balanced();
            const auto unsupported = machine.invoke(function("HostUnsupported"));
            check(!unsupported.succeeded() && unsupported.error().code == "WVM1223",
                  "hosted ref/view callback was advertised without a borrow contract");
            const auto capture = machine.invoke(function("HostCapture"));
            check(capture.succeeded(), "hosted callback receiver capture failed");
            saved = *reinterpret_cast<const WioNativeObjectHost*>(
                static_cast<std::uintptr_t>(capture.value().asUnsignedInteger()));
            WioNativeAbiRetain(saved.owner);
            Callback state;
            WioNativeAbiValue wire{};
            wire.kind = WIO_NATIVE_ABI_CALLBACK;
            wire.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            wire.typeId = callbackIdentity->stableTypeId;
            wire.payload.callback = {&state, &Callback::Ops};
            const auto invoke = [&](const WioNativeAbiValue& argument, const char* name)
            {
                Wire result;
                WioNativeAbiFailure failure{};
                const auto status = saved.invoke(&saved, method(name).stableId, &argument, 1, &result.value, &failure);
                if (status == WIO_NATIVE_ABI_OK)
                    check(result.value.kind == WIO_NATIVE_ABI_I64 && result.value.payload.signedInteger == 9,
                          "hosted callable result changed");
                check(state.references == 1, "hosted method retained native callable after return");
                return status;
            };
            check(invoke(wire, "Apply") == WIO_NATIVE_ABI_OK && invoke(wire, "Forwarded") == WIO_NATIVE_ABI_OK,
                  "native callable direct/forwarded dispatch failed");
            const auto calls = state.calls;
            for (unsigned variant = 0; variant < 10; ++variant)
            {
                auto bad = wire;
                auto badOps = Callback::Ops;
                switch (variant)
                {
                case 0:
                    bad.typeId = 0;
                    break;
                case 1:
                    bad.kind = WIO_NATIVE_ABI_U64;
                    break;
                case 2:
                    bad.payload.callback.userdata = nullptr;
                    break;
                case 3:
                    bad.payload.callback.ops = nullptr;
                    break;
                case 4:
                    badOps.abiVersion = 0;
                    bad.payload.callback.ops = &badOps;
                    break;
                case 5:
                    badOps.retain = nullptr;
                    bad.payload.callback.ops = &badOps;
                    break;
                case 6:
                    badOps.flags = 0x80000000u;
                    bad.payload.callback.ops = &badOps;
                    break;
                case 7:
                    bad.flags = WIO_NATIVE_ABI_VALUE_NULLABLE;
                    break;
                case 8:
                    bad.owner.state = &state;
                    break;
                case 9:
                    badOps.release = nullptr;
                    bad.payload.callback.ops = &badOps;
                    break;
                }
                check(invoke(bad, "Apply") == WIO_NATIVE_ABI_TYPE_MISMATCH, "malformed hosted callback accepted");
            }
            check(state.calls == calls, "malformed hosted callback entered producer");
            for (state.behavior = 1; state.behavior <= 2; ++state.behavior)
                check(invoke(wire, "Apply") == WIO_NATIVE_ABI_PANIC,
                      "native callback failure/result mismatch escaped method boundary");
            state.behavior = 0;
            check(invoke(wire, "Apply") == WIO_NATIVE_ABI_OK, "hosted callback failed to recover");
            {
                Wire result;
                WioNativeAbiFailure failure{};
                check(saved.invoke(&saved, method("Remember").stableId, &wire, 1, &result.value, &failure) ==
                              WIO_NATIVE_ABI_OK &&
                          state.references == 2,
                      "method-retained native callable lost its owning claim");
                const auto run = machine.invoke(function("HostRunStored"));
                check(run.succeeded() && run.value().asSignedInteger() == 9,
                      "native callable failed after hosted method returned");
                check(machine.invoke(function("HostClearStored")).succeeded() && state.references == 1,
                      "retained hosted callable did not release at VM safe point");
            }
            std::atomic<WioNativeAbiStatus> wrongThread{WIO_NATIVE_ABI_OK};
            std::thread worker(
                [&]
                {
                    Wire result;
                    WioNativeAbiFailure failure{};
                    wrongThread = saved.invoke(&saved, method("Apply").stableId, &wire, 1, &result.value, &failure);
                });
            worker.join();
            check(wrongThread == WIO_NATIVE_ABI_WRONG_THREAD, "hosted dispatch lost receiver affinity");
            check(machine.invoke(function("HostClear")).succeeded(), "hosted receiver release failed");
            // saved.owner still deliberately owns the receiver until shutdown.
        }
        Wire result;
        WioNativeAbiFailure failure{};
        check(saved.invoke(&saved, method("Apply").stableId, nullptr, 0, &result.value, &failure) ==
                  WIO_NATIVE_ABI_STALE_HANDLE,
              "hosted callback receiver survived owner shutdown");
        WioNativeAbiRelease(&saved.owner);
        wio::vm::Machine::collectNativeObjectRetirements();
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "hosted callback host requires bytecode and producer paths");
        host(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
