#include "wio/bytecode/codec.h"
#include "wio/bytecode/verifier.h"
#include "wio/vm/machine.h"

#include <wio_native_abi.h>

#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <latch>
#include <thread>
#include <vector>

namespace
{
    namespace bytecode = wio::bytecode;
    namespace vm = wio::vm;
    std::atomic_uint calls{0};
    vm::Machine* reentrantMachine = nullptr;
    bool reentrySucceeded = false;

    bool expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << message << '\n';
        return condition;
    }

    WioNativeAbiStatus initialize(const WioNativeAbiValue*, std::uint64_t count, WioNativeAbiValue* result,
                                  WioNativeAbiFailure*) noexcept
    {
        if (count != 0 || !result)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        ++calls;
        if (reentrantMachine)
        {
            const auto nested = reentrantMachine->invoke(2);
            reentrySucceeded = nested.succeeded() && nested.value().asSignedInteger() == 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
        result->kind = WIO_NATIVE_ABI_I64;
        result->payload.signedInteger = 42;
        return WIO_NATIVE_ABI_OK;
    }

    bytecode::Module makeModule()
    {
        bytecode::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.strings = {"release:global-initializer"};
        module.constants = {bytecode::Constant{}};
        module.types = {bytecode::Type{.kind = 1}, bytecode::Type{.kind = 5},
                        bytecode::Type{.kind = 29, .arguments = {1}, .flags = 1}};
        module.globals = {bytecode::Global{.id = 0, .type = 1, .initializer = 1, .flags = 1}};

        bytecode::Function native;
        native.id = 0;
        native.flags = 0x0042;
        native.returnType = 1;
        native.hasNativeBinding = true;
        native.nativeBinding.stableKey = 0;
        native.nativeBinding.result = {.type = 1, .passing = 4, .marshalling = 1};
        module.functions.push_back(native);

        bytecode::Instruction call;
        call.opcode = bytecode::Opcode::NativeInvoke;
        call.constant = 0;
        call.result = 0;
        call.resultType = 1;
        call.callee = 0;
        bytecode::Instruction returned;
        returned.opcode = bytecode::Opcode::Return;
        returned.constant = 0;
        returned.operands = {0};
        bytecode::Function initializer;
        initializer.id = 1;
        initializer.returnType = 1;
        initializer.blocks = {bytecode::Block{.id = 0, .instructions = {call, returned}}};
        module.functions.push_back(initializer);

        bytecode::Instruction place;
        place.opcode = bytecode::Opcode::GlobalPlace;
        place.constant = 0;
        place.result = 0;
        place.resultType = 2;
        place.global = 0;
        bytecode::Instruction load;
        load.opcode = bytecode::Opcode::Load;
        load.constant = 0;
        load.result = 1;
        load.resultType = 1;
        load.operands = {0};
        returned.operands = {1};
        bytecode::Function read;
        read.id = 2;
        read.returnType = 1;
        read.blocks = {bytecode::Block{.id = 0, .instructions = {place, load, returned}}};
        module.functions.push_back(read);
        return module;
    }
} // namespace

int main()
{
    bool ok = true;
    const auto decoded = bytecode::decode(bytecode::encode(makeModule()));
    ok &= expect(decoded.succeeded(), "Global initializer metadata must survive the codec");
    if (!decoded.succeeded())
        return 1;
    const auto& module = decoded.module;
    const auto verified = bytecode::Verifier{}.verify(module);
    for (const auto& diagnostic : verified.diagnostics())
        std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    if (!verified.succeeded())
        return 1;
    const WioNativeAbiFunctionDescriptor descriptor{
        WIO_NATIVE_ABI_VERSION, 0, 1, "release:global-initializer", "Initialize", "initialize", &initialize};
    const WioNativeAbiRegistry registry{WIO_NATIVE_ABI_VERSION, 1, &descriptor};
    {
        vm::Machine machine{module};
        ok &= expect(calls == 0, "Construction must not run globals before native registration");
        ok &= expect(machine.registerNativeRegistry(registry), "Native initializer registry must register");
        reentrantMachine = &machine;
        const auto first = machine.invoke(2);
        reentrantMachine = nullptr;
        ok &= expect(first.succeeded() && first.value().asSignedInteger() == 42 && reentrySucceeded,
                     "Initializers must allow synchronous same-thread reentry without restarting");
        for (int index = 0; index < 8; ++index)
            ok &= expect(machine.invoke(2).succeeded(), "Repeated entry must preserve initialized globals");
        ok &= expect(calls == 1, "Globals must initialize exactly once per machine");
    }
    {
        vm::Machine machine{module};
        ok &= expect(machine.registerNativeRegistry(registry), "A second machine must accept the same registry");
        std::latch start{8};
        std::atomic_uint successes{0};
        std::vector<std::thread> threads;
        for (int index = 0; index < 8; ++index)
            threads.emplace_back(
                [&]
                {
                    start.arrive_and_wait();
                    const auto result = machine.invoke(2);
                    if (result.succeeded() && result.value().asSignedInteger() == 42)
                        ++successes;
                });
        for (auto& thread : threads)
            thread.join();
        ok &= expect(successes == 8 && calls == 2,
                     "Concurrent first entries must wait for one initializer, independently per machine");
    }
    {
        vm::Machine machine{module};
        const auto first = machine.invoke(2);
        ok &= expect(!first.succeeded() && first.error().code == "WVM1220",
                     "Missing native global dependencies must fail initialization");
        ok &= expect(machine.registerNativeRegistry(registry), "Registration remains explicit after failure");
        const auto second = machine.invoke(2);
        ok &= expect(!second.succeeded() && second.error().code == first.error().code && calls == 2,
                     "A failed initializer must retain its first error and never retry on the same machine");
    }
    for (const unsigned mutation : {0u, 1u, 2u, 3u, 4u})
    {
        auto malformed = module;
        auto& initializer = malformed.functions[1];
        if (mutation == 0)
            malformed.globals[0].initializer = 999;
        else if (mutation == 1)
            initializer.returnType = 0;
        else if (mutation == 2)
            initializer.parameters = {bytecode::Parameter{.value = 0, .type = 1}};
        else if (mutation == 3)
            initializer.flags |= 1;
        else
            initializer.captureParameterCount = 1;
        const auto verified = bytecode::Verifier{}.verify(malformed);
        bool rejected = false;
        for (const auto& diagnostic : verified.diagnostics())
            rejected |= diagnostic.code == "WBC1008";
        ok &= expect(rejected, "Malformed initializer references/signatures must fail bytecode verification");
    }
    return ok ? 0 : 1;
}
