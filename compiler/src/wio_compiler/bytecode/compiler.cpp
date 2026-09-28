#include "wio/bytecode/compiler.h"

#include "wio/wir/lowered_ir_verifier.h"

#include <bit>
#include <limits>
#include <type_traits>
#include <utility>

namespace wio::bytecode
{
    namespace
    {
        template<typename Id>
        std::uint32_t rawId(const Id id)
        {
            return id ? id.value() : InvalidIndex;
        }

        Opcode mapOpcode(const wir::lowered::Opcode opcode)
        {
#define WIO_MAP_OPCODE(name) case wir::lowered::Opcode::name: return Opcode::name
            switch (opcode)
            {
                WIO_MAP_OPCODE(Constant);
                WIO_MAP_OPCODE(GenericConstant);
                WIO_MAP_OPCODE(DefaultValue);
                WIO_MAP_OPCODE(Unary);
                WIO_MAP_OPCODE(Binary);
                WIO_MAP_OPCODE(RangeContains);
                WIO_MAP_OPCODE(Convert);
                WIO_MAP_OPCODE(Call);
                WIO_MAP_OPCODE(NativeInvoke);
                WIO_MAP_OPCODE(FunctionReference);
                WIO_MAP_OPCODE(ClosureCreate);
                WIO_MAP_OPCODE(IndirectCall);
                WIO_MAP_OPCODE(ExtensionCall);
                WIO_MAP_OPCODE(MethodCall);
                WIO_MAP_OPCODE(VirtualCall);
                WIO_MAP_OPCODE(InterfaceCall);
                WIO_MAP_OPCODE(Upcast);
                WIO_MAP_OPCODE(CheckedCast);
                WIO_MAP_OPCODE(TypeTest);
                WIO_MAP_OPCODE(IdentityEqual);
                WIO_MAP_OPCODE(VariantTest);
                WIO_MAP_OPCODE(VariantPayload);
                WIO_MAP_OPCODE(ArrayLength);
                WIO_MAP_OPCODE(ArrayElement);
                WIO_MAP_OPCODE(ArrayCreate);
                WIO_MAP_OPCODE(ArrayGet);
                WIO_MAP_OPCODE(DictionaryCreate);
                WIO_MAP_OPCODE(DictionaryGet);
                WIO_MAP_OPCODE(DictionaryPlace);
                WIO_MAP_OPCODE(Interpolate);
                WIO_MAP_OPCODE(EnumConstant);
                WIO_MAP_OPCODE(IntrinsicCall);
                WIO_MAP_OPCODE(AnyBox);
                WIO_MAP_OPCODE(AnyCheckedCast);
                WIO_MAP_OPCODE(AnyTypeTest);
                WIO_MAP_OPCODE(NullableWrap);
                WIO_MAP_OPCODE(NullableUnwrap);
                WIO_MAP_OPCODE(IteratorCreate);
                WIO_MAP_OPCODE(IteratorHasNext);
                WIO_MAP_OPCODE(IteratorValue);
                WIO_MAP_OPCODE(IteratorAdvance);
                WIO_MAP_OPCODE(ResultIsError);
                WIO_MAP_OPCODE(ResultValue);
                WIO_MAP_OPCODE(ResultUnwrap);
                WIO_MAP_OPCODE(ResultPropagate);
                WIO_MAP_OPCODE(CancellationCheck);
                WIO_MAP_OPCODE(CoroutineSuspend);
                WIO_MAP_OPCODE(CoroutineResume);
                WIO_MAP_OPCODE(CoroutineComplete);
                WIO_MAP_OPCODE(GlobalPlace);
                WIO_MAP_OPCODE(LocalPlace);
                WIO_MAP_OPCODE(PlaceInit);
                WIO_MAP_OPCODE(Load);
                WIO_MAP_OPCODE(Store);
                WIO_MAP_OPCODE(FieldPlace);
                WIO_MAP_OPCODE(ArrayPlace);
                WIO_MAP_OPCODE(Borrow);
                WIO_MAP_OPCODE(ConstructComponent);
                WIO_MAP_OPCODE(ConstructObject);
                WIO_MAP_OPCODE(Retain);
                WIO_MAP_OPCODE(CopyValue);
                WIO_MAP_OPCODE(MoveValue);
                WIO_MAP_OPCODE(Replace);
                WIO_MAP_OPCODE(Release);
                WIO_MAP_OPCODE(DropValue);
                WIO_MAP_OPCODE(ReleasePlace);
                WIO_MAP_OPCODE(DropPlace);
                WIO_MAP_OPCODE(Return);
                WIO_MAP_OPCODE(Jump);
                WIO_MAP_OPCODE(CondJump);
                WIO_MAP_OPCODE(Unreachable);
            }
#undef WIO_MAP_OPCODE
            return Opcode::Unreachable;
        }

