#include "wio/bytecode/compiler.h"
#include "wio/bytecode/enum_encoding.h"

#include "wio/wir/lowered_ir_verifier.h"

#include <bit>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace wio::bytecode
{
    namespace
    {
        template <typename Id> std::uint32_t rawId(const Id id)
        {
            return id ? id.value() : InvalidIndex;
        }

        Opcode mapOpcode(const wir::lowered::Opcode opcode)
        {
#define WIO_MAP_OPCODE(name)                                                                                           \
    case wir::lowered::Opcode::name:                                                                                   \
        return Opcode::name
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
            return SourceLocation{.file = source.file.empty() ? InvalidIndex : strings.intern(source.file),
                                  .line = source.line,
                                  .column = source.column};
        }

        SourceSpan compileSpan(const wir::SourceSpan& source, StringTableBuilder& strings)
        {
            return SourceSpan{.begin = compileLocation(source.begin, strings),
                              .end = compileLocation(source.end, strings)};
        }

        Parameter compileParameter(const wir::lowered::Parameter& parameter, StringTableBuilder& strings)
        {
            return Parameter{.value = rawId(parameter.id),
                             .name = strings.intern(parameter.name),
                             .type = rawId(parameter.type),
                             .ownership = encodeEnum(parameter.ownership),
                             .borrowLifetime = encodeEnum(parameter.borrowLifetime),
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
        std::unordered_map<std::uint32_t, std::uint32_t> functionIds;
        functionIds.reserve(source.functions.size());
        for (std::size_t index = 0; index < source.functions.size(); ++index)
            functionIds.emplace(rawId(source.functions[index].id), static_cast<std::uint32_t>(index));
        const auto functionId = [&](const wir::FunctionId id)
        {
            if (!id)
                return InvalidIndex;
            const auto found = functionIds.find(id.value());
            return found == functionIds.end() ? InvalidIndex : found->second;
        };
        std::unordered_map<std::uint32_t, std::uint32_t> globalIds;
        globalIds.reserve(source.globals.size());
        for (std::size_t index = 0; index < source.globals.size(); ++index)
            globalIds.emplace(rawId(source.globals[index].id), static_cast<std::uint32_t>(index));
        const auto globalId = [&](const wir::GlobalId id)
        {
            if (!id)
                return InvalidIndex;
            const auto found = globalIds.find(id.value());
            return found == globalIds.end() ? InvalidIndex : found->second;
        };
        module.name = strings.intern(source.name);
        module.moduleKind = encodeEnum(source.contract.kind);
        module.stableId = source.contract.stableId;
        module.logicalName = strings.intern(source.contract.logicalName);
        module.stableKey = strings.intern(source.contract.stableKey);
        module.abiDescriptorVersion = source.contract.callTable.descriptorVersion;

        for (const wir::ModuleImport& sourceImport : source.contract.imports)
        {
            Import import;
            import.stableId = sourceImport.stableId;
            import.logicalName = strings.intern(sourceImport.logicalName);
            import.sourcePath = strings.intern(sourceImport.sourcePath);
            import.alias = strings.intern(sourceImport.alias);
            for (const std::string& symbol : sourceImport.importedSymbols)
                import.importedSymbols.push_back(strings.intern(symbol));
            import.kind = encodeEnum(sourceImport.kind);
            import.importAll = sourceImport.importAll;
            module.contract.imports.push_back(std::move(import));
        }
        for (const wir::ModuleExport& sourceExport : source.contract.exports)
        {
            Export exportRecord;
            exportRecord.stableId = sourceExport.stableId;
            exportRecord.stableKey = strings.intern(sourceExport.stableKey);
            exportRecord.logicalName = strings.intern(sourceExport.logicalName);
            exportRecord.symbolName = strings.intern(sourceExport.symbolName);
            exportRecord.kind = encodeEnum(sourceExport.kind);
            exportRecord.role = encodeEnum(sourceExport.role);
            exportRecord.roleName = strings.intern(sourceExport.roleName);
            exportRecord.function = functionId(sourceExport.function);
            exportRecord.type = rawId(sourceExport.type);
            for (const wir::TypeId type : sourceExport.parameterTypes)
                exportRecord.parameterTypes.push_back(rawId(type));
            exportRecord.returnType = rawId(sourceExport.returnType);
            for (const wir::TypeId type : sourceExport.genericArguments)
                exportRecord.genericArguments.push_back(rawId(type));
            exportRecord.callTableSlot = sourceExport.callTableSlot;
            exportRecord.isAsync = sourceExport.isAsync;
            module.contract.exports.push_back(std::move(exportRecord));
        }
        for (const wir::AttributeApplicationDescriptor& sourceAttribute : source.contract.attributes)
        {
            AttributeApplication attribute;
            attribute.stableId = sourceAttribute.stableId;
            attribute.canonicalName = strings.intern(sourceAttribute.canonicalName);
            attribute.originParent = strings.intern(sourceAttribute.originParent);
            attribute.selector = strings.intern(sourceAttribute.selector);
            attribute.targetKind = encodeEnum(sourceAttribute.targetKind);
            attribute.origin = encodeEnum(sourceAttribute.origin);
            attribute.targetStableId = sourceAttribute.targetStableId;
            attribute.targetType = rawId(sourceAttribute.targetType);
            attribute.targetFunction = functionId(sourceAttribute.targetFunction);
            attribute.parameterIndex = sourceAttribute.parameterIndex;
            attribute.sourceOrder = sourceAttribute.sourceOrder;
            attribute.runtimeRetained = sourceAttribute.runtimeRetained;
            for (const wir::AttributeArgumentDescriptor& sourceArgument : sourceAttribute.arguments)
            {
                attribute.arguments.push_back(AttributeArgument{.name = strings.intern(sourceArgument.name),
                                                                .sourceText = strings.intern(sourceArgument.sourceText),
                                                                .type = rawId(sourceArgument.type),
                                                                .usedDefault = sourceArgument.usedDefault});
            }
            for (const wir::AttributeProcessorDescriptor& sourceProcessor : sourceAttribute.processors)
            {
                attribute.processors.push_back(
                    AttributeProcessor{.stableId = sourceProcessor.stableId,
                                       .canonicalTypeName = strings.intern(sourceProcessor.canonicalTypeName),
                                       .hookName = strings.intern(sourceProcessor.hookName),
                                       .hookMode = strings.intern(sourceProcessor.hookMode),
                                       .phase = encodeEnum(sourceProcessor.phase),
                                       .processorType = rawId(sourceProcessor.processorType),
                                       .hookFunction = functionId(sourceProcessor.hookFunction),
                                       .valueType = rawId(sourceProcessor.valueType)});
            }
            module.contract.attributes.push_back(std::move(attribute));
        }
        for (const wir::ReflectionDescriptor& sourceReflection : source.contract.reflection)
        {
            Reflection reflection;
            reflection.stableTypeId = sourceReflection.stableTypeId;
            reflection.logicalName = strings.intern(sourceReflection.logicalName);
            reflection.type = rawId(sourceReflection.type);
            reflection.nominalKind = encodeEnum(sourceReflection.nominalKind);
            reflection.isExported = sourceReflection.isExported;
            reflection.runtimeVisible = sourceReflection.runtimeVisible;
            for (const std::string& name : sourceReflection.genericParameterNames)
                reflection.genericParameterNames.push_back(strings.intern(name));
            for (const wir::TypeId type : sourceReflection.genericArguments)
                reflection.genericArguments.push_back(rawId(type));
            reflection.attributes = sourceReflection.attributes;
            for (const wir::ReflectedFieldDescriptor& sourceField : sourceReflection.fields)
            {
                reflection.fields.push_back(ReflectedField{.stableId = sourceField.stableId,
                                                           .name = strings.intern(sourceField.name),
                                                           .type = rawId(sourceField.type),
                                                           .visibility = encodeEnum(sourceField.visibility),
                                                           .isMutable = sourceField.isMutable,
                                                           .attributes = sourceField.attributes});
            }
            for (const wir::ReflectedMethodDescriptor& sourceMethod : sourceReflection.methods)
            {
                ReflectedMethod method;
                method.stableId = sourceMethod.stableId;
                method.name = strings.intern(sourceMethod.name);
                method.function = functionId(sourceMethod.function);
                method.returnType = rawId(sourceMethod.returnType);
                for (const wir::TypeId type : sourceMethod.parameterTypes)
                    method.parameterTypes.push_back(rawId(type));
                method.slot = sourceMethod.slot;
                method.isAsync = sourceMethod.isAsync;
                method.visibility = encodeEnum(sourceMethod.visibility);
                method.attributes = sourceMethod.attributes;
                reflection.methods.push_back(std::move(method));
            }
            for (const wir::ReflectedCaseDescriptor& sourceCase : sourceReflection.cases)
            {
                reflection.cases.push_back(ReflectedCase{.stableId = sourceCase.stableId,
                                                         .name = strings.intern(sourceCase.name),
                                                         .attributes = sourceCase.attributes});
            }
            module.contract.reflection.push_back(std::move(reflection));
        }
        for (const wir::SystemDescriptor& sourceSystem : source.contract.systems)
        {
            module.contract.systems.push_back(System{.stableId = sourceSystem.stableId,
                                                     .logicalName = strings.intern(sourceSystem.logicalName),
                                                     .type = rawId(sourceSystem.type),
                                                     .start = functionId(sourceSystem.start),
                                                     .update = functionId(sourceSystem.update),
                                                     .close = functionId(sourceSystem.close)});
        }
        if (source.contract.application)
        {
            const wir::ApplicationDescriptor& sourceApplication = *source.contract.application;
            Application& application = module.contract.application;
            module.contract.hasApplication = true;
            application.stableId = sourceApplication.stableId;
            application.logicalName = strings.intern(sourceApplication.logicalName);
            application.type = rawId(sourceApplication.type);
            application.construct = functionId(sourceApplication.construct);
            application.entry = functionId(sourceApplication.entry);
            application.start = functionId(sourceApplication.start);
            application.update = functionId(sourceApplication.update);
            application.close = functionId(sourceApplication.close);
            application.exit = functionId(sourceApplication.exit);
            for (const wir::TypeId system : sourceApplication.systems)
                application.systems.push_back(rawId(system));
            for (const wir::ApplicationStageDescriptor& sourceStage : sourceApplication.stages)
            {
                Stage stage;
                stage.stableId = sourceStage.stableId;
                stage.name = strings.intern(sourceStage.name);
                stage.after = strings.intern(sourceStage.after);
                stage.fixedHzBits = std::bit_cast<std::uint64_t>(sourceStage.fixedHz);
                stage.order = sourceStage.order;
                stage.kind = encodeEnum(sourceStage.kind);
                stage.affinity = encodeEnum(sourceStage.affinity);
                stage.legacyExplicit = sourceStage.legacyExplicit;
                for (const wir::ApplicationStageRun& sourceRun : sourceStage.runs)
                {
                    StageRun run;
                    run.targetName = strings.intern(sourceRun.targetName);
                    run.methodName = strings.intern(sourceRun.methodName);
                    run.targetType = rawId(sourceRun.targetType);
                    run.function = functionId(sourceRun.function);
                    for (const wir::ApplicationResourceBinding& sourceResource : sourceRun.resources)
                    {
                        run.resources.push_back(ResourceBinding{.name = strings.intern(sourceResource.name),
                                                                .type = rawId(sourceResource.type),
                                                                .access = encodeEnum(sourceResource.access)});
                    }
                    run.applicationTarget = sourceRun.applicationTarget;
                    run.acceptsDelta = sourceRun.acceptsDelta;
                    stage.runs.push_back(std::move(run));
                }
                application.stages.push_back(std::move(stage));
            }
            application.hostOwnsStorage = sourceApplication.hostOwnsStorage;
            application.nonBlockingScheduling = sourceApplication.nonBlockingScheduling;
        }
        module.contract.lifecycle = Lifecycle{.apiVersion = functionId(source.contract.lifecycle.apiVersion),
                                              .load = functionId(source.contract.lifecycle.load),
                                              .update = functionId(source.contract.lifecycle.update),
                                              .unload = functionId(source.contract.lifecycle.unload),
                                              .saveState = functionId(source.contract.lifecycle.saveState),
                                              .restoreState = functionId(source.contract.lifecycle.restoreState),
                                              .stateSchemaVersion = source.contract.lifecycle.stateSchemaVersion};
        module.contract.callTableStableId = source.contract.callTable.stableId;
        module.contract.callTableEntries = source.contract.callTable.entries;

        module.types.reserve(source.types.size());
        for (const wir::Type& sourceType : source.types.types())
        {
            Type type;
            type.kind = encodeEnum(sourceType.kind);
            type.name = strings.intern(sourceType.name);
            type.arguments.reserve(sourceType.arguments.size());
            for (const wir::TypeId argument : sourceType.arguments)
                type.arguments.push_back(rawId(argument));
            if (sourceType.staticExtent)
                type.staticExtent = static_cast<std::uint64_t>(*sourceType.staticExtent);
            type.extentParameter = rawId(sourceType.extentParameter);
            type.nominalKind = encodeEnum(sourceType.nominalKind);
            type.nominalRepresentation = encodeEnum(sourceType.nominalRepresentation);
            type.nominalValueModel = encodeEnum(sourceType.nominalValueModel);
            type.ownership = encodeEnum(sourceType.ownership);
            type.cleanup = encodeEnum(sourceType.cleanup);
            type.flags = sourceType.isMutable ? 0x01u : 0u;
            type.flags |= sourceType.hasConstructor ? 0x02u : 0u;
            type.flags |= sourceType.hasDestructor ? 0x04u : 0u;
            for (const wir::TypeId base : sourceType.baseTypes)
                type.baseTypes.push_back(rawId(base));
            for (const wir::FieldLayout& sourceField : sourceType.fields)
            {
                type.fields.push_back(Type::Field{.name = strings.intern(sourceField.name),
                                                  .type = rawId(sourceField.type),
                                                  .visibility = encodeEnum(sourceField.visibility),
                                                  .isMutable = sourceField.isMutable});
            }
            for (const wir::MethodLayout& sourceMethod : sourceType.methods)
            {
                Type::Method method;
                method.name = strings.intern(sourceMethod.name);
                for (const wir::TypeId parameter : sourceMethod.parameterTypes)
                    method.parameterTypes.push_back(rawId(parameter));
                method.returnType = rawId(sourceMethod.returnType);
                method.function = functionId(sourceMethod.function);
                method.slot = sourceMethod.slot;
                method.visibility = encodeEnum(sourceMethod.visibility);
                method.receiverMutable = sourceMethod.receiverMutable;
                method.isAbstract = sourceMethod.isAbstract;
                type.methods.push_back(std::move(method));
            }
            for (const wir::TypeId castType : sourceType.castTypes)
                type.castTypes.push_back(rawId(castType));
            for (const wir::DispatchEntry& sourceDispatch : sourceType.dispatchEntries)
            {
                type.dispatchEntries.push_back(
                    Type::DispatchEntry{.contractType = rawId(sourceDispatch.contractType),
                                        .slot = sourceDispatch.slot,
                                        .implementation = functionId(sourceDispatch.implementation)});
            }
            type.destructor = functionId(sourceType.destructor);
            type.defaultConstructor = functionId(sourceType.defaultConstructor);
            type.fieldInitializer = functionId(sourceType.fieldInitializer);
            type.enumUnderlyingType = rawId(sourceType.enumUnderlyingType);
            for (const wir::EnumCaseLayout& sourceCase : sourceType.enumCases)
                type.enumCases.push_back(Type::EnumCase{strings.intern(sourceCase.name), sourceCase.rawValue});
            if (sourceType.nativeBinding)
            {
                type.hasNativeBinding = true;
                type.nativeBinding =
                    Type::NativeBinding{.cppName = strings.intern(sourceType.nativeBinding->cppName),
                                        .header = strings.intern(sourceType.nativeBinding->header),
                                        .standardLayout = sourceType.nativeBinding->standardLayout,
                                        .triviallyCopyable = sourceType.nativeBinding->triviallyCopyable};
            }
            module.types.push_back(std::move(type));
        }

        module.globals.reserve(source.globals.size());
        for (const wir::lowered::Global& sourceGlobal : source.globals)
        {
            module.globals.push_back(Global{.id = globalId(sourceGlobal.id),
                                            .name = strings.intern(sourceGlobal.name),
                                            .type = rawId(sourceGlobal.type),
                                            .initializer = functionId(sourceGlobal.initializer),
                                            .source = compileSpan(sourceGlobal.source, strings),
                                            .flags = static_cast<std::uint8_t>((sourceGlobal.isMutable ? 0x01u : 0u) |
                                                                               (sourceGlobal.isConst ? 0x02u : 0u))});
        }

        module.functions.reserve(source.functions.size());
        for (const wir::lowered::Function& sourceFunction : source.functions)
        {
            Function function;
            function.id = functionId(sourceFunction.id);
            function.name = strings.intern(sourceFunction.name);
            function.returnType = rawId(sourceFunction.returnType);
            function.callableType = rawId(sourceFunction.callableType);
            function.ownerType = rawId(sourceFunction.ownerType);
            function.methodSlot = sourceFunction.methodSlot;
            function.captureParameterCount = sourceFunction.captureParameterCount;
            function.genericOrigin = functionId(sourceFunction.genericOrigin);
            function.specializationKey = strings.intern(sourceFunction.specializationKey);
            function.source = compileSpan(sourceFunction.source, strings);
            function.flags = static_cast<std::uint16_t>(
                (sourceFunction.isAsync ? 0x0001u : 0u) | (sourceFunction.isExternal ? 0x0002u : 0u) |
                (sourceFunction.isMethod ? 0x0004u : 0u) | (sourceFunction.isAbstract ? 0x0008u : 0u) |
                (sourceFunction.isExtension ? 0x0010u : 0u) | (sourceFunction.isClosureBody ? 0x0020u : 0u) |
                (sourceFunction.nativeBinding ? 0x0040u : 0u) | (sourceFunction.coroutine ? 0x0080u : 0u));

            for (const wir::CaptureLayout& sourceCapture : sourceFunction.captures)
            {
                function.captures.push_back(Function::Capture{.name = strings.intern(sourceCapture.name),
                                                              .type = rawId(sourceCapture.type),
                                                              .kind = encodeEnum(sourceCapture.kind)});
            }
            if (sourceFunction.nativeBinding)
            {
                const wir::NativeBinding& sourceBinding = *sourceFunction.nativeBinding;
                function.hasNativeBinding = true;
                function.nativeBinding.symbol = strings.intern(sourceBinding.symbol);
                function.nativeBinding.header = strings.intern(sourceBinding.header);
                function.nativeBinding.stableKey = strings.intern(sourceBinding.stableKey);
                function.nativeBinding.thunkSymbol = strings.intern(sourceBinding.thunkSymbol);
                function.nativeBinding.language = encodeEnum(sourceBinding.language);
                function.nativeBinding.callingConvention = encodeEnum(sourceBinding.callingConvention);
                function.nativeBinding.exceptionBoundary = encodeEnum(sourceBinding.exceptionBoundary);
                function.nativeBinding.thunkKind = encodeEnum(sourceBinding.thunkKind);
                function.nativeBinding.receiver = encodeEnum(sourceBinding.receiver);
                const auto compileAbiValue = [](const wir::NativeAbiValue& value)
                {
                    return Function::NativeAbiValue{.type = rawId(value.type),
                                                    .passing = encodeEnum(value.passing),
                                                    .marshalling = encodeEnum(value.marshalling),
                                                    .callbackLifetime = encodeEnum(value.callbackLifetime),
                                                    .callbackThread = encodeEnum(value.callbackThread),
                                                    .nullable = value.nullable};
                };
                for (const wir::NativeAbiValue& parameter : sourceBinding.parameters)
                    function.nativeBinding.parameters.push_back(compileAbiValue(parameter));
                function.nativeBinding.result = compileAbiValue(sourceBinding.result);
                for (const wir::TypeId argument : sourceBinding.templateArguments)
                    function.nativeBinding.templateArguments.push_back(rawId(argument));
                function.nativeBinding.explicitTemplateArguments = sourceBinding.explicitTemplateArguments;
                function.nativeBinding.requiresAdapter = sourceBinding.requiresAdapter;
            }
            if (sourceFunction.coroutine)
            {
                const wir::CoroutineLayout& sourceCoroutine = *sourceFunction.coroutine;
                function.hasCoroutine = true;
                function.coroutine.resultType = rawId(sourceCoroutine.resultType);
                for (const wir::CoroutineFrameSlot& sourceSlot : sourceCoroutine.frameSlots)
                {
                    function.coroutine.frameSlots.push_back(
                        Function::CoroutineFrameSlot{.slot = sourceSlot.slot,
                                                     .value = rawId(sourceSlot.value),
                                                     .type = rawId(sourceSlot.type),
                                                     .kind = encodeEnum(sourceSlot.kind),
                                                     .ownership = encodeEnum(sourceSlot.ownership),
                                                     .cleanup = encodeEnum(sourceSlot.cleanup)});
                }
                for (const wir::CoroutineState& sourceState : sourceCoroutine.states)
                {
                    function.coroutine.states.push_back(
                        Function::CoroutineState{.index = sourceState.index,
                                                 .suspendBlock = rawId(sourceState.suspendBlock),
                                                 .resumeBlock = rawId(sourceState.resumeBlock),
                                                 .awaitedTask = rawId(sourceState.awaitedTask),
                                                 .resumedValue = rawId(sourceState.resumedValue),
                                                 .resultType = rawId(sourceState.resultType),
                                                 .executor = encodeEnum(sourceState.executor),
                                                 .cancellationPoint = sourceState.cancellationPoint});
                }
                function.coroutine.retainedReceiver = rawId(sourceCoroutine.retainedReceiver);
                function.coroutine.cooperativeCancellation = sourceCoroutine.cooperativeCancellation;
                function.coroutine.maySwitchThreads = sourceCoroutine.maySwitchThreads;
            }

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
                    instruction.callee = functionId(sourceInstruction.callee);
                    instruction.global = globalId(sourceInstruction.global);
                    instruction.constant = compileConstant(sourceInstruction.literal, module, strings);
                    instruction.unaryOperator = encodeEnum(sourceInstruction.unaryOperator);
                    instruction.binaryOperator = encodeEnum(sourceInstruction.binaryOperator);
                    instruction.conversionKind = encodeEnum(sourceInstruction.conversionKind);
                    instruction.selector = strings.intern(sourceInstruction.selector);
                    instruction.projectionIndex = sourceInstruction.projectionIndex;
                    for (const wir::TypeId type : sourceInstruction.signatureTypes)
                        instruction.signatureTypes.push_back(rawId(type));
                    for (const wir::TypeId type : sourceInstruction.genericArguments)
                        instruction.genericArguments.push_back(rawId(type));
                    for (const wir::CaptureKind capture : sourceInstruction.captureKinds)
                        instruction.captureKinds.push_back(encodeEnum(capture));
                    for (const bool expanded : sourceInstruction.expandedOperands)
                        instruction.expandedOperands.push_back(expanded ? 1u : 0u);
                    for (const std::string& segment : sourceInstruction.stringSegments)
                        instruction.stringSegments.push_back(strings.intern(segment));
                    instruction.specializationKey = strings.intern(sourceInstruction.specializationKey);
                    instruction.intrinsicFamily = encodeEnum(sourceInstruction.intrinsicFamily);
                    instruction.asyncOperation = encodeEnum(sourceInstruction.asyncOperation);
                    instruction.asyncExecutor = encodeEnum(sourceInstruction.asyncExecutor);
                    instruction.targetType = rawId(sourceInstruction.targetType);
                    instruction.resultOwnership = encodeEnum(sourceInstruction.resultOwnership);
                    instruction.borrowLifetime = encodeEnum(sourceInstruction.borrowLifetime);
                    instruction.borrowOrigin = rawId(sourceInstruction.borrowOrigin);
                    instruction.storageClass = encodeEnum(sourceInstruction.storageClass);
                    instruction.escapeClass = encodeEnum(sourceInstruction.escapeClass);
                    instruction.boundsCheck = encodeEnum(sourceInstruction.boundsCheck);
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
