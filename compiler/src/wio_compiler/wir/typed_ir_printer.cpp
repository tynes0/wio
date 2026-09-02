#include "wio/wir/typed_ir_printer.h"

#include <iomanip>
#include <sstream>
#include <type_traits>

namespace wio::wir::typed
{
    namespace
    {
        std::string typeRef(const TypeId id)
        {
            return id ? "!t" + std::to_string(id.value()) : "!invalid";
        }

        std::string valueRef(const ValueId id)
        {
            return id ? "%v" + std::to_string(id.value()) : "%invalid";
        }

        std::string blockRef(const BlockId id)
        {
            return id ? "^b" + std::to_string(id.value()) : "^invalid";
        }

        std::string functionRef(const FunctionId id)
        {
            return id ? "@f" + std::to_string(id.value()) : "@invalid";
        }

        std::string printLiteral(const Literal& literal)
        {
            return std::visit([](const auto& value) -> std::string
            {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, std::monostate>)
                    return "<none>";
                else if constexpr (std::is_same_v<Value, NullLiteral>)
                    return "null";
                else if constexpr (std::is_same_v<Value, bool>)
                    return value ? "true" : "false";
                else if constexpr (std::is_same_v<Value, std::string>)
                {
                    std::ostringstream stream;
                    stream << std::quoted(value);
                    return stream.str();
                }
                else
                    return std::to_string(value);
            }, literal);
        }

        void printParameters(std::ostringstream& stream, const std::vector<Parameter>& parameters)
        {
            for (std::size_t index = 0; index < parameters.size(); ++index)
            {
                if (index > 0)
                    stream << ", ";
                const Parameter& parameter = parameters[index];
                stream << valueRef(parameter.id);
                if (!parameter.name.empty())
                    stream << " " << std::quoted(parameter.name);
                stream << ": " << typeRef(parameter.type);
                if (parameter.ownership != ValueOwnership::Trivial)
                    stream << " [" << valueOwnershipName(parameter.ownership) << "]";
                if (parameter.borrowLifetime != BorrowLifetime::None)
                    stream << " lifetime=" << borrowLifetimeName(parameter.borrowLifetime);
            }
        }

        void printNativeBinding(std::ostringstream& stream, const NativeBinding& binding)
        {
            stream << " native[symbol=" << std::quoted(binding.symbol)
                   << " header=" << std::quoted(binding.header)
                   << " key=" << std::quoted(binding.stableKey)
                   << " thunk=" << std::quoted(binding.thunkSymbol)
                   << " language=" << nativeSymbolLanguageName(binding.language)
                   << " calling-convention=" << nativeCallingConventionName(binding.callingConvention)
                   << " exception=" << nativeExceptionBoundaryName(binding.exceptionBoundary)
                   << " thunk-kind=" << nativeThunkKindName(binding.thunkKind)
                   << " receiver=" << nativeReceiverKindName(binding.receiver)
                   << " params={";
            for (std::size_t index = 0; index < binding.parameters.size(); ++index)
            {
                if (index > 0) stream << ", ";
                const NativeAbiValue& parameter = binding.parameters[index];
                stream << typeRef(parameter.type) << ":" << nativePassingModeName(parameter.passing)
                       << "/" << nativeMarshallingKindName(parameter.marshalling);
                if (parameter.marshalling == NativeMarshallingKind::Callback)
                    stream << "/" << nativeCallbackLifetimeName(parameter.callbackLifetime)
                           << "/" << nativeCallbackThreadName(parameter.callbackThread);
                if (parameter.nullable) stream << "?";
            }
            stream << "} result=" << typeRef(binding.result.type) << ":"
                   << nativePassingModeName(binding.result.passing) << "/"
                   << nativeMarshallingKindName(binding.result.marshalling);
            if (binding.result.nullable) stream << "?";
            if (binding.requiresAdapter) stream << " adapter";
            stream << "]";
        }

