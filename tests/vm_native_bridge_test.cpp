#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"

#include <wio_native_abi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <thread>
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
    constexpr std::uint32_t Opaque = 5;
    constexpr std::uint32_t Pod = 6;
    constexpr std::uint32_t ForeignObject = 7;
    constexpr std::uint32_t RuntimeValue = 8;
    constexpr std::uint32_t Callback = 9;
    constexpr std::uint64_t PodTypeId = 0x11'22'33'44u;
    constexpr std::uint64_t ObjectTypeId = 0x22'33'44'55u;
    constexpr std::uint64_t RuntimeTypeId = 0x33'44'55'66u;

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
        module.strings = {"native:add",
                          "native:bump",
                          "native:fail",
                          "native:text",
                          "native:string",
                          "native:bad-result",
                          "native:opaque",
                          "native:pod",
                          "native:make-object",
                          "native:object",
                          "native:make-runtime",
                          "native:runtime",
                          "fixture.Pod",
                          "fixture.Object",
                          "fixture.Runtime",
                          "native:callback-now",
                          "native:callback-call",
                          "native:callback-retained",
                          "native:callback-wrong-thread"};
        module.constants = {bytecode::Constant{}};
        module.types = {
            bytecode::Type{.kind = 1},
            bytecode::Type{.kind = 5},
            bytecode::Type{.kind = 17},
            bytecode::Type{.kind = 18},
            bytecode::Type{.kind = 29, .arguments = {I32}, .flags = 0x01u},
            bytecode::Type{.kind = 20},
            bytecode::Type{.kind = 28, .nominalRepresentation = 1, .hasNativeBinding = true},
            bytecode::Type{.kind = 28},
            bytecode::Type{.kind = 28},
            bytecode::Type{.kind = 33, .arguments = {I32, I32}},
        };
        module.contract.reflection = {
            bytecode::Reflection{.stableTypeId = PodTypeId, .logicalName = 12, .type = Pod},
            bytecode::Reflection{.stableTypeId = ObjectTypeId, .logicalName = 13, .type = ForeignObject},
            bytecode::Reflection{.stableTypeId = RuntimeTypeId, .logicalName = 14, .type = RuntimeValue},
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

        module.functions.push_back(
            nativeFunction(13, 6, {parameter(0, Opaque)}, Opaque, {abi(Opaque, 0, 5)}, abi(Opaque, 4, 5)));
        module.functions.push_back(nativeFunction(14, 7, {parameter(0, Pod)}, Pod, {abi(Pod, 0, 4)}, abi(Pod, 4, 4)));
        module.functions.push_back(nativeFunction(15, 8, {}, ForeignObject, {}, abi(ForeignObject, 4, 6)));
        module.functions.push_back(nativeFunction(16, 9, {parameter(0, ForeignObject)}, ForeignObject,
                                                  {abi(ForeignObject, 0, 6)}, abi(ForeignObject, 4, 6)));
        module.functions.push_back(nativeFunction(17, 10, {}, RuntimeValue, {}, abi(RuntimeValue, 4, 8)));
        module.functions.push_back(nativeFunction(18, 11, {parameter(0, RuntimeValue)}, RuntimeValue,
                                                  {abi(RuntimeValue, 0, 8)}, abi(RuntimeValue, 4, 8)));
        module.functions.push_back(
            caller(19, {parameter(0, Opaque)}, Opaque, {nativeCall(13, 1, Opaque, {0}), returnValue(1)}));
        module.functions.push_back(caller(20, {parameter(0, Pod)}, Pod, {nativeCall(14, 1, Pod, {0}), returnValue(1)}));
        module.functions.push_back(
            caller(21, {}, ForeignObject, {nativeCall(15, 0, ForeignObject, {}), returnValue(0)}));
        module.functions.push_back(caller(22, {parameter(0, ForeignObject)}, ForeignObject,
                                          {nativeCall(16, 1, ForeignObject, {0}), returnValue(1)}));
        module.functions.push_back(caller(23, {}, RuntimeValue, {nativeCall(17, 0, RuntimeValue, {}), returnValue(0)}));
        module.functions.push_back(caller(24, {parameter(0, RuntimeValue)}, RuntimeValue,
                                          {nativeCall(18, 1, RuntimeValue, {0}), returnValue(1)}));
        module.functions.push_back(nativeFunction(
            25, 15, {parameter(0, Callback)}, I32,
            {bytecode::Function::NativeAbiValue{
                .type = Callback, .passing = 0, .marshalling = 7, .callbackLifetime = 0, .callbackThread = 0}},
            abi(I32, 4, 1)));
        module.functions.push_back(nativeFunction(
            26, 16, {parameter(0, Callback)}, Void,
            {bytecode::Function::NativeAbiValue{
                .type = Callback, .passing = 0, .marshalling = 7, .callbackLifetime = 0, .callbackThread = 0}},
            abi(Void, 4, 0)));
        module.functions.push_back(nativeFunction(
            27, 17, {parameter(0, Callback)}, Void,
            {bytecode::Function::NativeAbiValue{
                .type = Callback, .passing = 0, .marshalling = 7, .callbackLifetime = 1, .callbackThread = 1}},
            abi(Void, 4, 0)));
        module.functions.push_back(nativeFunction(
            28, 18, {parameter(0, Callback)}, I32,
            {bytecode::Function::NativeAbiValue{
                .type = Callback, .passing = 0, .marshalling = 7, .callbackLifetime = 0, .callbackThread = 0}},
            abi(I32, 4, 1)));
        module.functions.push_back(caller(29, {parameter(0, I32)}, I32, {returnValue(0)}));
        module.functions.push_back(
            caller(30, {parameter(0, Callback)}, I32, {nativeCall(25, 1, I32, {0}), returnValue(1)}));
        module.functions.push_back(
            caller(31, {parameter(0, Callback)}, Void,
                   {nativeCall(26, bytecode::InvalidIndex, Void, {0}), instruction(bytecode::Opcode::Return)}));
        module.functions.push_back(
            caller(32, {parameter(0, Callback)}, Void,
                   {nativeCall(27, bytecode::InvalidIndex, Void, {0}), instruction(bytecode::Opcode::Return)}));
        module.functions.push_back(
            caller(33, {parameter(0, Callback)}, I32, {nativeCall(28, 1, I32, {0}), returnValue(1)}));
        return module;
    }

    struct TrackedHandle
    {
        std::atomic_uint32_t references{1};
        std::uint64_t typeId = 0;
    };

    std::atomic_uint32_t destroyedHandles{0};

    void retainHandle(void* const state) noexcept
    {
        static_cast<TrackedHandle*>(state)->references.fetch_add(1, std::memory_order_relaxed);
    }

    void releaseHandle(void* const state) noexcept
    {
        auto* handle = static_cast<TrackedHandle*>(state);
        if (handle->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            destroyedHandles.fetch_add(1, std::memory_order_relaxed);
            delete handle;
        }
    }

    std::uint64_t handleTypeId(const void* const state) noexcept
    {
        return static_cast<const TrackedHandle*>(state)->typeId;
    }

    constexpr WioNativeAbiHandleOps HandleOps{WIO_NATIVE_ABI_VERSION, 0, &retainHandle, &releaseHandle, &handleTypeId};

    WioNativeAbiStatus makeOwnedHandle(const std::uint64_t typeId, const WioNativeAbiValueKind kind,
                                       WioNativeAbiValue* const result) noexcept
    {
        if (!result)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        result->kind = kind;
        result->flags = WIO_NATIVE_ABI_VALUE_OWNED;
        result->typeId = typeId;
        result->payload.handle = {new TrackedHandle{.typeId = typeId}, &HandleOps, 1};
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus makeObject(const WioNativeAbiValue*, const std::uint64_t count, WioNativeAbiValue* result,
                                  WioNativeAbiFailure*) noexcept
    {
        return count == 0 ? makeOwnedHandle(ObjectTypeId, WIO_NATIVE_ABI_OBJECT, result)
                          : WIO_NATIVE_ABI_INVALID_ARGUMENT;
    }

    WioNativeAbiStatus makeRuntimeValue(const WioNativeAbiValue*, const std::uint64_t count, WioNativeAbiValue* result,
                                        WioNativeAbiFailure*) noexcept
    {
        return count == 0 ? makeOwnedHandle(RuntimeTypeId, WIO_NATIVE_ABI_RUNTIME_VALUE, result)
                          : WIO_NATIVE_ABI_INVALID_ARGUMENT;
    }

    WioNativeAbiCallback storedCallback{};
    std::atomic_int retainedEntryStatus{WIO_NATIVE_ABI_INVALID_ARGUMENT};
    std::atomic_int64_t retainedEntryResult{0};

    void releaseStoredCallback()
    {
        if (storedCallback.userdata && storedCallback.ops && storedCallback.ops->release)
            storedCallback.ops->release(storedCallback.userdata);
        storedCallback = {};
    }

    WioNativeAbiStatus invokeCallback(const WioNativeAbiValue* arguments, const std::uint64_t count,
                                      WioNativeAbiValue* result, WioNativeAbiFailure* failure) noexcept
    {
        if (!arguments || count != 1 || !result || arguments[0].kind != WIO_NATIVE_ABI_CALLBACK ||
            !arguments[0].payload.callback.ops || !arguments[0].payload.callback.ops->invoke)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        WioNativeAbiValue callbackArgument;
        callbackArgument.kind = WIO_NATIVE_ABI_I64;
        callbackArgument.payload.signedInteger = 41;
        return arguments[0].payload.callback.ops->invoke(arguments[0].payload.callback.userdata, &callbackArgument, 1,
                                                         result, failure);
    }

    WioNativeAbiStatus storeCallback(const WioNativeAbiValue* arguments, const std::uint64_t count,
                                     WioNativeAbiValue* result, WioNativeAbiFailure*) noexcept
    {
        if (!arguments || count != 1 || !result || arguments[0].kind != WIO_NATIVE_ABI_CALLBACK ||
            !arguments[0].payload.callback.ops || !arguments[0].payload.callback.ops->retain)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        releaseStoredCallback();
        storedCallback = arguments[0].payload.callback;
        storedCallback.ops->retain(storedCallback.userdata);
        if ((storedCallback.ops->flags & WIO_NATIVE_ABI_CALLBACK_ANY_THREAD) != 0)
        {
            std::thread worker{[]
                               {
                                   WioNativeAbiValue callbackArgument;
                                   callbackArgument.kind = WIO_NATIVE_ABI_I64;
                                   callbackArgument.payload.signedInteger = 9;
                                   WioNativeAbiValue callbackResult;
                                   const WioNativeAbiStatus status = storedCallback.ops->invoke(
                                       storedCallback.userdata, &callbackArgument, 1, &callbackResult, nullptr);
                                   retainedEntryStatus.store(status, std::memory_order_relaxed);
                                   if (status == WIO_NATIVE_ABI_OK && callbackResult.kind == WIO_NATIVE_ABI_I64)
                                       retainedEntryResult.store(callbackResult.payload.signedInteger,
                                                                 std::memory_order_relaxed);
                                   WioNativeAbiReleaseValue(&callbackResult);
                               }};
            worker.join();
        }
        *result = {};
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus invokeCallbackFromWrongThread(const WioNativeAbiValue* arguments, const std::uint64_t count,
                                                     WioNativeAbiValue* result, WioNativeAbiFailure*) noexcept
    {
        if (!arguments || count != 1 || !result || arguments[0].kind != WIO_NATIVE_ABI_CALLBACK ||
            !arguments[0].payload.callback.ops || !arguments[0].payload.callback.ops->invoke)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        WioNativeAbiStatus callbackStatus = WIO_NATIVE_ABI_OK;
        std::thread worker{[&]
                           {
                               WioNativeAbiValue callbackResult;
                               callbackStatus = arguments[0].payload.callback.ops->invoke(
                                   arguments[0].payload.callback.userdata, nullptr, 0, &callbackResult, nullptr);
                               WioNativeAbiReleaseValue(&callbackResult);
                           }};
        worker.join();
        result->kind = WIO_NATIVE_ABI_I64;
        result->payload.signedInteger = callbackStatus;
        return WIO_NATIVE_ABI_OK;
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

    bytecode::Module missingTypeIdentity = makeModule();
    missingTypeIdentity.contract.reflection.clear();
    const bytecode::DecodeResult missingTypeIdentityDecoded = bytecode::decode(bytecode::encode(missingTypeIdentity));
    vm::Machine missingTypeIdentityMachine{missingTypeIdentityDecoded.module};
    const vm::ExecutionResult missingTypeIdentityResult = missingTypeIdentityMachine.invoke(21);
    ok &= expect(missingTypeIdentityDecoded.succeeded() && !missingTypeIdentityResult.succeeded() &&
                     missingTypeIdentityResult.error().code == "WVM1001" &&
                     missingTypeIdentityResult.error().message.find("WBC1074") != std::string::npos,
                 "The bytecode verifier must require stable type identity for POD and owned handle marshalling");

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
        WioNativeAbiFunctionDescriptor{WIO_NATIVE_ABI_VERSION, 0, 7, "native:opaque", "EchoOpaque", "opaque", &echo},
        WioNativeAbiFunctionDescriptor{WIO_NATIVE_ABI_VERSION, 0, 8, "native:pod", "EchoPod", "pod", &echo},
        WioNativeAbiFunctionDescriptor{WIO_NATIVE_ABI_VERSION, 0, 9, "native:make-object", "MakeObject", "makeObject",
                                       &makeObject},
        WioNativeAbiFunctionDescriptor{WIO_NATIVE_ABI_VERSION, 0, 10, "native:object", "EchoObject", "object", &echo},
        WioNativeAbiFunctionDescriptor{WIO_NATIVE_ABI_VERSION, 0, 11, "native:make-runtime", "MakeRuntime",
                                       "makeRuntime", &makeRuntimeValue},
        WioNativeAbiFunctionDescriptor{WIO_NATIVE_ABI_VERSION, 0, 12, "native:runtime", "EchoRuntime", "runtime",
                                       &echo},
        WioNativeAbiFunctionDescriptor{WIO_NATIVE_ABI_VERSION, 0, 13, "native:callback-now", "InvokeCallback",
                                       "invokeCallback", &invokeCallback},
        WioNativeAbiFunctionDescriptor{WIO_NATIVE_ABI_VERSION, 0, 14, "native:callback-call", "StoreCallback",
                                       "storeCallback", &storeCallback},
        WioNativeAbiFunctionDescriptor{WIO_NATIVE_ABI_VERSION, 0, 15, "native:callback-retained", "StoreCallback",
                                       "storeCallback", &storeCallback},
        WioNativeAbiFunctionDescriptor{WIO_NATIVE_ABI_VERSION, 0, 16, "native:callback-wrong-thread",
                                       "InvokeCallbackFromWrongThread", "invokeCallbackFromWrongThread",
                                       &invokeCallbackFromWrongThread},
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

    std::uint64_t opaqueState = 0x1234u;
    const vm::Value opaque = vm::Value::opaque(Opaque, 0, &opaqueState);
    const vm::ExecutionResult echoedOpaque = machine.invoke(19, std::span{&opaque, 1});
    ok &= expect(echoedOpaque.succeeded() && echoedOpaque.value().kind() == vm::Value::Kind::Opaque &&
                     echoedOpaque.value().opaquePointer() == &opaqueState,
                 "Opaque pointers must cross the VM bridge without reinterpretation or foreign deletion");

    const std::array podBytes{std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x40}};
    const vm::Value pod = vm::Value::nativePod(Pod, PodTypeId, podBytes);
    const vm::ExecutionResult echoedPod = machine.invoke(20, std::span{&pod, 1});
    ok &= expect(echoedPod.succeeded() && echoedPod.value().kind() == vm::Value::Kind::NativePod &&
                     echoedPod.value().foreignTypeId() == PodTypeId &&
                     std::equal(echoedPod.value().nativePodBytes().begin(), echoedPod.value().nativePodBytes().end(),
                                podBytes.begin(), podBytes.end()),
                 "Native POD values must be copied into VM-owned bytes and retain their stable type identity");

    const std::uint32_t destroyedBeforeObject = destroyedHandles.load(std::memory_order_relaxed);
    {
        const vm::ExecutionResult object = machine.invoke(21);
        ok &= expect(object.succeeded() && object.value().kind() == vm::Value::Kind::ForeignObject &&
                         object.value().foreignTypeId() == ObjectTypeId,
                     "Owned native object results must become producer-managed VM handles");
        const vm::Value objectArgument = object.value();
        const vm::ExecutionResult echoedObject = machine.invoke(22, std::span{&objectArgument, 1});
        ok &= expect(echoedObject.succeeded() && echoedObject.value() == object.value(),
                     "Foreign object identity must survive a borrowed native round-trip");
    }
    ok &= expect(destroyedHandles.load(std::memory_order_relaxed) == destroyedBeforeObject + 1,
                 "The producer release operation must destroy an owned object exactly once");

    const std::uint32_t destroyedBeforeRuntime = destroyedHandles.load(std::memory_order_relaxed);
    {
        const vm::ExecutionResult runtime = machine.invoke(23);
        ok &= expect(runtime.succeeded() && runtime.value().kind() == vm::Value::Kind::RuntimeValue &&
                         runtime.value().foreignTypeId() == RuntimeTypeId,
                     "Owned native runtime values must preserve their producer operations");
        const vm::Value runtimeArgument = runtime.value();
        const vm::ExecutionResult echoedRuntime = machine.invoke(24, std::span{&runtimeArgument, 1});
        ok &= expect(echoedRuntime.succeeded() && echoedRuntime.value() == runtime.value(),
                     "Runtime handles must round-trip through the VM without changing ownership domains");
    }
    ok &= expect(destroyedHandles.load(std::memory_order_relaxed) == destroyedBeforeRuntime + 1,
                 "The producer release operation must destroy an owned runtime value exactly once");

    const vm::Value callback = vm::Value::callable(29, {});
    const vm::ExecutionResult callbackResult = machine.invoke(30, std::span{&callback, 1});
    ok &= expect(callbackResult.succeeded() && callbackResult.value().kind() == vm::Value::Kind::SignedInteger &&
                     callbackResult.value().asSignedInteger() == 41,
                 "A native thunk must be able to synchronously call a scalar Wio callable");

    const vm::ExecutionResult wrongThread = machine.invoke(33, std::span{&callback, 1});
    ok &= expect(wrongThread.succeeded() && wrongThread.value().asSignedInteger() == WIO_NATIVE_ABI_WRONG_THREAD,
                 "Caller-thread callbacks must reject entry from a foreign thread before VM execution");

    const vm::ExecutionResult storedCallScoped = machine.invoke(31, std::span{&callback, 1});
    WioNativeAbiValue callbackArgument;
    callbackArgument.kind = WIO_NATIVE_ABI_I64;
    callbackArgument.payload.signedInteger = 7;
    WioNativeAbiValue lateCallResult;
    const WioNativeAbiStatus lateCallStatus =
        storedCallback.ops->invoke(storedCallback.userdata, &callbackArgument, 1, &lateCallResult, nullptr);
    WioNativeAbiReleaseValue(&lateCallResult);
    ok &= expect(storedCallScoped.succeeded() && lateCallStatus == WIO_NATIVE_ABI_STALE_HANDLE,
                 "Call-scoped callbacks must expire as soon as the native invocation returns");
    releaseStoredCallback();

    const vm::ExecutionResult storedRetained = machine.invoke(32, std::span{&callback, 1});
    ok &=
        expect(storedRetained.succeeded() && retainedEntryStatus.load(std::memory_order_relaxed) == WIO_NATIVE_ABI_OK &&
                   retainedEntryResult.load(std::memory_order_relaxed) == 9,
               "Any-thread callbacks must re-enter while the native caller is still active without deadlocking");
    WioNativeAbiStatus retainedStatus = WIO_NATIVE_ABI_INVALID_ARGUMENT;
    std::int64_t retainedResult = 0;
    std::thread retainedWorker{[&]
                               {
                                   WioNativeAbiValue result;
                                   retainedStatus = storedCallback.ops->invoke(storedCallback.userdata,
                                                                               &callbackArgument, 1, &result, nullptr);
                                   if (retainedStatus == WIO_NATIVE_ABI_OK && result.kind == WIO_NATIVE_ABI_I64)
                                       retainedResult = result.payload.signedInteger;
                                   WioNativeAbiReleaseValue(&result);
                               }};
    retainedWorker.join();
    ok &= expect(retainedStatus == WIO_NATIVE_ABI_OK && retainedResult == 7,
                 "Retained any-thread callbacks must stay callable after return and enter the serialized VM safely");
    releaseStoredCallback();

    {
        vm::Machine transientMachine{decoded.module};
        ok &= expect(transientMachine.registerNativeRegistry(registry),
                     "A transient callback owner must accept the shared native registry");
        const vm::ExecutionResult retainedByNative = transientMachine.invoke(32, std::span{&callback, 1});
        ok &= expect(retainedByNative.succeeded(), "A retained callback must be created before owner teardown");
    }
    WioNativeAbiValue afterOwnerResult;
    const WioNativeAbiStatus afterOwnerStatus =
        storedCallback.ops->invoke(storedCallback.userdata, &callbackArgument, 1, &afterOwnerResult, nullptr);
    WioNativeAbiReleaseValue(&afterOwnerResult);
    ok &= expect(afterOwnerStatus == WIO_NATIVE_ABI_STALE_HANDLE,
                 "Retained callbacks must fail safely after their owning VM machine is destroyed");
    releaseStoredCallback();

    const WioNativeAbiFunctionDescriptor conflict{WIO_NATIVE_ABI_VERSION, 0, 1, "native:add", "Other", "other",
                                                  &conflictingAdd};
    const WioNativeAbiRegistry conflictingRegistry{WIO_NATIVE_ABI_VERSION, 1, &conflict};
    ok &= expect(!machine.registerNativeRegistry(conflictingRegistry, &registryError),
                 "A stable native key cannot be rebound to a different thunk inside one machine");
    return ok ? 0 : 1;
}
