#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"

#include <wio_native_abi.h>

#include <array>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    namespace bytecode = wio::bytecode;
    namespace vm = wio::vm;

    constexpr std::uint32_t Void = 0;
    constexpr std::uint32_t I32 = 1;
    constexpr std::uint32_t String = 2;
    constexpr std::uint32_t Text = 3;
    constexpr std::uint32_t RefI32 = 4;

    bool expect(const bool condition, const std::string_view message)
    {
        if (condition)
            return true;
        std::cerr << message << '\n';
        return false;
    }

    bytecode::Instruction instruction(const bytecode::Opcode opcode)
    {
        bytecode::Instruction result;
        result.opcode = opcode;
        result.constant = 0;
        return result;
    }

    bytecode::Parameter parameter(const std::uint32_t value, const std::uint32_t type)
    {
        return {.value = value, .type = type};
    }

    bytecode::Function::NativeAbiValue abi(const std::uint32_t type, const std::uint8_t passing,
                                           const std::uint8_t marshalling)
    {
        return {.type = type, .passing = passing, .marshalling = marshalling};
    }

    bytecode::Function nativeFunction(const std::uint32_t id, const bytecode::StringId key,
                                      std::vector<bytecode::Parameter> parameters, const std::uint32_t result,
                                      std::vector<bytecode::Function::NativeAbiValue> abiParameters,
                                      const bytecode::Function::NativeAbiValue abiResult)
    {
        bytecode::Function function;
        function.id = id;
        function.parameters = std::move(parameters);
        function.returnType = result;
        function.flags = 0x0042u;
        function.hasNativeBinding = true;
        function.nativeBinding.stableKey = key;
        function.nativeBinding.parameters = std::move(abiParameters);
        function.nativeBinding.result = abiResult;
        return function;
    }

    bytecode::Instruction nativeCall(const std::uint32_t callee, const std::uint32_t result,
                                     const std::uint32_t resultType, std::vector<std::uint32_t> operands)
    {
        bytecode::Instruction call = instruction(bytecode::Opcode::NativeInvoke);
        call.callee = callee;
        call.result = result;
        call.resultType = resultType;
        call.operands = std::move(operands);
        return call;
    }

    bytecode::Instruction returnValue(const std::uint32_t value)
    {
        bytecode::Instruction result = instruction(bytecode::Opcode::Return);
        result.operands = {value};
        return result;
    }

    bytecode::Function caller(const std::uint32_t id, std::vector<bytecode::Parameter> parameters,
                              const std::uint32_t result, std::vector<bytecode::Instruction> instructions)
    {
        bytecode::Function function;
        function.id = id;
        function.parameters = std::move(parameters);
        function.returnType = result;
        function.blocks.push_back(bytecode::Block{.id = 0, .instructions = std::move(instructions)});
        return function;
    }

    bytecode::Module makeModule()
    {
        bytecode::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.strings = {"native:add",  "native:bump",   "native:fail",
                          "native:text", "native:string", "native:bad-result"};
        module.constants = {bytecode::Constant{}};
        module.types = {
            bytecode::Type{.kind = 1},
            bytecode::Type{.kind = 5},
            bytecode::Type{.kind = 17},
            bytecode::Type{.kind = 18},
            bytecode::Type{.kind = 29, .arguments = {I32}, .flags = 0x01u},
        };
        module.globals = {bytecode::Global{.id = 0, .type = I32, .flags = 0x01u}};

        module.functions.push_back(nativeFunction(0, 0, {parameter(0, I32), parameter(1, I32)}, I32,
                                                  {abi(I32, 0, 1), abi(I32, 0, 1)}, abi(I32, 4, 1)));
        module.functions.push_back(
            nativeFunction(1, 1, {parameter(0, RefI32)}, Void, {abi(RefI32, 2, 1)}, abi(Void, 4, 0)));
        module.functions.push_back(
            nativeFunction(2, 2, {parameter(0, RefI32)}, Void, {abi(RefI32, 2, 1)}, abi(Void, 4, 0)));
        module.functions.push_back(
            nativeFunction(3, 3, {parameter(0, Text)}, Text, {abi(Text, 0, 3)}, abi(Text, 4, 3)));
        module.functions.push_back(
            nativeFunction(4, 4, {parameter(0, String)}, String, {abi(String, 0, 2)}, abi(String, 4, 2)));
        module.functions.push_back(nativeFunction(5, 5, {}, I32, {}, abi(I32, 4, 1)));

        module.functions.push_back(
            caller(6, {parameter(0, I32), parameter(1, I32)}, I32, {nativeCall(0, 2, I32, {0, 1}), returnValue(2)}));

        bytecode::Instruction local = instruction(bytecode::Opcode::LocalPlace);
        local.result = 1;
        local.resultType = RefI32;
        bytecode::Instruction initialize = instruction(bytecode::Opcode::PlaceInit);
        initialize.operands = {1, 0};
        bytecode::Instruction bump = nativeCall(1, bytecode::InvalidIndex, Void, {1});
        bytecode::Instruction load = instruction(bytecode::Opcode::Load);
        load.result = 2;
        load.resultType = I32;
        load.operands = {1};
        module.functions.push_back(
            caller(7, {parameter(0, I32)}, I32,
                   {std::move(local), std::move(initialize), std::move(bump), std::move(load), returnValue(2)}));

        module.functions.push_back(
            caller(8, {parameter(0, Text)}, Text, {nativeCall(3, 1, Text, {0}), returnValue(1)}));
        module.functions.push_back(
            caller(9, {parameter(0, String)}, String, {nativeCall(4, 1, String, {0}), returnValue(1)}));
        module.functions.push_back(caller(10, {}, I32, {nativeCall(5, 0, I32, {}), returnValue(0)}));

        bytecode::Instruction global = instruction(bytecode::Opcode::GlobalPlace);
        global.result = 0;
        global.resultType = RefI32;
        global.global = 0;
        module.functions.push_back(caller(11, {}, Void,
                                          {std::move(global), nativeCall(2, bytecode::InvalidIndex, Void, {0}),
                                           instruction(bytecode::Opcode::Return)}));
        bytecode::Instruction readGlobal = instruction(bytecode::Opcode::GlobalPlace);
        readGlobal.result = 0;
        readGlobal.resultType = RefI32;
        readGlobal.global = 0;
        bytecode::Instruction loadGlobal = instruction(bytecode::Opcode::Load);
        loadGlobal.result = 1;
        loadGlobal.resultType = I32;
        loadGlobal.operands = {0};
        module.functions.push_back(caller(12, {}, I32, {std::move(readGlobal), std::move(loadGlobal), returnValue(1)}));
        return module;
    }

    WioNativeAbiStatus add(const WioNativeAbiValue* arguments, const std::uint64_t count, WioNativeAbiValue* result,
                           WioNativeAbiFailure*) noexcept
    {
        if (!arguments || count != 2 || !result || arguments[0].kind != WIO_NATIVE_ABI_I64 ||
            arguments[1].kind != WIO_NATIVE_ABI_I64)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        result->kind = WIO_NATIVE_ABI_I64;
        result->payload.signedInteger = arguments[0].payload.signedInteger + arguments[1].payload.signedInteger;
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus bump(const WioNativeAbiValue* arguments, const std::uint64_t count, WioNativeAbiValue* result,
                            WioNativeAbiFailure*) noexcept
    {
        if (!arguments || count != 1 || !result || arguments[0].kind != WIO_NATIVE_ABI_REFERENCE ||
            !(arguments[0].flags & WIO_NATIVE_ABI_VALUE_MUTABLE) || !arguments[0].payload.mutableSlice.data)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        auto* token = static_cast<WioNativeAbiValue*>(arguments[0].payload.mutableSlice.data);
        if (token->kind != WIO_NATIVE_ABI_I64)
            return WIO_NATIVE_ABI_TYPE_MISMATCH;
        ++token->payload.signedInteger;
        *result = {};
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus failAfterBump(const WioNativeAbiValue* arguments, const std::uint64_t count,
                                     WioNativeAbiValue* result, WioNativeAbiFailure* failure) noexcept
    {
        const WioNativeAbiStatus bumped = bump(arguments, count, result, failure);
        if (bumped != WIO_NATIVE_ABI_OK)
            return bumped;
        static constexpr char Category[] = "fixture.native";
        static constexpr char Message[] = "expected failure";
        if (failure)
            *failure = {WIO_NATIVE_ABI_EXCEPTION, 73, {Category, sizeof(Category) - 1}, {Message, sizeof(Message) - 1}};
        return WIO_NATIVE_ABI_EXCEPTION;
    }

    WioNativeAbiStatus echo(const WioNativeAbiValue* arguments, const std::uint64_t count, WioNativeAbiValue* result,
                            WioNativeAbiFailure*) noexcept
    {
        if (!arguments || count != 1 || !result)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        *result = arguments[0];
        result->flags &= ~WIO_NATIVE_ABI_VALUE_OWNED;
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus badResult(const WioNativeAbiValue*, const std::uint64_t, WioNativeAbiValue* result,
                                 WioNativeAbiFailure*) noexcept
    {
        static constexpr char Wrong[] = "wrong";
        result->kind = WIO_NATIVE_ABI_BYTES;
        result->payload.slice = {Wrong, sizeof(Wrong) - 1};
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus conflictingAdd(const WioNativeAbiValue*, const std::uint64_t, WioNativeAbiValue*,
                                      WioNativeAbiFailure*) noexcept
    {
        return WIO_NATIVE_ABI_PANIC;
    }
} // namespace

int main()
{
    bool ok = true;
    bytecode::Module malformed = makeModule();
    malformed.functions[6].blocks.front().instructions.front().callee = 1;
    const bytecode::DecodeResult malformedDecoded = bytecode::decode(bytecode::encode(malformed));
    vm::Machine malformedMachine{malformedDecoded.module};
    const vm::ExecutionResult malformedResult = malformedMachine.invoke(6);
    ok &= expect(malformedDecoded.succeeded() && !malformedResult.succeeded() &&
                     malformedResult.error().code == "WVM1001" &&
                     malformedResult.error().message.find("WBC1073") != std::string::npos,
                 "The bytecode verifier must reject NativeInvoke metadata that disagrees with its ABI declaration");

    const bytecode::DecodeResult decoded = bytecode::decode(bytecode::encode(makeModule()));
    ok &= expect(decoded.succeeded(), "Native bridge fixture must survive the real .wiob codec");
    if (!decoded.succeeded())
        return 1;

    vm::Machine machine{decoded.module};
    const vm::Value twenty = vm::Value::signedInteger(20);
    const vm::Value twentyTwo = vm::Value::signedInteger(22);
    const vm::Value addArguments[] = {twenty, twentyTwo};
    const vm::ExecutionResult missing = machine.invoke(6, addArguments);
    ok &= expect(!missing.succeeded() && missing.error().code == "WVM1220",
                 "A native call must reject an unregistered stable key");

    std::string registryError;
    const WioNativeAbiRegistry invalidRegistry{WIO_NATIVE_ABI_VERSION + 1, 0, nullptr};
    ok &= expect(!machine.registerNativeRegistry(invalidRegistry, &registryError) && !registryError.empty(),
                 "A registry with the wrong ABI version must be rejected before execution");

    const std::array descriptors{
        WioNativeAbiFunctionDescriptor{WIO_NATIVE_ABI_VERSION, 0, 1, "native:add", "Add", "add", &add},
        WioNativeAbiFunctionDescriptor{WIO_NATIVE_ABI_VERSION, 0, 2, "native:bump", "Bump", "bump", &bump},
        WioNativeAbiFunctionDescriptor{WIO_NATIVE_ABI_VERSION, 0, 3, "native:fail", "Fail", "fail", &failAfterBump},
        WioNativeAbiFunctionDescriptor{WIO_NATIVE_ABI_VERSION, 0, 4, "native:text", "EchoText", "text", &echo},
        WioNativeAbiFunctionDescriptor{WIO_NATIVE_ABI_VERSION, 0, 5, "native:string", "EchoString", "string", &echo},
        WioNativeAbiFunctionDescriptor{WIO_NATIVE_ABI_VERSION, 0, 6, "native:bad-result", "Bad", "bad", &badResult},
    };
    const WioNativeAbiRegistry registry{WIO_NATIVE_ABI_VERSION, static_cast<std::uint32_t>(descriptors.size()),
                                        descriptors.data()};
    ok &= expect(machine.registerNativeRegistry(registry, &registryError) && registryError.empty(),
                 "A valid wire-v2 registry must register by stable native key");
    ok &= expect(machine.registerNativeRegistry(registry), "Registering the same native registry must be idempotent");

    const vm::ExecutionResult sum = machine.invoke(6, addArguments);
    ok &= expect(sum.succeeded() && sum.value().kind() == vm::Value::Kind::SignedInteger &&
                     sum.value().asSignedInteger() == 42,
                 "VM NativeInvoke must execute registered scalar wire-v2 thunks");

    const vm::Value ten = vm::Value::signedInteger(10);
    const vm::ExecutionResult incremented = machine.invoke(7, std::span{&ten, 1});
    ok &= expect(incremented.succeeded() && incremented.value().asSignedInteger() == 11,
                 "Mutable native references must copy their committed wire value back into the VM place");

    const vm::Value unicode = vm::Value::text(U"Merhaba U0001f30d");
    const vm::ExecutionResult echoedText = machine.invoke(8, std::span{&unicode, 1});
    ok &= expect(echoedText.succeeded() && echoedText.value().kind() == vm::Value::Kind::Text &&
                     echoedText.value().asText() == unicode.asText(),
                 "Native UTF-32 text must round-trip without losing Unicode scalars");
    const vm::Value bytes = vm::Value::string(std::string{"a\0b", 3});
    const vm::ExecutionResult echoedString = machine.invoke(9, std::span{&bytes, 1});
    ok &= expect(echoedString.succeeded() && echoedString.value().asString() == bytes.asString(),
                 "Native UTF-8 strings must preserve embedded zero bytes");

    const vm::ExecutionResult failed = machine.invoke(11);
    const vm::ExecutionResult copiedBack = machine.invoke(12);
    ok &= expect(!failed.succeeded() && failed.error().code == "WVM1222" &&
                     failed.error().message.find("fixture.native: expected failure") != std::string::npos &&
                     failed.error().message.find("73") != std::string::npos,
                 "Native ABI failures must become source-aware VM failures with category and native code");
    ok &= expect(copiedBack.succeeded() && copiedBack.value().asSignedInteger() == 1,
                 "Mutable reference copy-back must occur even when the native thunk reports failure");

    const vm::ExecutionResult mismatch = machine.invoke(10);
    ok &= expect(!mismatch.succeeded() && mismatch.error().code == "WVM1224",
                 "A native thunk returning the wrong wire kind must fail at the VM boundary");

    const WioNativeAbiFunctionDescriptor conflict{WIO_NATIVE_ABI_VERSION, 0, 1, "native:add", "Other", "other",
                                                  &conflictingAdd};
    const WioNativeAbiRegistry conflictingRegistry{WIO_NATIVE_ABI_VERSION, 1, &conflict};
    ok &= expect(!machine.registerNativeRegistry(conflictingRegistry, &registryError),
                 "A stable native key cannot be rebound to a different thunk inside one machine");
    return ok ? 0 : 1;
}
