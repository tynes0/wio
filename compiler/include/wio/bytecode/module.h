#pragma once

#include "wio/bytecode/contract.h"
#include "wio/bytecode/format.h"

#include <compare>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace wio::bytecode
{
    using StringId = std::uint32_t;
    using ConstantId = std::uint32_t;

    struct SourceLocation
    {
        StringId file = InvalidIndex;
        std::uint64_t line = 0;
        std::uint64_t column = 0;

        auto operator<=>(const SourceLocation&) const = default;
    };

    struct SourceSpan
    {
        SourceLocation begin;
        SourceLocation end;

        auto operator<=>(const SourceSpan&) const = default;
    };

    struct Constant
    {
        ConstantKind kind = ConstantKind::Empty;
        std::uint64_t bits = 0;
        StringId string = InvalidIndex;

        auto operator<=>(const Constant&) const = default;
    };

    struct Parameter
    {
        std::uint32_t value = InvalidIndex;
        StringId name = InvalidIndex;
        std::uint32_t type = InvalidIndex;
        std::uint8_t ownership = 0;
        std::uint8_t borrowLifetime = 0;
        SourceSpan source;

        auto operator<=>(const Parameter&) const = default;
    };

    struct BranchTarget
    {
        std::uint32_t block = InvalidIndex;
        std::vector<std::uint32_t> arguments;

        auto operator<=>(const BranchTarget&) const = default;
    };

    struct Instruction
    {
        Opcode opcode = Opcode::Unreachable;
        std::uint32_t result = InvalidIndex;
        std::uint32_t resultType = InvalidIndex;
        std::vector<std::uint32_t> operands;
        std::vector<BranchTarget> targets;
        std::uint32_t callee = InvalidIndex;
        std::uint32_t global = InvalidIndex;
        ConstantId constant = InvalidIndex;
        std::uint8_t unaryOperator = 0;
        std::uint8_t binaryOperator = 0;
        std::uint8_t conversionKind = 0;
        StringId selector = InvalidIndex;
        std::uint32_t projectionIndex = 0;
        std::vector<std::uint32_t> signatureTypes;
        std::vector<std::uint32_t> genericArguments;
        std::vector<std::uint8_t> captureKinds;
        std::vector<std::uint8_t> expandedOperands;
        std::vector<StringId> stringSegments;
        StringId specializationKey = InvalidIndex;
        std::uint8_t intrinsicFamily = 0;
        std::uint8_t asyncOperation = 0;
        std::uint8_t asyncExecutor = 0;
        std::uint32_t targetType = InvalidIndex;
        std::uint8_t resultOwnership = 0;
        std::uint8_t borrowLifetime = 0;
        std::uint32_t borrowOrigin = InvalidIndex;
        std::uint8_t storageClass = 0;
        std::uint8_t escapeClass = 0;
        std::uint8_t boundsCheck = 0;
        SourceSpan source;

        auto operator<=>(const Instruction&) const = default;
    };

    struct Block
    {
        std::uint32_t id = InvalidIndex;
        StringId name = InvalidIndex;
        std::vector<Parameter> parameters;
        std::vector<Instruction> instructions;
        SourceSpan source;

        auto operator<=>(const Block&) const = default;
    };

    struct Function
    {
        std::uint32_t id = InvalidIndex;
        StringId name = InvalidIndex;
        std::vector<Parameter> parameters;
        std::uint32_t returnType = InvalidIndex;
        std::uint32_t callableType = InvalidIndex;
        std::uint32_t ownerType = InvalidIndex;
        std::uint32_t methodSlot = 0;
        std::uint32_t captureParameterCount = 0;
        struct Capture
        {
            StringId name = InvalidIndex;
            std::uint32_t type = InvalidIndex;
            std::uint8_t kind = 0;

            auto operator<=>(const Capture&) const = default;
        };
        struct NativeAbiValue
        {
            std::uint32_t type = InvalidIndex;
            std::uint8_t passing = 0;
            std::uint8_t marshalling = 0;
            std::uint8_t callbackLifetime = 0;
            std::uint8_t callbackThread = 0;
            bool nullable = false;

            auto operator<=>(const NativeAbiValue&) const = default;
        };
        struct NativeBinding
        {
            StringId symbol = InvalidIndex;
            StringId header = InvalidIndex;
            StringId stableKey = InvalidIndex;
            StringId thunkSymbol = InvalidIndex;
            std::uint8_t language = 0;
            std::uint8_t callingConvention = 0;
            std::uint8_t exceptionBoundary = 0;
            std::uint8_t thunkKind = 0;
            std::uint8_t receiver = 0;
            std::vector<NativeAbiValue> parameters;
            NativeAbiValue result;
            std::vector<std::uint32_t> templateArguments;
            bool explicitTemplateArguments = false;
            bool requiresAdapter = false;

            auto operator<=>(const NativeBinding&) const = default;
        };
        struct CoroutineFrameSlot
        {
            std::uint32_t slot = 0;
            std::uint32_t value = InvalidIndex;
            std::uint32_t type = InvalidIndex;
            std::uint8_t kind = 0;
            std::uint8_t ownership = 0;
            std::uint8_t cleanup = 0;

            auto operator<=>(const CoroutineFrameSlot&) const = default;
        };
        struct CoroutineState
        {
            std::uint32_t index = 0;
            std::uint32_t suspendBlock = InvalidIndex;
            std::uint32_t resumeBlock = InvalidIndex;
            std::uint32_t awaitedTask = InvalidIndex;
            std::uint32_t resumedValue = InvalidIndex;
            std::uint32_t resultType = InvalidIndex;
            std::uint8_t executor = 0;
            bool cancellationPoint = true;

            auto operator<=>(const CoroutineState&) const = default;
        };
        struct CoroutineLayout
        {
            std::uint32_t resultType = InvalidIndex;
            std::vector<CoroutineFrameSlot> frameSlots;
            std::vector<CoroutineState> states;
            std::uint32_t retainedReceiver = InvalidIndex;
            bool cooperativeCancellation = true;
            bool maySwitchThreads = false;

            auto operator<=>(const CoroutineLayout&) const = default;
        };
        std::vector<Capture> captures;
        std::vector<std::uint32_t> genericParameters;
        std::uint32_t genericOrigin = InvalidIndex;
        std::vector<std::uint32_t> specializationArguments;
        StringId specializationKey = InvalidIndex;
        std::vector<Block> blocks;
        SourceSpan source;
        std::uint16_t flags = 0;
        bool hasNativeBinding = false;
        NativeBinding nativeBinding;
        bool hasCoroutine = false;
        CoroutineLayout coroutine;

        auto operator<=>(const Function&) const = default;
    };

    struct Global
    {
        std::uint32_t id = InvalidIndex;
        StringId name = InvalidIndex;
        std::uint32_t type = InvalidIndex;
        std::uint32_t initializer = InvalidIndex;
        SourceSpan source;
        std::uint8_t flags = 0;

        auto operator<=>(const Global&) const = default;
    };

    struct Type
    {
        struct Field
        {
            StringId name = InvalidIndex;
            std::uint32_t type = InvalidIndex;
            std::uint8_t visibility = 0;
            bool isMutable = true;

            auto operator<=>(const Field&) const = default;
        };
        struct Method
        {
            StringId name = InvalidIndex;
            std::vector<std::uint32_t> parameterTypes;
            std::uint32_t returnType = InvalidIndex;
            std::uint32_t function = InvalidIndex;
            std::uint32_t slot = 0;
            std::uint8_t visibility = 0;
            bool receiverMutable = true;
            bool isAbstract = false;

            auto operator<=>(const Method&) const = default;
        };
        struct DispatchEntry
        {
            std::uint32_t contractType = InvalidIndex;
            std::uint32_t slot = 0;
            std::uint32_t implementation = InvalidIndex;

            auto operator<=>(const DispatchEntry&) const = default;
        };
        struct EnumCase
        {
            StringId name = InvalidIndex;
            std::uint64_t rawValue = 0;

            auto operator<=>(const EnumCase&) const = default;
        };
        struct NativeBinding
        {
            StringId cppName = InvalidIndex;
            StringId header = InvalidIndex;
            bool standardLayout = true;
            bool triviallyCopyable = true;

            auto operator<=>(const NativeBinding&) const = default;
        };
        std::uint8_t kind = 0;
        StringId name = InvalidIndex;
        std::vector<std::uint32_t> arguments;
        std::uint64_t staticExtent = (std::numeric_limits<std::uint64_t>::max)();
        std::uint32_t extentParameter = InvalidIndex;
        std::uint8_t nominalKind = 0;
        std::uint8_t nominalRepresentation = 0;
        std::uint8_t nominalValueModel = 0;
        std::uint8_t ownership = 0;
        std::uint8_t cleanup = 0;
        std::uint8_t flags = 0;
        std::vector<std::uint32_t> baseTypes;
        std::vector<Field> fields;
        std::vector<Method> methods;
        std::vector<std::uint32_t> castTypes;
        std::vector<DispatchEntry> dispatchEntries;
        std::uint32_t destructor = InvalidIndex;
        std::uint32_t defaultConstructor = InvalidIndex;
        std::uint32_t fieldInitializer = InvalidIndex;
        std::uint32_t enumUnderlyingType = InvalidIndex;
        std::vector<EnumCase> enumCases;
        bool hasNativeBinding = false;
        NativeBinding nativeBinding;

        auto operator<=>(const Type&) const = default;
    };

    struct Module
    {
        std::vector<std::string> strings;
        std::vector<Constant> constants;
        std::vector<Type> types;
        std::vector<Global> globals;
        std::vector<Function> functions;
        StringId name = InvalidIndex;
        std::uint8_t moduleKind = 0;
        std::uint64_t stableId = 0;
        StringId logicalName = InvalidIndex;
        StringId stableKey = InvalidIndex;
        std::uint32_t abiDescriptorVersion = 0;
        Contract contract;

        [[nodiscard]] std::string_view string(StringId id) const noexcept;

        auto operator<=>(const Module&) const = default;
    };

    class StringTableBuilder final
    {
    public:
        explicit StringTableBuilder(Module& module);
        [[nodiscard]] StringId intern(std::string_view value);

    private:
        Module& module_;
        std::unordered_map<std::string, StringId> ids_;
    };
} // namespace wio::bytecode