        SourceLocation compileLocation(const common::Location& source, StringTableBuilder& strings)
        {
            return SourceLocation{
                .file = source.file.empty() ? InvalidIndex : strings.intern(source.file),
                .line = source.line,
                .column = source.column};
        }

        SourceSpan compileSpan(const wir::SourceSpan& source, StringTableBuilder& strings)
        {
            return SourceSpan{
                .begin = compileLocation(source.begin, strings),
                .end = compileLocation(source.end, strings)};
        }

        Parameter compileParameter(const wir::lowered::Parameter& parameter, StringTableBuilder& strings)
        {
            return Parameter{
                .value = rawId(parameter.id),
                .name = strings.intern(parameter.name),
                .type = rawId(parameter.type),
                .ownership = static_cast<std::uint8_t>(parameter.ownership),
                .borrowLifetime = static_cast<std::uint8_t>(parameter.borrowLifetime),
                .source = compileSpan(parameter.source, strings)};
        }

        ConstantId compileConstant(const wir::typed::Literal& literal, Module& module, StringTableBuilder& strings)
        {
            Constant constant;
            std::visit(
                [&](const auto& value)
                {
                    using Value = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Value, std::monostate>)
                        constant.kind = ConstantKind::Empty;
                    else if constexpr (std::is_same_v<Value, wir::typed::NullLiteral>)
                        constant.kind = ConstantKind::Null;
                    else if constexpr (std::is_same_v<Value, bool>)
                    {
                        constant.kind = ConstantKind::Boolean;
                        constant.bits = value ? 1 : 0;
                    }
                    else if constexpr (std::is_same_v<Value, std::int64_t>)
                    {
                        constant.kind = ConstantKind::SignedInteger;
                        constant.bits = std::bit_cast<std::uint64_t>(value);
                    }
                    else if constexpr (std::is_same_v<Value, std::uint64_t>)
                    {
                        constant.kind = ConstantKind::UnsignedInteger;
                        constant.bits = value;
                    }
                    else if constexpr (std::is_same_v<Value, double>)
                    {
                        constant.kind = ConstantKind::Float64;
                        constant.bits = std::bit_cast<std::uint64_t>(value);
                    }
                    else if constexpr (std::is_same_v<Value, std::string>)
                    {
                        constant.kind = ConstantKind::String;
                        constant.string = strings.intern(value);
                    }
                },
                literal);
            const ConstantId id = static_cast<ConstantId>(module.constants.size());
            module.constants.push_back(constant);
            return id;
        }
    } // namespace

    CompileResult Compiler::compile(const wir::lowered::Module& source) const
    {
        CompileResult result;
        const wir::lowered::VerificationResult verification = wir::lowered::Verifier{}.verify(source);
        if (!verification.succeeded())
        {
            result.diagnostics_.reserve(verification.diagnostics().size());
            for (const auto& diagnostic : verification.diagnostics())
                result.diagnostics_.push_back({diagnostic.code, diagnostic.message, diagnostic.source});
            return result;
        }

        Module& module = result.module_;
        StringTableBuilder strings{module};
        module.name = strings.intern(source.name);
        module.moduleKind = static_cast<std::uint8_t>(source.contract.kind);
        module.stableId = source.contract.stableId;
        module.logicalName = strings.intern(source.contract.logicalName);
        module.stableKey = strings.intern(source.contract.stableKey);
        module.abiDescriptorVersion = source.contract.callTable.descriptorVersion;

        module.types.reserve(source.types.size());
        for (const wir::Type& sourceType : source.types.types())
        {
            Type type;
            type.kind = static_cast<std::uint8_t>(sourceType.kind);
            type.name = strings.intern(sourceType.name);
            type.arguments.reserve(sourceType.arguments.size());
            for (const wir::TypeId argument : sourceType.arguments)
                type.arguments.push_back(rawId(argument));
            if (sourceType.staticExtent)
                type.staticExtent = static_cast<std::uint64_t>(*sourceType.staticExtent);
            type.extentParameter = rawId(sourceType.extentParameter);
            type.nominalKind = static_cast<std::uint8_t>(sourceType.nominalKind);
            type.nominalRepresentation = static_cast<std::uint8_t>(sourceType.nominalRepresentation);
            type.nominalValueModel = static_cast<std::uint8_t>(sourceType.nominalValueModel);
            type.ownership = static_cast<std::uint8_t>(sourceType.ownership);
            type.cleanup = static_cast<std::uint8_t>(sourceType.cleanup);
            type.flags = sourceType.isMutable ? 0x01u : 0u;
            type.flags |= sourceType.hasConstructor ? 0x02u : 0u;
            type.flags |= sourceType.hasDestructor ? 0x04u : 0u;
            module.types.push_back(std::move(type));
        }

        module.globals.reserve(source.globals.size());
        for (const wir::lowered::Global& sourceGlobal : source.globals)
        {
            module.globals.push_back(Global{
                .id = rawId(sourceGlobal.id),
                .name = strings.intern(sourceGlobal.name),
                .type = rawId(sourceGlobal.type),
                .initializer = rawId(sourceGlobal.initializer),
                .source = compileSpan(sourceGlobal.source, strings),
                .flags = static_cast<std::uint8_t>((sourceGlobal.isMutable ? 0x01u : 0u) |
                                                   (sourceGlobal.isConst ? 0x02u : 0u))});
        }

        module.functions.reserve(source.functions.size());
        for (const wir::lowered::Function& sourceFunction : source.functions)
        {
            Function function;
            function.id = rawId(sourceFunction.id);
            function.name = strings.intern(sourceFunction.name);
            function.returnType = rawId(sourceFunction.returnType);
            function.callableType = rawId(sourceFunction.callableType);
            function.ownerType = rawId(sourceFunction.ownerType);
            function.methodSlot = sourceFunction.methodSlot;
            function.captureParameterCount = sourceFunction.captureParameterCount;
            function.genericOrigin = rawId(sourceFunction.genericOrigin);
            function.specializationKey = strings.intern(sourceFunction.specializationKey);
            function.source = compileSpan(sourceFunction.source, strings);
            function.flags = static_cast<std::uint16_t>((sourceFunction.isAsync ? 0x0001u : 0u) |
                                                        (sourceFunction.isExternal ? 0x0002u : 0u) |
                                                        (sourceFunction.isMethod ? 0x0004u : 0u) |
                                                        (sourceFunction.isAbstract ? 0x0008u : 0u) |
                                                        (sourceFunction.isExtension ? 0x0010u : 0u) |
                                                        (sourceFunction.isClosureBody ? 0x0020u : 0u) |
                                                        (sourceFunction.nativeBinding ? 0x0040u : 0u) |
                                                        (sourceFunction.coroutine ? 0x0080u : 0u));

            function.parameters.reserve(sourceFunction.parameters.size());
            for (const auto& parameter : sourceFunction.parameters)
                function.parameters.push_back(compileParameter(parameter, strings));
            for (const wir::TypeId type : sourceFunction.genericParameters)
                function.genericParameters.push_back(rawId(type));
            for (const wir::TypeId type : sourceFunction.specializationArguments)
                function.specializationArguments.push_back(rawId(type));

            function.blocks.reserve(sourceFunction.blocks.size());
            for (const wir::lowered::BasicBlock& sourceBlock : sourceFunction.blocks)
            {
                Block block;
                block.id = rawId(sourceBlock.id);
                block.name = strings.intern(sourceBlock.name);
                block.source = compileSpan(sourceBlock.source, strings);
                block.parameters.reserve(sourceBlock.parameters.size());
                for (const auto& parameter : sourceBlock.parameters)
                    block.parameters.push_back(compileParameter(parameter, strings));
                block.instructions.reserve(sourceBlock.instructions.size());
                for (const wir::lowered::Instruction& sourceInstruction : sourceBlock.instructions)
                {
                    Instruction instruction;
                    instruction.opcode = mapOpcode(sourceInstruction.opcode);
                    instruction.result = rawId(sourceInstruction.result);
                    instruction.resultType = rawId(sourceInstruction.resultType);
                    for (const wir::ValueId operand : sourceInstruction.operands)
                        instruction.operands.push_back(rawId(operand));
                    for (const auto& sourceTarget : sourceInstruction.targets)
                    {
                        BranchTarget target;
                        target.block = rawId(sourceTarget.block);
                        for (const wir::ValueId argument : sourceTarget.arguments)
                            target.arguments.push_back(rawId(argument));
                        instruction.targets.push_back(std::move(target));
                    }
                    instruction.callee = rawId(sourceInstruction.callee);
                    instruction.global = rawId(sourceInstruction.global);
                    instruction.constant = compileConstant(sourceInstruction.literal, module, strings);
                    instruction.unaryOperator = static_cast<std::uint8_t>(sourceInstruction.unaryOperator);
                    instruction.binaryOperator = static_cast<std::uint8_t>(sourceInstruction.binaryOperator);
                    instruction.conversionKind = static_cast<std::uint8_t>(sourceInstruction.conversionKind);
                    instruction.selector = strings.intern(sourceInstruction.selector);
                    instruction.projectionIndex = sourceInstruction.projectionIndex;
                    for (const wir::TypeId type : sourceInstruction.signatureTypes)
                        instruction.signatureTypes.push_back(rawId(type));
                    for (const wir::TypeId type : sourceInstruction.genericArguments)
                        instruction.genericArguments.push_back(rawId(type));
                    for (const wir::CaptureKind capture : sourceInstruction.captureKinds)
                        instruction.captureKinds.push_back(static_cast<std::uint8_t>(capture));
                    for (const bool expanded : sourceInstruction.expandedOperands)
                        instruction.expandedOperands.push_back(expanded ? 1u : 0u);
                    for (const std::string& segment : sourceInstruction.stringSegments)
                        instruction.stringSegments.push_back(strings.intern(segment));
                    instruction.specializationKey = strings.intern(sourceInstruction.specializationKey);
                    instruction.intrinsicFamily = static_cast<std::uint8_t>(sourceInstruction.intrinsicFamily);
                    instruction.asyncOperation = static_cast<std::uint8_t>(sourceInstruction.asyncOperation);
                    instruction.asyncExecutor = static_cast<std::uint8_t>(sourceInstruction.asyncExecutor);
                    instruction.targetType = rawId(sourceInstruction.targetType);
                    instruction.resultOwnership = static_cast<std::uint8_t>(sourceInstruction.resultOwnership);
                    instruction.borrowLifetime = static_cast<std::uint8_t>(sourceInstruction.borrowLifetime);
                    instruction.borrowOrigin = rawId(sourceInstruction.borrowOrigin);
                    instruction.storageClass = static_cast<std::uint8_t>(sourceInstruction.storageClass);
                    instruction.escapeClass = static_cast<std::uint8_t>(sourceInstruction.escapeClass);
                    instruction.boundsCheck = static_cast<std::uint8_t>(sourceInstruction.boundsCheck);
                    instruction.source = compileSpan(sourceInstruction.source, strings);
                    block.instructions.push_back(std::move(instruction));
                }
                function.blocks.push_back(std::move(block));
            }
            module.functions.push_back(std::move(function));
        }
        return result;
    }
} // namespace wio::bytecode