        std::string printType(const Type& type)
        {
            std::ostringstream stream;
            stream << typeKindName(type.kind);
            if (!type.name.empty())
                stream << " " << std::quoted(type.name);
            if (!type.arguments.empty())
            {
                stream << "<";
                for (std::size_t index = 0; index < type.arguments.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << typeRef(type.arguments[index]);
                }
                stream << ">";
            }
            if (type.isMutable)
                stream << " mutable";
            if (type.staticExtent.has_value())
                stream << " extent=" << *type.staticExtent;
            if (type.nominalKind != NominalKind::None)
                stream << " nominal=" << nominalKindName(type.nominalKind);
            if (type.nominalRepresentation != NominalRepresentation::Wio)
                stream << " representation=" << nominalRepresentationName(type.nominalRepresentation);
            if (type.nominalValueModel != NominalValueModel::Regular)
                stream << " value-model=" << nominalValueModelName(type.nominalValueModel);
            if (!type.baseTypes.empty())
            {
                stream << " bases=[";
                for (std::size_t index = 0; index < type.baseTypes.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << typeRef(type.baseTypes[index]);
                }
                stream << "]";
            }
            if (!type.fields.empty())
            {
                stream << " fields={";
                for (std::size_t index = 0; index < type.fields.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    const FieldLayout& field = type.fields[index];
                    stream << std::quoted(field.name) << ":" << typeRef(field.type) << " "
                           << fieldVisibilityName(field.visibility);
                    if (field.isMutable)
                        stream << " mutable";
                }
                stream << "}";
            }
            if (!type.methods.empty())
            {
                stream << " methods={";
                for (std::size_t index = 0; index < type.methods.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    const MethodLayout& method = type.methods[index];
                    stream << std::quoted(method.name) << "#" << method.slot << "=" << functionRef(method.function);
                    if (method.isAbstract)
                        stream << " abstract";
                }
                stream << "}";
            }
            if (type.hasConstructor)
                stream << " has-constructor";
            if (type.hasDestructor)
                stream << " has-destructor";
            if (type.ownership != OwnershipModel::Trivial)
                stream << " ownership=" << ownershipModelName(type.ownership);
            if (type.cleanup != CleanupKind::None)
                stream << " cleanup=" << cleanupKindName(type.cleanup);
            if (type.nativeBinding)
                stream << " native-type[cpp=" << std::quoted(type.nativeBinding->cppName)
                       << " header=" << std::quoted(type.nativeBinding->header)
                       << " standard-layout=" << (type.nativeBinding->standardLayout ? "true" : "false")
                       << " trivially-copyable=" << (type.nativeBinding->triviallyCopyable ? "true" : "false")
                       << "]";
            return stream.str();
        }

