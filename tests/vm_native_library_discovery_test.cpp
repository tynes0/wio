#include "wio/vm/machine.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace
{
    namespace bytecode = wio::bytecode;
    namespace vm = wio::vm;

    bool expect(const bool condition, const std::string_view message)
    {
        if (condition)
            return true;
        std::cerr << message << '\n';
        return false;
    }

    bytecode::Module makeModule()
    {
        bytecode::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.strings = {"test:vm-discovery-answer", "answer", "Entry", "test:vm-discovery-object", "Object"};
        module.constants = {bytecode::Constant{}};
        module.types = {
            bytecode::Type{.kind = 1},
            bytecode::Type{.kind = 5},
            bytecode::Type{.kind = 28},
        };
        module.contract.reflection = {
            bytecode::Reflection{.stableTypeId = 0x20'05'00u, .logicalName = 4, .type = 2},
        };

        bytecode::Function native;
        native.id = 0;
        native.name = 1;
        native.returnType = 1;
        native.flags = 0x0042u;
        native.hasNativeBinding = true;
        native.nativeBinding.stableKey = 0;
        native.nativeBinding.result = {.type = 1, .passing = 4, .marshalling = 1};
        module.functions.push_back(std::move(native));

        bytecode::Instruction invoke;
        invoke.opcode = bytecode::Opcode::NativeInvoke;
        invoke.result = 0;
        invoke.resultType = 1;
        invoke.callee = 0;
        invoke.constant = 0;
        bytecode::Instruction returnValue;
        returnValue.opcode = bytecode::Opcode::Return;
        returnValue.constant = 0;
        returnValue.operands = {0};
        bytecode::Function entry;
        entry.id = 1;
        entry.name = 2;
        entry.returnType = 1;
        entry.blocks.push_back(bytecode::Block{.id = 0, .instructions = {invoke, returnValue}});
        module.functions.push_back(std::move(entry));

        bytecode::Function factory;
        factory.id = 2;
        factory.returnType = 2;
        factory.flags = 0x0042u;
        factory.hasNativeBinding = true;
        factory.nativeBinding.stableKey = 3;
        factory.nativeBinding.result = {.type = 2, .passing = 4, .marshalling = 6};
        module.functions.push_back(std::move(factory));
        invoke.callee = 2;
        invoke.resultType = 2;
        bytecode::Function objectCaller;
        objectCaller.id = 3;
        objectCaller.returnType = 2;
        objectCaller.blocks.push_back(bytecode::Block{.id = 0, .instructions = {invoke, returnValue}});
        module.functions.push_back(std::move(objectCaller));
        return module;
    }
} // namespace

int main()
{
    bool ok = true;
    const bytecode::Module module = makeModule();
    vm::Machine machine{module};

    const vm::ExecutionResult missing = machine.invoke(1);
    ok &= expect(!missing.succeeded() && missing.error().code == "WVM1220",
                 "Native invocation must fail before dynamic registry discovery");

    std::string error;
    ok &= expect(!machine.loadNativeLibrary(WIO_TEST_MISSING_REGISTRY_LIBRARY, &error) &&
                     error.find("does not export WioGetNativeAbiRegistry") != std::string::npos,
                 "A non-Wio library must be rejected without altering the machine registry");
    ok &= expect(!machine.loadNativeLibrary(WIO_TEST_INVALID_ABI_LIBRARY, &error) &&
                     error.find("ABI version") != std::string::npos,
                 "Incompatible registry ABI versions must be rejected");
    ok &= expect(!machine.loadNativeLibrary(WIO_TEST_INVALID_SIDECAR_LIBRARY, &error) &&
                     error.find("without the native task API") != std::string::npos,
                 "Completion sidecars without task APIs must be rejected");
    ok &= expect(machine.loadNativeLibrary(WIO_TEST_NATIVE_LIBRARY, &error), error);
    ok &= expect(machine.loadNativeLibrary(WIO_TEST_NATIVE_LIBRARY, &error),
                 "Loading one native module twice must be idempotent");
    const std::filesystem::path libraryPath = WIO_TEST_NATIVE_LIBRARY;
    ok &= expect(machine.loadNativeLibrary(libraryPath.parent_path() / "." / libraryPath.filename(), &error),
                 "Equivalent native module paths must share one registry lease");
#if defined(_WIN32)
    std::string lowerCasePath = WIO_TEST_NATIVE_LIBRARY;
    for (char& value : lowerCasePath)
        if (value >= 'A' && value <= 'Z')
            value = static_cast<char>(value + ('a' - 'A'));
    ok &= expect(machine.loadNativeLibrary(lowerCasePath, &error),
                 "Windows path case variants must not duplicate machine registrations");
#endif
    const vm::ExecutionResult answer = machine.invoke(1);
    ok &= expect(answer.succeeded() && answer.value().kind() == vm::Value::Kind::SignedInteger &&
                     answer.value().asSignedInteger() == 42,
                 "Discovered native registry must execute through the VM");

    std::filesystem::path missingPath = WIO_TEST_NATIVE_LIBRARY;
    missingPath += ".missing";
    ok &= expect(!machine.loadNativeLibrary(missingPath, &error) && error.find("does not exist") != std::string::npos,
                 "Missing native modules must produce a deterministic discovery error");

    vm::Value escaped;
    {
        vm::Machine owner{module};
        ok &= expect(owner.loadNativeLibrary(WIO_TEST_NATIVE_LIBRARY, &error), error);
        const auto object = owner.invoke(3);
        ok &= expect(object.succeeded(), "Native object factory must execute from discovered registry");
        if (object.succeeded())
            escaped = object.value();
    }
    const auto stillRunning = machine.invoke(1);
    ok &= expect(stillRunning.succeeded() && stillRunning.value().asSignedInteger() == 42,
                 "Destroying one machine must not shut down a library shared by another machine");
    // Drop the other machine's lease as well. The escaped foreign value must
    // retain the DLL containing its producer operations independently.
    machine = vm::Machine{module};
    const auto handle = escaped.foreignHandle();
    ok &= expect(handle.state && handle.ops && handle.ops->typeId && handle.ops->typeId(handle.state) == 0x20'05'00u,
                 "Escaped native values must keep producer code loaded after machine teardown");
    ok &= expect(machine.loadNativeLibrary(WIO_TEST_NATIVE_LIBRARY, &error), error);
    const auto afterShutdown = machine.invoke(1);
    ok &= expect(afterShutdown.succeeded() && afterShutdown.value().asSignedInteger() == 43,
                 "The last machine must shut down the producer once even while an escaped value pins its code");
    machine = vm::Machine{module};
    escaped = {};
    return ok ? 0 : 1;
}