        void printInstruction(std::ostringstream& stream, const Instruction& instruction)
        {
            stream << "    ";
            if (instruction.result)
            {
                stream << valueRef(instruction.result) << ": " << typeRef(instruction.resultType);
                if (instruction.resultOwnership != ValueOwnership::Trivial)
                    stream << " [" << valueOwnershipName(instruction.resultOwnership) << "]";
                if (instruction.borrowLifetime != BorrowLifetime::None)
                {
                    stream << " lifetime=" << borrowLifetimeName(instruction.borrowLifetime);
                    if (instruction.borrowOrigin)
                        stream << " origin=" << valueRef(instruction.borrowOrigin);
                }
                stream << " = ";
            }

            switch (instruction.opcode)
            {
            case Opcode::Constant:
                stream << "const " << printLiteral(instruction.literal);
                break;
            case Opcode::Unary:
                stream << unaryOperatorName(instruction.unaryOperator) << " " << valueRef(instruction.operands.at(0));
                break;
            case Opcode::Binary:
                stream << binaryOperatorName(instruction.binaryOperator) << " "
                       << valueRef(instruction.operands.at(0)) << ", " << valueRef(instruction.operands.at(1));
                break;
            case Opcode::Convert:
                stream << conversionKindName(instruction.conversionKind) << " "
                       << valueRef(instruction.operands.at(0));
                break;
            case Opcode::Call:
                stream << "call " << functionRef(instruction.callee) << "(";
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << valueRef(instruction.operands[index]);
                }
                stream << ")";
                break;
            case Opcode::NativeCall:
                stream << "native-call " << functionRef(instruction.callee) << "(";
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (index > 0) stream << ", ";
                    stream << valueRef(instruction.operands[index]);
                }
                stream << ")";
                break;
            case Opcode::FunctionReference:
                stream << "function-ref " << functionRef(instruction.callee);
                break;
            case Opcode::ClosureCreate:
                stream << "closure-create " << functionRef(instruction.callee) << " captures(";
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << captureKindName(instruction.captureKinds.at(index)) << " "
                           << valueRef(instruction.operands[index]);
                }
                stream << ")";
                break;
            case Opcode::IndirectCall:
                stream << "indirect-call " << valueRef(instruction.operands.at(0)) << "(";
                for (std::size_t index = 1; index < instruction.operands.size(); ++index)
                {
                    if (index > 1)
                        stream << ", ";
                    stream << valueRef(instruction.operands[index]);
                }
                stream << ")";
                break;
            case Opcode::ExtensionCall:
                stream << "extension-call " << typeRef(instruction.targetType) << "::"
                       << std::quoted(instruction.selector) << " " << functionRef(instruction.callee) << "(";
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << valueRef(instruction.operands[index]);
                }
                stream << ")";
                break;
            case Opcode::MethodCall:
            case Opcode::VirtualCall:
            case Opcode::InterfaceCall:
                stream << opcodeName(instruction.opcode) << " " << typeRef(instruction.targetType)
                       << "::" << std::quoted(instruction.selector) << "#" << instruction.projectionIndex
                       << " " << functionRef(instruction.callee) << "(";
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << valueRef(instruction.operands[index]);
                }
                stream << ")";
                break;
            case Opcode::Upcast:
            case Opcode::CheckedCast:
            case Opcode::TypeTest:
                stream << opcodeName(instruction.opcode) << " " << valueRef(instruction.operands.at(0))
                       << " to " << typeRef(instruction.targetType);
                break;
            case Opcode::IdentityEqual:
                stream << "identity-" << binaryOperatorName(instruction.binaryOperator) << " "
                       << valueRef(instruction.operands.at(0)) << ", " << valueRef(instruction.operands.at(1));
                break;
            case Opcode::VariantTest:
                stream << "variant-test " << valueRef(instruction.operands.at(0)) << ", "
                       << std::quoted(instruction.selector);
                break;
            case Opcode::VariantPayload:
                stream << "variant-payload " << valueRef(instruction.operands.at(0)) << ", "
                       << std::quoted(instruction.selector) << ", " << instruction.projectionIndex;
                break;
            case Opcode::ArrayLength:
                stream << "array-length " << valueRef(instruction.operands.at(0));
                break;
            case Opcode::ArrayElement:
                stream << "array-element " << valueRef(instruction.operands.at(0)) << ", "
                       << instruction.projectionIndex;
                break;
            case Opcode::ArrayCreate:
                stream << "array-create [";
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << valueRef(instruction.operands[index]);
                }
                stream << "]";
                break;
            case Opcode::ArrayGet:
                stream << "array-get " << valueRef(instruction.operands.at(0)) << ", "
                       << valueRef(instruction.operands.at(1));
                break;
            case Opcode::DictionaryCreate:
                stream << "dictionary-create " << std::quoted(instruction.selector) << " [";
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (index > 0) stream << ", ";
                    stream << valueRef(instruction.operands[index]);
                }
                stream << "]";
                break;
            case Opcode::DictionaryGet:
            case Opcode::DictionaryPlace:
                stream << opcodeName(instruction.opcode) << " " << valueRef(instruction.operands.at(0))
                       << ", " << valueRef(instruction.operands.at(1));
                break;
            case Opcode::Interpolate:
                stream << "interpolate " << intrinsicFamilyName(instruction.intrinsicFamily) << " [";
                for (std::size_t index = 0; index < instruction.stringSegments.size(); ++index)
                {
                    if (index > 0) stream << ", ";
                    stream << std::quoted(instruction.stringSegments[index]);
                    if (index < instruction.operands.size())
                        stream << ", " << valueRef(instruction.operands[index]);
                }
                stream << "]";
                break;
            case Opcode::EnumConstant:
                stream << "enum-constant " << typeRef(instruction.targetType) << "::"
                       << std::quoted(instruction.selector);
                break;
            case Opcode::IntrinsicCall:
                stream << "intrinsic-call " << intrinsicFamilyName(instruction.intrinsicFamily) << "::"
                       << std::quoted(instruction.selector) << "(";
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (index > 0) stream << ", ";
                    stream << valueRef(instruction.operands[index]);
                }
                stream << ")";
                break;
            case Opcode::AnyBox:
            case Opcode::AnyCheckedCast:
            case Opcode::AnyTypeTest:
            case Opcode::NullableWrap:
                stream << opcodeName(instruction.opcode) << " " << valueRef(instruction.operands.at(0))
                       << " to " << typeRef(instruction.targetType);
                break;
            case Opcode::Await:
                stream << "await " << valueRef(instruction.operands.at(0));
                break;
            case Opcode::ExecutorSwitch:
                stream << "executor-switch " << asyncExecutorKindName(instruction.asyncExecutor);
                break;
            case Opcode::LocalPlace:
                stream << "local-place " << std::quoted(instruction.selector);
                break;
            case Opcode::PlaceInit:
                stream << "place-init " << valueRef(instruction.operands.at(0)) << ", "
                       << valueRef(instruction.operands.at(1));
                break;
            case Opcode::Load:
                stream << "load " << valueRef(instruction.operands.at(0));
                break;
            case Opcode::Store:
                stream << "store " << valueRef(instruction.operands.at(0)) << ", "
                       << valueRef(instruction.operands.at(1));
                break;
            case Opcode::FieldPlace:
                stream << "field-place " << valueRef(instruction.operands.at(0)) << ", "
                       << std::quoted(instruction.selector);
                break;
            case Opcode::ArrayPlace:
                stream << "array-place " << valueRef(instruction.operands.at(0)) << ", "
                       << valueRef(instruction.operands.at(1));
                break;
            case Opcode::Borrow:
                stream << "borrow " << valueRef(instruction.operands.at(0));
                break;
            case Opcode::ConstructComponent:
            case Opcode::ConstructObject:
                stream << opcodeName(instruction.opcode) << " " << std::quoted(instruction.selector) << "(";
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << valueRef(instruction.operands[index]);
                }
                stream << ")";
                break;
            case Opcode::Copy:
            case Opcode::Move:
                stream << opcodeName(instruction.opcode) << " " << valueRef(instruction.operands.at(0));
                break;
            case Opcode::Replace:
                stream << "replace " << valueRef(instruction.operands.at(0)) << ", "
                       << valueRef(instruction.operands.at(1));
                break;
            case Opcode::Release:
                stream << "release " << valueRef(instruction.operands.at(0));
                break;
            case Opcode::Drop:
                stream << "drop " << valueRef(instruction.operands.at(0));
                break;
            case Opcode::Select:
                stream << "select " << valueRef(instruction.operands.at(0)) << ", "
                       << valueRef(instruction.operands.at(1)) << ", " << valueRef(instruction.operands.at(2));
                break;
            case Opcode::Return:
                stream << "return";
                if (!instruction.operands.empty())
                    stream << " " << valueRef(instruction.operands.front());
                break;
            case Opcode::Branch:
                stream << "branch " << blockRef(instruction.targets.at(0));
                if (!instruction.operands.empty())
                {
                    stream << "(";
                    for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                    {
                        if (index > 0)
                            stream << ", ";
                        stream << valueRef(instruction.operands[index]);
                    }
                    stream << ")";
                }
                break;
            case Opcode::CondBranch:
                stream << "cond-branch " << valueRef(instruction.operands.at(0)) << ", "
                       << blockRef(instruction.targets.at(0)) << ", " << blockRef(instruction.targets.at(1));
                break;
            case Opcode::Unreachable:
                stream << "unreachable";
                break;
            }
            if (!instruction.genericArguments.empty())
            {
                stream << " generic<";
                for (std::size_t index = 0; index < instruction.genericArguments.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << typeRef(instruction.genericArguments[index]);
                }
                stream << ">";
            }
            if (!instruction.specializationKey.empty())
                stream << " specialization=" << std::quoted(instruction.specializationKey);
            if (instruction.asyncOperation != AsyncOperation::None)
                stream << " async=" << asyncOperationName(instruction.asyncOperation)
                       << " executor=" << asyncExecutorKindName(instruction.asyncExecutor);
            stream << '\n';
        }
    }

    std::string Printer::print(const Module& module) const
    {
        std::ostringstream stream;
        stream << "typed-wir module " << std::quoted(module.name) << " {\n";

        stream << "  contract kind=" << moduleKindName(module.contract.kind)
               << " logical-name=" << std::quoted(module.contract.logicalName)
               << " stable-key=" << std::quoted(module.contract.stableKey)
               << " stable-id=" << module.contract.stableId
               << " abi-v" << module.contract.callTable.descriptorVersion << '\n';
        for (const ModuleImport& import : module.contract.imports)
            stream << "  import " << moduleImportKindName(import.kind) << " "
                   << std::quoted(import.logicalName) << " from " << std::quoted(import.sourcePath)
                   << " stable-id=" << import.stableId << '\n';
        for (const ModuleExport& entry : module.contract.exports)
        {
            stream << "  export[" << entry.callTableSlot << "] " << moduleExportKindName(entry.kind)
                   << "/" << moduleExportRoleName(entry.role) << " " << std::quoted(entry.logicalName)
                   << " symbol=" << std::quoted(entry.symbolName) << " stable-id=" << entry.stableId;
            if (!entry.roleName.empty()) stream << " role-name=" << std::quoted(entry.roleName);
            if (entry.function) stream << " function=" << functionRef(entry.function);
            if (entry.type) stream << " type=" << typeRef(entry.type);
            stream << '\n';
        }
        for (const ReflectionDescriptor& descriptor : module.contract.reflection)
            stream << "  reflect " << typeRef(descriptor.type) << " " << std::quoted(descriptor.logicalName)
                   << " stable-type-id=" << descriptor.stableTypeId
                   << " exported=" << (descriptor.isExported ? "true" : "false") << '\n';
        const ModuleLifecycle& lifecycle = module.contract.lifecycle;
        if (lifecycle.apiVersion || lifecycle.load || lifecycle.update || lifecycle.unload ||
            lifecycle.saveState || lifecycle.restoreState)
            stream << "  lifecycle api-version=" << functionRef(lifecycle.apiVersion)
                   << " load=" << functionRef(lifecycle.load)
                   << " update=" << functionRef(lifecycle.update)
                   << " save=" << functionRef(lifecycle.saveState)
                   << " restore=" << functionRef(lifecycle.restoreState)
                   << " unload=" << functionRef(lifecycle.unload) << '\n';

        for (std::size_t index = 0; index < module.types.size(); ++index)
        {
            const TypeId id{static_cast<TypeId::ValueType>(index)};
            stream << "  " << typeRef(id) << " = " << printType(module.types.get(id)) << '\n';
        }

        for (const Function& function : module.functions)
        {
            stream << "\n  ";
            if (function.isExternal)
                stream << "external ";
            if (function.isAsync)
                stream << "async ";
            if (function.isAbstract)
                stream << "abstract ";
            if (function.isExtension)
                stream << "extension ";
            if (function.isClosureBody)
                stream << "closure-body ";
            stream << "func " << functionRef(function.id) << " " << std::quoted(function.name) << "(";
            printParameters(stream, function.parameters);
            stream << ") -> " << typeRef(function.returnType) << " callable=" << typeRef(function.callableType);
            if (function.isMethod)
                stream << " owner=" << typeRef(function.ownerType) << " slot=" << function.methodSlot;
            if (function.nativeBinding)
                printNativeBinding(stream, *function.nativeBinding);
            if (!function.captures.empty())
            {
                stream << " captures={";
                for (std::size_t index = 0; index < function.captures.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    const CaptureLayout& capture = function.captures[index];
                    stream << std::quoted(capture.name) << ":" << typeRef(capture.type)
                           << " " << captureKindName(capture.kind);
                }
                stream << "}";
            }
            if (function.coroutine)
            {
                stream << " coroutine[result=" << typeRef(function.coroutine->resultType)
                       << " cancellation=" << (function.coroutine->cooperativeCancellation ? "cooperative" : "none")
                       << " thread-switch=" << (function.coroutine->maySwitchThreads ? "true" : "false")
                       << "]";
            }
            if (function.isExternal)
            {
                stream << '\n';
                continue;
            }
            stream << " {\n";

            for (const BasicBlock& block : function.blocks)
            {
                stream << "  " << blockRef(block.id);
                if (!block.name.empty())
                    stream << " " << std::quoted(block.name);
                if (!block.parameters.empty())
                {
                    stream << "(";
                    printParameters(stream, block.parameters);
                    stream << ")";
                }
                stream << ":\n";
                for (const Instruction& instruction : block.instructions)
                    printInstruction(stream, instruction);
            }
            stream << "  }\n";
        }

        stream << "}\n";
        return stream.str();
    }
}
