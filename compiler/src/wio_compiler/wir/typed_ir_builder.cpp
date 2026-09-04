#include "wio/wir/typed_ir_builder.h"

#include "wio/ast/attribute_queries.h"
#include "wio/ast/attribute_contract.h"
#include "wio/common/utility.h"
#include "wio/sema/intrinsic_member_resolver.h"
#include "wio/sema/scope.h"
#include "wio/sema/symbol.h"
#include "wio/sema/type.h"

#include <algorithm>
#include <iomanip>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace wio::wir::typed
{
    class BuildContext final
    {
    public:
        BuildContext(BuildResult& result, const BuildOptions& options)
            : result_(result), options_(options) {}

        void build(const Ref<Program>& program)
        {
            result_.module_.name = program && program->location().hasFile()
                ? program->location().file
                : "module";
            if (!program)
            {
                report("WIR2000", "Cannot build Typed WIR from a null program.");
                return;
            }

            std::string logicalName = options_.logicalModuleName.empty()
                ? result_.module_.name
                : options_.logicalModuleName;
            std::ranges::replace(logicalName, '\\', '/');
            if (options_.logicalModuleName.empty())
                if (const std::size_t slash = logicalName.find_last_of('/'); slash != std::string::npos)
                    logicalName.erase(0, slash + 1);
            result_.module_.contract.logicalName = logicalName;
            if (options_.moduleKind)
                result_.module_.contract.kind = *options_.moduleKind;
            result_.module_.contract.lifecycle.stateSchemaVersion = options_.stateSchemaVersion;
            result_.module_.contract.stableKey = "wio.module:" + logicalName;
            result_.module_.contract.stableId = stableModuleHash(result_.module_.contract.stableKey);
            result_.module_.contract.callTable.stableId = stableModuleHash(
                result_.module_.contract.stableKey + ":sdk-call-table:v" +
                std::to_string(ModuleAbiDescriptorVersion));
            collectImports(program->statements);
            collectFunctions(program->statements);
            collectTypeDeclarations(program->statements);
            nextFunctionId_ = static_cast<FunctionId::ValueType>(declarations_.size());
            for (const DeclarationInfo& declaration : declarations_)
                buildFunction(declaration);
            buildTypeExportsAndReflection();
            buildApplicationSystemAttributeReflection();
            const ModuleLifecycle& lifecycle = result_.module_.contract.lifecycle;
            if (!options_.moduleKind && (!result_.module_.contract.exports.empty() || lifecycle.apiVersion || lifecycle.load ||
                lifecycle.update || lifecycle.unload || lifecycle.saveState || lifecycle.restoreState))
                result_.module_.contract.kind = ModuleKind::WioLibrary;
        }

    private:
        struct FunctionState
        {
            Function* function = nullptr;
            std::size_t blockIndex = 0;
            ValueId::ValueType nextValue = 0;
            BlockId::ValueType nextBlock = 0;
            std::unordered_map<const sema::Symbol*, ValueId> values;
            std::vector<const sema::Symbol*> valueOrder;
            std::unordered_map<const sema::Symbol*, ValueId> places;
            std::vector<const sema::Symbol*> placeOrder;
            std::unordered_map<ValueId::ValueType, ValueOwnership> ownerships;
            std::unordered_set<const sema::Symbol*> movedPlaces;
            ValueId selfValue;
            TypeId selfType;
        };

        struct DeclarationInfo
        {
            const FunctionDeclaration* declaration = nullptr;
            Ref<sema::Type> ownerType;
            bool isExtension = false;
        };

        struct TypeDeclarationInfo
        {
            const ASTNode* declaration = nullptr;
            const std::vector<NodePtr<AttributeStatement>>* attributes = nullptr;
            Ref<sema::Type> type;
            ModuleExportKind exportKind = ModuleExportKind::ComponentType;
            std::string role;
            bool exportable = true;
        };

        struct LoopContext
        {
            BlockId continueTarget;
            BlockId breakTarget;
            std::vector<const sema::Symbol*> carriedSymbols;
            std::size_t placeDepth = 0;
        };

        BuildResult& result_;
        const BuildOptions& options_;
        std::vector<DeclarationInfo> declarations_;
        std::vector<TypeDeclarationInfo> typeDeclarations_;
        std::unordered_map<const sema::Symbol*, FunctionId> functionsBySymbol_;
        std::unordered_map<const sema::Symbol*, const FunctionDeclaration*> declarationsBySymbol_;
        std::unordered_map<const sema::Type*, TypeId> typesBySemanticType_;
        std::unordered_map<const LambdaExpression*, FunctionId> lambdaFunctions_;
        FunctionId::ValueType nextFunctionId_ = 0;
        std::vector<LoopContext> loopContexts_;

        TypeId referenceType(const TypeId referredType, const bool isMutable)
        {
            return result_.module_.types.intern(Type{
                .kind = TypeKind::Reference,
                .arguments = {referredType},
                .isMutable = isMutable,
                .ownership = OwnershipModel::Borrowed
            });
        }

        const Type* underlyingNominalType(TypeId typeId, TypeId* nominalTypeId = nullptr) const
        {
            const Type* type = result_.module_.types.tryGet(typeId);
            if (type && type->kind == TypeKind::Reference && type->arguments.size() == 1)
            {
                typeId = type->arguments.front();
                type = result_.module_.types.tryGet(typeId);
            }
            if (!type || type->kind != TypeKind::Named ||
                (type->nominalKind != NominalKind::Object && type->nominalKind != NominalKind::Interface))
                return nullptr;
            if (nominalTypeId)
                *nominalTypeId = typeId;
            return type;
        }

        IntrinsicFamily intrinsicFamilyFor(TypeId typeId) const
        {
            const Type* type = result_.module_.types.tryGet(typeId);
            if (type && type->kind == TypeKind::Reference && type->arguments.size() == 1)
                type = result_.module_.types.tryGet(type->arguments.front());
            if (!type)
                return IntrinsicFamily::None;
            switch (type->kind)
            {
            case TypeKind::Array: return IntrinsicFamily::Array;
            case TypeKind::Dictionary: return IntrinsicFamily::Dictionary;
            case TypeKind::String: return IntrinsicFamily::String;
            case TypeKind::Text: return IntrinsicFamily::Text;
            case TypeKind::Nullable: return IntrinsicFamily::Nullable;
            case TypeKind::Any: return IntrinsicFamily::Any;
            case TypeKind::Named:
                if (type->nominalKind == NominalKind::Enum) return IntrinsicFamily::Enum;
                if (type->nominalKind == NominalKind::Flagset) return IntrinsicFamily::Flagset;
                switch (type->nominalValueModel)
                {
                case NominalValueModel::Tuple: return IntrinsicFamily::Tuple;
                case NominalValueModel::Span: return IntrinsicFamily::Span;
                case NominalValueModel::Option: return IntrinsicFamily::Option;
                case NominalValueModel::Result: return IntrinsicFamily::Result;
                case NominalValueModel::Regular: break;
                }
                break;
            default: break;
            }
            return IntrinsicFamily::None;
        }

        bool nominalDerivesFrom(const TypeId sourceType, const TypeId destinationType) const
        {
            if (sourceType == destinationType)
                return true;
            const Type* source = result_.module_.types.tryGet(sourceType);
            if (!source || source->kind != TypeKind::Named)
                return false;
            return std::ranges::any_of(source->baseTypes, [&](const TypeId base)
                { return nominalDerivesFrom(base, destinationType); });
        }

        const MethodLayout* findMethodLayout(
            const TypeId ownerTypeId,
            const Ref<sema::Symbol>& symbol) const
        {
            const Type* owner = result_.module_.types.tryGet(ownerTypeId);
            if (!owner || !symbol)
                return nullptr;
            const auto function = functionsBySymbol_.find(symbol.Get());
            const auto found = std::ranges::find_if(owner->methods, [&](const MethodLayout& method)
            {
                return function != functionsBySymbol_.end() && method.function == function->second;
            });
            return found == owner->methods.end() ? nullptr : &*found;
        }

        bool autoReadableReference(const Type& reference) const
        {
            if (reference.kind != TypeKind::Reference || reference.arguments.size() != 1)
                return false;
            const Type* referred = result_.module_.types.tryGet(reference.arguments.front());
            return referred && !(referred->kind == TypeKind::Named &&
                (referred->nominalKind == NominalKind::Object || referred->nominalKind == NominalKind::Interface));
        }

        ValueOwnership ownershipForType(const TypeId typeId) const
        {
            const Type* type = result_.module_.types.tryGet(typeId);
            if (!type || type->ownership == OwnershipModel::Trivial)
                return ValueOwnership::Trivial;
            if (type->ownership == OwnershipModel::Borrowed)
                return ValueOwnership::Borrowed;
            return ValueOwnership::Owned;
        }

        bool typeRequiresCleanup(const TypeId typeId) const
        {
            const Type* type = result_.module_.types.tryGet(typeId);
            return type && requiresCleanup(*type);
        }

        static bool hasBuiltinAttribute(
            const std::vector<NodePtr<AttributeStatement>>& attributes,
            const Attribute attribute)
        {
            return attribute_queries::hasAttribute(attributes, attribute);
        }

        static std::string attributeValue(
            const std::vector<NodePtr<AttributeStatement>>& attributes,
            const Attribute attribute)
        {
            const Token* token = attribute_queries::getFirstAttributeArg(attributes, attribute);
            return token ? token->value : std::string{};
        }

        static std::uint64_t stableHash(const std::string_view text)
        {
            std::uint64_t hash = 14695981039346656037ull;
            for (const unsigned char byte : text)
            {
                hash ^= byte;
                hash *= 1099511628211ull;
            }
            return hash;
        }

        std::string typeStableKey(const TypeId typeId) const
        {
            const Type* type = result_.module_.types.tryGet(typeId);
            if (!type)
                return "invalid";
            std::string key{typeKindName(type->kind)};
            if (!type->name.empty())
                key += ":" + type->name;
            if (!type->arguments.empty())
            {
                key += "<";
                for (std::size_t index = 0; index < type->arguments.size(); ++index)
                {
                    if (index > 0) key += ",";
                    key += typeStableKey(type->arguments[index]);
                }
                key += ">";
            }
            return key;
        }

        std::string exportStableKey(
            const Function& function,
            const std::string_view logicalName,
            const std::vector<TypeId>& genericArguments) const
        {
            std::string key = result_.module_.contract.stableKey + ":export:" +
                std::string{logicalName} + "(";
            const std::size_t hidden = function.captureParameterCount + (function.isMethod ? 1u : 0u);
            for (std::size_t index = hidden; index < function.parameters.size(); ++index)
            {
                if (index > hidden) key += ",";
                key += typeStableKey(function.parameters[index].type);
            }
            key += ")->" + typeStableKey(function.returnType);
            if (!genericArguments.empty())
            {
                key += "<";
                for (std::size_t index = 0; index < genericArguments.size(); ++index)
                {
                    if (index > 0) key += ",";
                    key += typeStableKey(genericArguments[index]);
                }
                key += ">";
            }
            return key;
        }

        TypeId substituteGenericType(
            const TypeId source,
            const std::vector<TypeId>& parameters,
            const std::vector<TypeId>& arguments)
        {
            for (std::size_t index = 0; index < parameters.size() && index < arguments.size(); ++index)
                if (source == parameters[index])
                    return arguments[index];
            const Type* sourceType = result_.module_.types.tryGet(source);
            if (!sourceType || sourceType->arguments.empty())
                return source;
            Type instantiated = *sourceType;
            bool changed = false;
            for (TypeId& argument : instantiated.arguments)
            {
                const TypeId replacement = substituteGenericType(argument, parameters, arguments);
                changed |= replacement != argument;
                argument = replacement;
            }
            return changed ? result_.module_.types.intern(std::move(instantiated)) : source;
        }

        static std::string nativeThunkSymbol(const std::string_view stableKey)
        {
            std::ostringstream stream;
            stream << "_wio_native_" << std::hex << std::setfill('0') << std::setw(16)
                   << stableHash(stableKey);
            return stream.str();
        }

        NativeMarshallingKind nativeMarshallingKind(TypeId typeId) const
        {
            const Type* type = result_.module_.types.tryGet(typeId);
            if (!type)
                return NativeMarshallingKind::Generic;
            if (type->kind == TypeKind::Reference && type->arguments.size() == 1)
                return nativeMarshallingKind(type->arguments.front());
            if (type->kind == TypeKind::Nullable && type->arguments.size() == 1)
                return nativeMarshallingKind(type->arguments.front());
            switch (type->kind)
            {
            case TypeKind::Void: return NativeMarshallingKind::Void;
            case TypeKind::String: return NativeMarshallingKind::Utf8String;
            case TypeKind::Text: return NativeMarshallingKind::UnicodeText;
            case TypeKind::Opaque: return NativeMarshallingKind::OpaqueHandle;
            case TypeKind::Function: return NativeMarshallingKind::Callback;
            case TypeKind::GenericParameter: return NativeMarshallingKind::Generic;
            case TypeKind::Named:
                if (type->nominalKind == NominalKind::Enum || type->nominalKind == NominalKind::Flagset)
                    return NativeMarshallingKind::Scalar;
                if (type->nominalRepresentation == NominalRepresentation::NativePod)
                    return NativeMarshallingKind::NativePod;
                if (type->nominalKind == NominalKind::Object ||
                    type->nominalKind == NominalKind::Interface)
                    return NativeMarshallingKind::ObjectHandle;
                return NativeMarshallingKind::RuntimeValue;
            case TypeKind::Any:
            case TypeKind::Array:
            case TypeKind::Dictionary:
            case TypeKind::AsyncTask:
                return NativeMarshallingKind::RuntimeValue;
            default:
                return NativeMarshallingKind::Scalar;
            }
        }

        NativeAbiValue nativeAbiValue(const TypeId typeId, const bool isReturn) const
        {
            NativeAbiValue result{
                .type = typeId,
                .passing = NativePassingMode::Value,
                .marshalling = nativeMarshallingKind(typeId)
            };
            const Type* type = result_.module_.types.tryGet(typeId);
            if (!type)
                return result;
            result.nullable = type->kind == TypeKind::Nullable;
            if (type->kind == TypeKind::Reference && type->arguments.size() == 1)
                result.passing = type->isMutable ? NativePassingMode::BorrowMut : NativePassingMode::Borrow;
            else if (isReturn && requiresCleanup(*type))
                result.passing = NativePassingMode::ReturnOwned;
            return result;
        }

        NativeBinding makeNativeBinding(
            const FunctionDeclaration& declaration,
            const Function& function,
            const sema::FunctionType& functionType)
        {
            std::string symbol = attributeValue(declaration.attributes, Attribute::CppName);
            if (symbol.empty())
                symbol = declaration.extensionMemberName.empty()
                    ? (declaration.name ? declaration.name->token.value : function.name)
                    : declaration.extensionMemberName;
            NativeBinding binding{
                .symbol = std::move(symbol),
                .header = attributeValue(declaration.attributes, Attribute::CppHeader),
                .language = NativeSymbolLanguage::Cpp,
                .callingConvention = NativeCallingConvention::PlatformDefault,
                .exceptionBoundary = NativeExceptionBoundary::TranslateToWioFailure,
                .receiver = function.isExtension
                    ? (declaration.extensionMutableReceiver
                        ? NativeReceiverKind::MutableReference
                        : NativeReceiverKind::ConstReference)
                    : NativeReceiverKind::None
            };
            binding.stableKey = "cpp:" + binding.symbol + ":" + functionType.toString();
            binding.thunkSymbol = nativeThunkSymbol(binding.stableKey);
            for (const Ref<sema::Type>& parameter : functionType.paramTypes)
                binding.parameters.push_back(nativeAbiValue(mapType(parameter, &declaration), false));
            binding.result = nativeAbiValue(function.returnType, true);
            const bool needsMarshalling = std::ranges::any_of(
                binding.parameters,
                [](const NativeAbiValue& value)
                {
                    return value.marshalling != NativeMarshallingKind::Scalar &&
                        value.marshalling != NativeMarshallingKind::NativePod &&
                        value.marshalling != NativeMarshallingKind::OpaqueHandle;
                }) || (binding.result.marshalling != NativeMarshallingKind::Void &&
                        binding.result.marshalling != NativeMarshallingKind::Scalar &&
                        binding.result.marshalling != NativeMarshallingKind::NativePod &&
                        binding.result.marshalling != NativeMarshallingKind::OpaqueHandle);
            if (!function.genericParameters.empty())
                binding.thunkKind = NativeThunkKind::TemplateSpecialization;
            else if (needsMarshalling || binding.receiver != NativeReceiverKind::None)
                binding.thunkKind = NativeThunkKind::Adapter;
            binding.requiresAdapter = binding.thunkKind != NativeThunkKind::Direct;
            return binding;
        }

        bool isNativeFunction(const FunctionId id) const
        {
            if (!id || id.value() >= declarations_.size())
                return false;
            const FunctionDeclaration* declaration = declarations_[id.value()].declaration;
            return declaration && hasBuiltinAttribute(declaration->attributes, Attribute::Native);
        }

        TypeId asyncResultType(const Function& function) const
        {
            if (!function.isAsync)
                return function.returnType;
            const Type* task = result_.module_.types.tryGet(function.returnType);
            return task && task->kind == TypeKind::AsyncTask && task->arguments.size() == 1
                ? task->arguments.front()
                : TypeId{};
        }

        AsyncOperation asyncOperationFor(const FunctionId id) const
        {
            if (!id || id.value() >= declarations_.size() || !declarations_[id.value()].declaration)
                return AsyncOperation::None;
            const FunctionDeclaration& declaration = *declarations_[id.value()].declaration;
            std::string name = attributeValue(declaration.attributes, Attribute::CppName);
            if (name.empty() && declaration.name)
                name = declaration.name->token.value;

            const auto endsWith = [&](const std::string_view suffix)
                { return name == suffix || name.ends_with(std::string{"::"} + std::string{suffix}); };
            if (endsWith("AsyncYield") || endsWith("Yield")) return AsyncOperation::Yield;
            if (endsWith("AsyncSleep") || endsWith("Sleep")) return AsyncOperation::Sleep;
            if (endsWith("StartAsync") || endsWith("Start")) return AsyncOperation::Start;
            if (endsWith("CancelAfterAsync") || endsWith("CancelAfter")) return AsyncOperation::CancelAfter;
            if (endsWith("CancelAsync") || endsWith("Cancel")) return AsyncOperation::Cancel;
            if (endsWith("DetachAsync") || endsWith("Detach")) return AsyncOperation::Detach;
            if (endsWith("RunBlockingAsync") || endsWith("SpawnBlocking")) return AsyncOperation::SpawnBlocking;
            if (endsWith("RunIoAsync") || endsWith("SpawnIo")) return AsyncOperation::SpawnIo;
            if (endsWith("RunAsync") || endsWith("SpawnWorker")) return AsyncOperation::SpawnWorker;
            if (endsWith("AsyncScopeSpawn") || endsWith("Spawn")) return AsyncOperation::Spawn;
            if (endsWith("AsyncScopeJoin") || endsWith("Join")) return AsyncOperation::Join;
            if (endsWith("AsyncWaitFor") || endsWith("BlockOn") || endsWith("Wait")) return AsyncOperation::Wait;
            return AsyncOperation::None;
        }

        void decorateAsyncOperation(Instruction& instruction) const
        {
            instruction.asyncOperation = asyncOperationFor(instruction.callee);
            switch (instruction.asyncOperation)
            {
            case AsyncOperation::SpawnWorker: instruction.asyncExecutor = AsyncExecutorKind::Worker; break;
            case AsyncOperation::SpawnBlocking: instruction.asyncExecutor = AsyncExecutorKind::Blocking; break;
            case AsyncOperation::SpawnIo: instruction.asyncExecutor = AsyncExecutorKind::Io; break;
            default: break;
            }
        }

        void rememberOwnership(FunctionState& state, const ValueId value, const ValueOwnership ownership)
        {
            if (value)
                state.ownerships[value.value()] = ownership;
        }

        ValueOwnership valueOwnership(const FunctionState& state, const ValueId value) const
        {
            const auto found = value ? state.ownerships.find(value.value()) : state.ownerships.end();
            return found != state.ownerships.end() ? found->second : ValueOwnership::Trivial;
        }

        ValueId ensureOwned(
            const ValueId value,
            const TypeId type,
            const ASTNode* source,
            FunctionState& state)
        {
            if (!value || !typeRequiresCleanup(type) || valueOwnership(state, value) == ValueOwnership::Owned)
                return value;
            const ValueId copy{state.nextValue++};
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::Copy,
                .result = copy,
                .resultType = type,
                .operands = {value},
                .resultOwnership = ValueOwnership::Owned,
                .source = source ? SourceSpan::at(source->location()) : SourceSpan{}
            });
            rememberOwnership(state, copy, ValueOwnership::Owned);
            return copy;
        }

        void releaseOwnedTemporary(
            const ValueId value,
            const ASTNode* source,
            FunctionState& state)
        {
            if (!value || valueOwnership(state, value) != ValueOwnership::Owned)
                return;
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::Release,
                .operands = {value},
                .source = source ? SourceSpan::at(source->location()) : SourceSpan{}
            });
        }

        ValueId emitLoad(
            const ValueId place,
            const TypeId valueType,
            const ASTNode* source,
            FunctionState& state)
        {
            const ValueId result{state.nextValue++};
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::Load,
                .result = result,
                .resultType = valueType,
                .operands = {place},
                .resultOwnership = typeRequiresCleanup(valueType)
                    ? ValueOwnership::Borrowed
                    : ValueOwnership::Trivial,
                .borrowLifetime = typeRequiresCleanup(valueType)
                    ? BorrowLifetime::Lexical
                    : BorrowLifetime::None,
                .borrowOrigin = typeRequiresCleanup(valueType) ? place : ValueId{},
                .source = source ? SourceSpan::at(source->location()) : SourceSpan{}
            });
            rememberOwnership(
                state,
                result,
                typeRequiresCleanup(valueType) ? ValueOwnership::Borrowed : ValueOwnership::Trivial);
            return result;
        }

        void emitDropsFrom(
            const std::size_t firstPlace,
            FunctionState& state,
            const ASTNode* source)
        {
            for (std::size_t index = state.placeOrder.size(); index > firstPlace; --index)
            {
                const sema::Symbol* symbol = state.placeOrder[index - 1];
                if (state.movedPlaces.contains(symbol))
                    continue;
                const auto place = state.places.find(symbol);
                if (place == state.places.end())
                    continue;
                const TypeId valueType = mapType(symbol->type, source);
                if (!typeRequiresCleanup(valueType))
                    continue;
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::Drop,
                    .operands = {place->second},
                    .source = source ? SourceSpan::at(source->location()) : SourceSpan{}
                });
            }
        }

        ValueId adaptPlaceMutability(
            const ValueId place,
            const TypeId placeTypeId,
            const bool needsMutable,
            const ASTNode* source,
            FunctionState& state)
        {
            const Type* placeType = result_.module_.types.tryGet(placeTypeId);
            if (!placeType || placeType->kind != TypeKind::Reference || placeType->arguments.size() != 1)
            {
                report("WIR2323", "Addressable expression did not produce a reference place.", source);
                return {};
            }
            if (needsMutable && !placeType->isMutable)
            {
                report("WIR2324", "A mutable place was requested from a read-only view.", source);
                return {};
            }
            if (needsMutable || !placeType->isMutable)
                return place;

            const ValueId result{state.nextValue++};
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::Borrow,
                .result = result,
                .resultType = referenceType(placeType->arguments.front(), false),
                .operands = {place},
                .resultOwnership = ValueOwnership::Borrowed,
                .borrowLifetime = BorrowLifetime::Lexical,
                .borrowOrigin = place,
                .source = source ? SourceSpan::at(source->location()) : SourceSpan{}
            });
            rememberOwnership(state, result, ValueOwnership::Borrowed);
            return result;
        }

        static BasicBlock& currentBlock(FunctionState& state)
        {
            return state.function->blocks.at(state.blockIndex);
        }

        static bool blockIsTerminated(FunctionState& state)
        {
            const BasicBlock& block = currentBlock(state);
            return !block.instructions.empty() && isTerminator(block.instructions.back().opcode);
        }

        static std::size_t createBlock(
            FunctionState& state,
            std::string name,
            const SourceSpan& source)
        {
            const std::size_t index = state.function->blocks.size();
            state.function->blocks.push_back(BasicBlock{
                .id = BlockId{state.nextBlock++},
                .name = std::move(name),
                .source = source
            });
            return index;
        }

        void report(std::string code, std::string message, const ASTNode* node = nullptr)
        {
            result_.diagnostics_.push_back(BuildDiagnostic{
                .code = std::move(code),
                .message = std::move(message),
                .source = node ? SourceSpan::at(node->location()) : SourceSpan{}
            });
        }

        void collectFunction(
            const FunctionDeclaration* function,
            Ref<sema::Type> ownerType = nullptr,
            const bool isExtension = false)
        {
            if (!function)
                return;
            const FunctionId id{static_cast<FunctionId::ValueType>(declarations_.size())};
            declarations_.push_back(DeclarationInfo{
                .declaration = function,
                .ownerType = std::move(ownerType),
                .isExtension = isExtension
            });
            const Ref<sema::Symbol> symbol = function->name
                ? function->name->referencedSymbol.Lock()
                : nullptr;
            if (symbol)
            {
                functionsBySymbol_[symbol.Get()] = id;
                declarationsBySymbol_[symbol.Get()] = function;
            }
        }

        static Ref<sema::Type> declaredOwnerType(const NodePtr<Identifier>& name)
        {
            const Ref<sema::Symbol> symbol = name ? name->referencedSymbol.Lock() : nullptr;
            return symbol ? symbol->type : nullptr;
        }

        void collectImports(const std::vector<NodePtr<Statement>>& statements)
        {
            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;
                if (const auto* use = statement->as<UseStatement>())
                {
                    ModuleImport import;
                    import.logicalName = use->moduleName.empty() ? use->modulePath : use->moduleName;
                    import.sourcePath = use->modulePath;
                    import.alias = use->aliasName;
                    import.importedSymbols = use->importedSymbols;
                    import.kind = use->isCppHeader
                        ? ModuleImportKind::NativeHeader
                        : (use->isStdLib ? ModuleImportKind::StandardModule : ModuleImportKind::WioModule);
                    import.importAll = use->importAllIntoScope;
                    const std::string identity = result_.module_.contract.stableKey + ":import:" +
                        std::string{moduleImportKindName(import.kind)} + ":" + import.sourcePath +
                        ":" + import.logicalName;
                    import.stableId = stableHash(identity);
                    result_.module_.contract.imports.push_back(std::move(import));
                    continue;
                }
                if (const auto* group = statement->as<DeclarationGroup>())
                    collectImports(group->declarations);
                else if (const auto* realm = statement->as<RealmDeclaration>())
                    collectImports(realm->statements);
            }
        }

        void collectTypeDeclarations(const std::vector<NodePtr<Statement>>& statements)
        {
            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;
                if (const auto* component = statement->as<ComponentDeclaration>())
                {
                    typeDeclarations_.push_back(TypeDeclarationInfo{
                        .declaration = component,
                        .attributes = &component->attributes,
                        .type = declaredOwnerType(component->name),
                        .exportKind = ModuleExportKind::ComponentType,
                        .role = component->attributeTargetOverride
                    });
                    continue;
                }
                if (const auto* object = statement->as<ObjectDeclaration>())
                {
                    typeDeclarations_.push_back(TypeDeclarationInfo{
                        .declaration = object,
                        .attributes = &object->attributes,
                        .type = declaredOwnerType(object->name),
                        .exportKind = ModuleExportKind::ObjectType,
                        .role = "type"
                    });
                    continue;
                }
                if (const auto* interfaceDeclaration = statement->as<InterfaceDeclaration>())
                {
                    typeDeclarations_.push_back(TypeDeclarationInfo{
                        .declaration = interfaceDeclaration,
                        .attributes = &interfaceDeclaration->attributes,
                        .type = declaredOwnerType(interfaceDeclaration->name),
                        .role = "type",
                        .exportable = false
                    });
                    continue;
                }
                if (const auto* enumeration = statement->as<EnumDeclaration>())
                {
                    typeDeclarations_.push_back(TypeDeclarationInfo{
                        .declaration = enumeration,
                        .attributes = &enumeration->attributes,
                        .type = declaredOwnerType(enumeration->name),
                        .role = "type",
                        .exportable = false
                    });
                    continue;
                }
                if (const auto* flagset = statement->as<FlagsetDeclaration>())
                {
                    typeDeclarations_.push_back(TypeDeclarationInfo{
                        .declaration = flagset,
                        .attributes = &flagset->attributes,
                        .type = declaredOwnerType(flagset->name),
                        .role = "type",
                        .exportable = false
                    });
                    continue;
                }
                if (const auto* group = statement->as<DeclarationGroup>())
                    collectTypeDeclarations(group->declarations);
                else if (const auto* realm = statement->as<RealmDeclaration>())
                    collectTypeDeclarations(realm->statements);
            }
        }

        void buildTypeExportsAndReflection()
        {
            std::unordered_set<TypeId::ValueType> exportedTypes;
            for (const TypeDeclarationInfo& declaration : typeDeclarations_)
            {
                if (!declaration.type)
                    continue;
                const TypeId type = mapType(declaration.type, declaration.declaration);
                const Type* descriptor = result_.module_.types.tryGet(type);
                if (!descriptor || descriptor->kind != TypeKind::Named)
                    continue;
                const bool exported = declaration.exportable && declaration.attributes &&
                    hasBuiltinAttribute(*declaration.attributes, Attribute::Export);
                if (!exported)
                    continue;
                exportedTypes.insert(type.value());
                ModuleExport entry;
                entry.logicalName = descriptor->name;
                entry.symbolName = descriptor->name;
                entry.kind = declaration.exportKind;
                entry.type = type;
                entry.callTableSlot = static_cast<std::uint32_t>(result_.module_.contract.exports.size());
                entry.stableKey = result_.module_.contract.stableKey + ":type-export:" +
                    typeStableKey(type);
                entry.stableId = stableHash(entry.stableKey);
                result_.module_.contract.callTable.entries.push_back(entry.stableId);
                result_.module_.contract.exports.push_back(std::move(entry));
            }

            for (std::size_t index = 0; index < result_.module_.types.size(); ++index)
            {
                const TypeId typeId{static_cast<TypeId::ValueType>(index)};
                const Type& type = result_.module_.types.get(typeId);
                if (type.kind != TypeKind::Named)
                    continue;
                result_.module_.contract.reflection.push_back(ReflectionDescriptor{
                    .stableTypeId = stableHash(result_.module_.contract.stableKey + ":type:" +
                        typeStableKey(typeId)),
                    .logicalName = type.name,
                    .type = typeId,
                    .nominalKind = type.nominalKind,
                    .isExported = exportedTypes.contains(typeId.value())
                });
            }
        }

        static AttributeOriginKind attributeOrigin(const AttributeOrigin origin)
        {
            switch (origin)
            {
            case AttributeOrigin::Direct: return AttributeOriginKind::Direct;
            case AttributeOrigin::Inherited: return AttributeOriginKind::Inherited;
            case AttributeOrigin::Scoped: return AttributeOriginKind::Scoped;
            case AttributeOrigin::Composed: return AttributeOriginKind::Composed;
            case AttributeOrigin::Generated: return AttributeOriginKind::Generated;
            case AttributeOrigin::Compiler: return AttributeOriginKind::Compiler;
            }
            return AttributeOriginKind::Direct;
        }

        static AttributeProcessorPhase processorPhase(const std::string_view phase)
        {
            if (phase == "validation") return AttributeProcessorPhase::Validation;
            if (phase == "derive") return AttributeProcessorPhase::Derive;
            if (phase == "pre") return AttributeProcessorPhase::Pre;
            if (phase == "post") return AttributeProcessorPhase::Post;
            if (phase == "finally") return AttributeProcessorPhase::Finally;
            if (phase == "around") return AttributeProcessorPhase::Around;
            return AttributeProcessorPhase::Unknown;
        }

        std::uint64_t typeMetadataId(const TypeId type) const
        {
            return stableHash(result_.module_.contract.stableKey + ":type:" + typeStableKey(type));
        }

        const Function* findFunction(const FunctionId id) const
        {
            const auto found = std::ranges::find_if(result_.module_.functions,
                [&](const Function& function) { return function.id == id; });
            return found == result_.module_.functions.end() ? nullptr : &*found;
        }

        std::uint64_t functionMetadataId(const FunctionId id) const
        {
            const Function* function = findFunction(id);
            return stableHash(result_.module_.contract.stableKey + ":function:" +
                std::to_string(id.value()) + ":" + (function ? function->name : std::string{}));
        }

        static MetadataTargetKind declarationTargetKind(
            const FunctionDeclaration& declaration,
            const bool isMethod)
        {
            if (declaration.attributeTargetOverride == "handler") return MetadataTargetKind::Handler;
            if (declaration.attributeTargetOverride == "method" || isMethod || declaration.isExtensionMethod)
                return MetadataTargetKind::Method;
            return MetadataTargetKind::Function;
        }

        std::vector<std::uint64_t> appendAttributeApplications(
            const std::vector<NodePtr<AttributeStatement>>& attributes,
            const MetadataTargetKind targetKind,
            const std::uint64_t targetStableId,
            const TypeId targetType = {},
            const FunctionId targetFunction = {},
            const std::string_view selector = {},
            const std::uint32_t parameterIndex = 0)
        {
            std::vector<std::uint64_t> ids;
            std::uint32_t occurrence = 0;
            for (const auto& attribute : attributes)
            {
                if (!attribute)
                    continue;
                std::string canonicalName = attribute->canonicalName;
                if (canonicalName.empty()) canonicalName = attribute->qualifiedName;
                if (canonicalName.empty()) canonicalName = canonicalBuiltinAttributeName(attribute->attribute);
                if (canonicalName.empty()) canonicalName = "std::attribute::Unknown";

                AttributeApplicationDescriptor descriptor;
                descriptor.canonicalName = std::move(canonicalName);
                descriptor.originParent = attribute->originParent;
                descriptor.selector = std::string{selector};
                descriptor.targetKind = targetKind;
                descriptor.origin = attributeOrigin(attribute->origin);
                descriptor.targetStableId = targetStableId;
                descriptor.targetType = targetType;
                descriptor.targetFunction = targetFunction;
                descriptor.parameterIndex = parameterIndex;
                descriptor.sourceOrder = static_cast<std::uint32_t>(attribute->processorOrder);
                descriptor.runtimeRetained = attribute->runtimeRetained;
                descriptor.stableId = stableHash(result_.module_.contract.stableKey + ":attribute:" +
                    std::to_string(targetStableId) + ":" + descriptor.canonicalName + ":" +
                    std::to_string(occurrence++));

                for (std::size_t index = 0; index < attribute->args.size(); ++index)
                {
                    AttributeArgumentDescriptor argument;
                    argument.sourceText = attribute->args[index].value;
                    if (index < attribute->argumentNames.size()) argument.name = attribute->argumentNames[index];
                    if (index < attribute->argumentUsedDefaults.size())
                        argument.usedDefault = attribute->argumentUsedDefaults[index];
                    if (index < attribute->typeArgs.size() && attribute->typeArgs[index])
                        if (const Ref<sema::Type> argumentType = attribute->typeArgs[index]->refType.Lock())
                            argument.type = mapType(argumentType, attribute->typeArgs[index].Get());
                    descriptor.arguments.push_back(std::move(argument));
                }
                for (std::size_t index = 0; index < attribute->processorBindings.size(); ++index)
                {
                    const auto& binding = attribute->processorBindings[index];
                    AttributeProcessorDescriptor processor;
                    processor.canonicalTypeName = binding.canonicalTypeName;
                    processor.hookName = binding.hookCppName;
                    processor.hookMode = binding.hookMode;
                    processor.phase = processorPhase(binding.phase);
                    if (const Ref<sema::Type> valueType = binding.hookValueType.Lock())
                        processor.valueType = mapType(valueType, attribute.Get());
                    processor.stableId = stableHash(std::to_string(descriptor.stableId) + ":processor:" +
                        std::to_string(index) + ":" + processor.canonicalTypeName + ":" + binding.phase);
                    descriptor.processors.push_back(std::move(processor));
                }
                ids.push_back(descriptor.stableId);
                result_.module_.contract.attributes.push_back(std::move(descriptor));
            }
            return ids;
        }

        FunctionId findExtensionFunction(const TypeId targetType, const std::string_view methodName)
        {
            for (std::size_t index = 0; index < declarations_.size(); ++index)
            {
                const DeclarationInfo& info = declarations_[index];
                if (!info.isExtension || !info.declaration)
                    continue;
                const Ref<sema::Type> semanticTarget = info.declaration->extensionTargetType.Lock();
                if (!semanticTarget || mapType(semanticTarget, info.declaration) != targetType)
                    continue;
                const std::string_view name = info.declaration->extensionMemberName.empty()
                    ? (info.declaration->name ? std::string_view{info.declaration->name->token.value} : std::string_view{})
                    : std::string_view{info.declaration->extensionMemberName};
                if (name == methodName)
                    return FunctionId{static_cast<FunctionId::ValueType>(index)};
            }
            return {};
        }

        void buildApplicationSystemAttributeReflection()
        {
            for (const TypeDeclarationInfo& declaration : typeDeclarations_)
            {
                if (!declaration.type)
                    continue;
                const TypeId typeId = mapType(declaration.type, declaration.declaration);
                const std::uint64_t stableTypeId = typeMetadataId(typeId);
                const MetadataTargetKind targetKind = declaration.role == "application"
                    ? MetadataTargetKind::Application
                    : (declaration.role == "system" ? MetadataTargetKind::System : MetadataTargetKind::Type);
                const std::vector<std::uint64_t> typeAttributes = declaration.attributes
                    ? appendAttributeApplications(*declaration.attributes, targetKind, stableTypeId, typeId)
                    : std::vector<std::uint64_t>{};
                auto reflected = std::ranges::find_if(result_.module_.contract.reflection,
                    [&](const ReflectionDescriptor& descriptor) { return descriptor.type == typeId; });
                if (reflected == result_.module_.contract.reflection.end())
                    continue;
                reflected->attributes = typeAttributes;

                const Type& type = result_.module_.types.get(typeId);
                for (const FieldLayout& field : type.fields)
                {
                    const std::uint64_t fieldId = stableHash(std::to_string(stableTypeId) + ":field:" + field.name);
                    std::vector<std::uint64_t> fieldAttributes;
                    const auto appendFieldAttributes = [&](const auto& members)
                    {
                        for (const auto& member : members)
                        {
                            const auto* variable = member.declaration
                                ? member.declaration->template as<VariableDeclaration>()
                                : nullptr;
                            if (!variable || !variable->name || variable->name->token.value != field.name)
                                continue;
                            const auto& attributes = member.attributes.empty() ? variable->attributes : member.attributes;
                            fieldAttributes = appendAttributeApplications(attributes,
                                MetadataTargetKind::Field, fieldId, typeId, {}, field.name);
                            break;
                        }
                    };
                    if (const auto* component = declaration.declaration->as<ComponentDeclaration>())
                        appendFieldAttributes(component->members);
                    else if (const auto* object = declaration.declaration->as<ObjectDeclaration>())
                        appendFieldAttributes(object->members);
                    reflected->fields.push_back(ReflectedFieldDescriptor{
                        .stableId = fieldId,
                        .name = field.name,
                        .type = field.type,
                        .visibility = field.visibility,
                        .isMutable = field.isMutable,
                        .attributes = std::move(fieldAttributes)
                    });
                }
                for (const MethodLayout& method : type.methods)
                {
                    const Function* function = findFunction(method.function);
                    reflected->methods.push_back(ReflectedMethodDescriptor{
                        .stableId = functionMetadataId(method.function),
                        .name = method.name,
                        .function = method.function,
                        .returnType = method.returnType,
                        .parameterTypes = method.parameterTypes,
                        .slot = method.slot,
                        .isAsync = function && function->isAsync
                    });
                }
                const auto appendCases = [&](const auto& members)
                {
                    for (const auto& member : members)
                    {
                        if (!member.name) continue;
                        const std::string& name = member.name->token.value;
                        const std::uint64_t caseId = stableHash(std::to_string(stableTypeId) + ":case:" + name);
                        reflected->cases.push_back(ReflectedCaseDescriptor{
                            .stableId = caseId,
                            .name = name,
                            .attributes = appendAttributeApplications(member.attributes,
                                MetadataTargetKind::EnumCase, caseId, typeId, {}, name)
                        });
                    }
                };
                if (const auto* enumeration = declaration.declaration->as<EnumDeclaration>())
                    appendCases(enumeration->members);
                else if (const auto* flagset = declaration.declaration->as<FlagsetDeclaration>())
                    appendCases(flagset->members);
            }

            for (std::size_t index = 0; index < declarations_.size(); ++index)
            {
                const DeclarationInfo& info = declarations_[index];
                if (!info.declaration)
                    continue;
                const FunctionId functionId{static_cast<FunctionId::ValueType>(index)};
                const Function* function = findFunction(functionId);
                if (!function)
                    continue;
                const TypeId targetType = info.ownerType
                    ? mapType(info.ownerType, info.declaration)
                    : (info.isExtension && info.declaration->extensionTargetType.Lock()
                        ? mapType(info.declaration->extensionTargetType.Lock(), info.declaration)
                        : TypeId{});
                const auto ids = appendAttributeApplications(info.declaration->attributes,
                    declarationTargetKind(*info.declaration, function->isMethod),
                    functionMetadataId(functionId), targetType, functionId,
                    info.declaration->extensionMemberName);
                for (std::size_t parameterIndex = 0;
                     parameterIndex < info.declaration->parameters.size(); ++parameterIndex)
                {
                    const wio::Parameter& parameter = info.declaration->parameters[parameterIndex];
                    if (parameter.attributes.empty()) continue;
                    const std::string parameterName = parameter.name
                        ? parameter.name->token.value
                        : std::string{"parameter." + std::to_string(parameterIndex)};
                    const std::uint64_t parameterId = stableHash(std::to_string(functionMetadataId(functionId)) +
                        ":parameter:" + std::to_string(parameterIndex) + ":" + parameterName);
                    appendAttributeApplications(parameter.attributes, MetadataTargetKind::Parameter,
                        parameterId, targetType, functionId, parameterName,
                        static_cast<std::uint32_t>(parameterIndex));
                }
                if (!ids.empty() && targetType)
                {
                    auto reflected = std::ranges::find_if(result_.module_.contract.reflection,
                        [&](const ReflectionDescriptor& descriptor) { return descriptor.type == targetType; });
                    if (reflected != result_.module_.contract.reflection.end())
                    {
                        auto method = std::ranges::find_if(reflected->methods,
                            [&](const ReflectedMethodDescriptor& descriptor) { return descriptor.function == functionId; });
                        if (method != reflected->methods.end()) method->attributes = ids;
                    }
                }
            }

            for (const TypeDeclarationInfo& declaration : typeDeclarations_)
            {
                if (declaration.role != "system" || !declaration.type)
                    continue;
                const TypeId type = mapType(declaration.type, declaration.declaration);
                const Type* descriptor = result_.module_.types.tryGet(type);
                if (!descriptor)
                    continue;
                result_.module_.contract.systems.push_back(SystemDescriptor{
                    .stableId = stableHash(result_.module_.contract.stableKey + ":system:" + descriptor->name),
                    .logicalName = descriptor->name,
                    .type = type,
                    .start = findExtensionFunction(type, "Start"),
                    .update = findExtensionFunction(type, "Update"),
                    .close = findExtensionFunction(type, "Close")
                });
            }

            for (std::size_t index = 0; index < declarations_.size(); ++index)
            {
                const FunctionDeclaration* entry = declarations_[index].declaration;
                if (!entry || !entry->isApplicationEntry)
                    continue;
                const auto applicationType = std::ranges::find_if(typeDeclarations_, [&](const TypeDeclarationInfo& type)
                {
                    const auto* component = type.declaration ? type.declaration->as<ComponentDeclaration>() : nullptr;
                    return type.role == "application" && component && component->name &&
                        component->name->token.value == entry->applicationName;
                });
                if (applicationType == typeDeclarations_.end() || !applicationType->type)
                    continue;
                ApplicationDescriptor application;
                application.logicalName = entry->applicationName;
                application.type = mapType(applicationType->type, applicationType->declaration);
                application.stableId = stableHash(result_.module_.contract.stableKey + ":application:" + application.logicalName);
                application.entry = FunctionId{static_cast<FunctionId::ValueType>(index)};
                application.start = findExtensionFunction(application.type, "Start");
                application.update = findExtensionFunction(application.type, "Update");
                application.close = findExtensionFunction(application.type, "Close");
                application.exit = findExtensionFunction(application.type, "Exit");
                const Type& appType = result_.module_.types.get(application.type);
                for (const FieldLayout& field : appType.fields)
                    if (std::ranges::any_of(result_.module_.contract.systems,
                        [&](const SystemDescriptor& system) { return system.type == field.type; }))
                        application.systems.push_back(field.type);

                for (const ApplicationStageMetadata& sourceStage : entry->applicationStages)
                {
                    ApplicationStageDescriptor stage;
                    stage.name = sourceStage.name;
                    stage.after = sourceStage.after;
                    stage.fixedHz = sourceStage.fixedHz;
                    stage.order = sourceStage.order;
                    stage.kind = sourceStage.fixed ? ApplicationStageKind::Fixed : ApplicationStageKind::Variable;
                    stage.affinity = sourceStage.mainThread ? ApplicationAffinity::Main : ApplicationAffinity::Inherit;
                    stage.legacyExplicit = sourceStage.legacyExplicit;
                    stage.stableId = stableHash(std::to_string(application.stableId) + ":stage:" + stage.name);
                    for (const ApplicationStageRunMetadata& sourceRun : sourceStage.runs)
                    {
                        ApplicationStageRun run;
                        run.targetName = sourceRun.target;
                        run.methodName = sourceRun.method;
                        run.applicationTarget = sourceRun.target == "self";
                        run.acceptsDelta = sourceRun.acceptsDelta;
                        if (run.applicationTarget)
                            run.targetType = application.type;
                        else
                        {
                            const auto field = std::ranges::find_if(appType.fields,
                                [&](const FieldLayout& candidate) { return candidate.name == sourceRun.target; });
                            if (field != appType.fields.end()) run.targetType = field->type;
                        }
                        run.function = findExtensionFunction(run.targetType, run.methodName);
                        const Function* function = findFunction(run.function);
                        std::size_t parameterIndex = 1u + (run.acceptsDelta ? 1u : 0u);
                        for (const std::string& resourceName : sourceRun.resourceNames)
                        {
                            ApplicationResourceBinding resource{.name = resourceName};
                            const auto field = std::ranges::find_if(appType.fields,
                                [&](const FieldLayout& candidate) { return candidate.name == resourceName; });
                            if (field != appType.fields.end()) resource.type = field->type;
                            if (function && parameterIndex < function->parameters.size())
                            {
                                const Type* parameter = result_.module_.types.tryGet(function->parameters[parameterIndex].type);
                                resource.access = parameter && parameter->kind == TypeKind::Reference && parameter->isMutable
                                    ? ResourceAccess::Write : ResourceAccess::Read;
                            }
                            ++parameterIndex;
                            run.resources.push_back(std::move(resource));
                        }
                        stage.runs.push_back(std::move(run));
                    }
                    application.stages.push_back(std::move(stage));
                }
                result_.module_.contract.application = std::move(application);
                break;
            }
        }

        void collectFunctions(const std::vector<NodePtr<Statement>>& statements)
        {
            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;
                if (const auto* function = statement->as<FunctionDeclaration>())
                {
                    collectFunction(function);
                    continue;
                }
                if (const auto* interfaceDeclaration = statement->as<InterfaceDeclaration>())
                {
                    const Ref<sema::Type> owner = declaredOwnerType(interfaceDeclaration->name);
                    for (const auto& method : interfaceDeclaration->methods)
                        collectFunction(method.Get(), owner);
                    continue;
                }
                if (const auto* component = statement->as<ComponentDeclaration>())
                {
                    const Ref<sema::Type> owner = declaredOwnerType(component->name);
                    for (const ComponentMember& member : component->members)
                        collectFunction(member.declaration ? member.declaration->as<FunctionDeclaration>() : nullptr, owner);
                    continue;
                }
                if (const auto* object = statement->as<ObjectDeclaration>())
                {
                    const Ref<sema::Type> owner = declaredOwnerType(object->name);
                    for (const ObjectMember& member : object->members)
                        collectFunction(member.declaration ? member.declaration->as<FunctionDeclaration>() : nullptr, owner);
                    continue;
                }
                if (const auto* extension = statement->as<ExtensionDeclaration>())
                {
                    for (const ExtensionMember& member : extension->members)
                        collectFunction(member.method.Get(), nullptr, true);
                    continue;
                }
                if (const auto* group = statement->as<DeclarationGroup>())
                {
                    collectFunctions(group->declarations);
                    continue;
                }
                if (const auto* realm = statement->as<RealmDeclaration>())
                    collectFunctions(realm->statements);
            }
        }

        static bool isLifecycleMethod(const std::string_view name)
        {
            return name == "OnConstruct" || name == "OnDestruct";
        }

        static bool sameMethodSignature(const MethodLayout& left, const MethodLayout& right)
        {
            return left.name == right.name && left.parameterTypes == right.parameterTypes &&
                left.returnType == right.returnType;
        }

        void appendMethodLayout(
            const Ref<sema::Symbol>& methodSymbol,
            std::vector<MethodLayout>& methods,
            const ASTNode* source)
        {
            if (!methodSymbol)
                return;
            if (methodSymbol->kind == sema::SymbolKind::FunctionGroup)
            {
                for (const Ref<sema::Symbol>& overload : methodSymbol->overloads)
                    appendMethodLayout(overload, methods, source);
                return;
            }
            if (methodSymbol->kind != sema::SymbolKind::Function || isLifecycleMethod(methodSymbol->name))
                return;
            const auto function = functionsBySymbol_.find(methodSymbol.Get());
            const auto functionType = methodSymbol->type && methodSymbol->type->kind() == sema::TypeKind::Function
                ? methodSymbol->type.AsFast<sema::FunctionType>()
                : nullptr;
            if (function == functionsBySymbol_.end() || !functionType)
                return;

            MethodLayout layout{
                .name = methodSymbol->name,
                .returnType = mapType(functionType->returnType, source),
                .function = function->second,
                .receiverMutable = true,
                .isAbstract = !declarationsBySymbol_.contains(methodSymbol.Get()) ||
                    declarationsBySymbol_.at(methodSymbol.Get())->body == nullptr
            };
            for (const Ref<sema::Type>& parameterType : functionType->paramTypes)
                layout.parameterTypes.push_back(mapType(parameterType, source));

            const auto overridden = std::ranges::find_if(methods, [&](const MethodLayout& inherited)
            {
                if (sameMethodSignature(inherited, layout))
                    return true;
                return std::ranges::any_of(methodSymbol->overriddenSymbols, [&](const WeakRef<sema::Symbol>& candidate)
                {
                    const Ref<sema::Symbol> symbol = candidate.Lock();
                    const auto implementation = symbol
                        ? functionsBySymbol_.find(symbol.Get())
                        : functionsBySymbol_.end();
                    return implementation != functionsBySymbol_.end() &&
                        inherited.function == implementation->second;
                });
            });
            if (overridden != methods.end())
            {
                layout.slot = overridden->slot;
                *overridden = std::move(layout);
                return;
            }

            layout.slot = static_cast<std::uint32_t>(methods.size());
            methods.push_back(std::move(layout));
        }

        TypeId mapType(Ref<sema::Type> type, const ASTNode* source)
        {
            if (!type)
            {
                report("WIR2001", "Semantic type is missing while building Typed WIR.", source);
                return {};
            }
            if (const auto found = typesBySemanticType_.find(type.Get()); found != typesBySemanticType_.end())
                return found->second;

            while (type && type->kind() == sema::TypeKind::Alias)
                type = type.AsFast<sema::AliasType>()->aliasedType;
            if (!type)
                return {};
            if (const auto found = typesBySemanticType_.find(type.Get()); found != typesBySemanticType_.end())
                return found->second;

            Type wirType;
            switch (type->kind())
            {
            case sema::TypeKind::Primitive:
            {
                const std::string& name = type.AsFast<sema::PrimitiveType>()->name;
                static const std::unordered_map<std::string, TypeKind> primitiveKinds{
                    {"void", TypeKind::Void}, {"bool", TypeKind::Bool},
                    {"i8", TypeKind::I8}, {"i16", TypeKind::I16}, {"i32", TypeKind::I32},
                    {"i64", TypeKind::I64}, {"isize", TypeKind::ISize},
                    {"u8", TypeKind::U8}, {"u16", TypeKind::U16}, {"u32", TypeKind::U32},
                    {"u64", TypeKind::U64}, {"usize", TypeKind::USize},
                    {"f32", TypeKind::F32}, {"f64", TypeKind::F64},
                    {"byte", TypeKind::Byte}, {"char", TypeKind::Char},
                    {"string", TypeKind::String}, {"text", TypeKind::Text},
                    {"any", TypeKind::Any}, {"opaque", TypeKind::Opaque}
                };
                const auto found = primitiveKinds.find(name);
                if (found == primitiveKinds.end())
                {
                    report("WIR2002", "Unsupported semantic primitive type '" + name + "' while mapping " +
                        (source ? getKindNameStr(source->kind()) : std::string{"an unknown source"}) + ".", source);
                    return {};
                }
                wirType.kind = found->second;
                if (wirType.kind == TypeKind::String || wirType.kind == TypeKind::Text ||
                    wirType.kind == TypeKind::Any)
                {
                    wirType.ownership = OwnershipModel::OwnedValue;
                    wirType.cleanup = CleanupKind::DestroyValue;
                }
                break;
            }
            case sema::TypeKind::GenericParameter:
                wirType.kind = TypeKind::GenericParameter;
                wirType.name = type.AsFast<sema::GenericParameterType>()->name;
                wirType.ownership = OwnershipModel::Generic;
                wirType.cleanup = CleanupKind::DestroyValue;
                break;
            case sema::TypeKind::Reference:
            {
                const auto reference = type.AsFast<sema::ReferenceType>();
                wirType.kind = TypeKind::Reference;
                wirType.arguments.push_back(mapType(reference->referredType, source));
                wirType.isMutable = reference->isMutable;
                wirType.ownership = OwnershipModel::Borrowed;
                break;
            }
            case sema::TypeKind::Nullable:
            {
                wirType.kind = TypeKind::Nullable;
                wirType.arguments.push_back(mapType(type.AsFast<sema::NullableType>()->valueType, source));
                if (const Type* valueType = result_.module_.types.tryGet(wirType.arguments.front());
                    valueType && requiresCleanup(*valueType))
                {
                    wirType.ownership = OwnershipModel::OwnedValue;
                    wirType.cleanup = CleanupKind::DestroyValue;
                }
                break;
            }
            case sema::TypeKind::Null:
                return mapType(type.AsFast<sema::NullType>()->transformedType, source);
            case sema::TypeKind::Array:
            {
                const auto array = type.AsFast<sema::ArrayType>();
                wirType.kind = TypeKind::Array;
                wirType.arguments.push_back(mapType(array->elementType, source));
                if (array->arrayKind == sema::ArrayType::ArrayKind::Static ||
                    array->arrayKind == sema::ArrayType::ArrayKind::Literal)
                {
                    wirType.staticExtent = array->size;
                }
                wirType.ownership = OwnershipModel::OwnedValue;
                wirType.cleanup = CleanupKind::DestroyValue;
                break;
            }
            case sema::TypeKind::Dictionary:
            {
                const auto dictionary = type.AsFast<sema::DictionaryType>();
                wirType.kind = TypeKind::Dictionary;
                wirType.name = dictionary->isOrdered ? "ordered" : "unordered";
                wirType.arguments = {
                    mapType(dictionary->keyType, source),
                    mapType(dictionary->valueType, source)
                };
                wirType.ownership = OwnershipModel::OwnedValue;
                wirType.cleanup = CleanupKind::DestroyValue;
                break;
            }
            case sema::TypeKind::Function:
            {
                const auto function = type.AsFast<sema::FunctionType>();
                wirType.kind = TypeKind::Function;
                for (const auto& parameterType : function->paramTypes)
                    wirType.arguments.push_back(mapType(parameterType, source));
                wirType.arguments.push_back(mapType(function->returnType, source));
                wirType.ownership = OwnershipModel::ReferenceCounted;
                wirType.cleanup = CleanupKind::ReleaseReference;
                break;
            }
            case sema::TypeKind::AsyncTask:
                wirType.kind = TypeKind::AsyncTask;
                wirType.arguments.push_back(mapType(type.AsFast<sema::AsyncTaskType>()->valueType, source));
                wirType.ownership = OwnershipModel::ReferenceCounted;
                wirType.cleanup = CleanupKind::ReleaseReference;
                break;
            case sema::TypeKind::Struct:
            {
                const auto structure = type.AsFast<sema::StructType>();
                wirType.kind = TypeKind::Named;
                wirType.name = structure->scopePath.empty()
                    ? structure->name
                    : structure->scopePath + "::" + structure->name;
                for (const auto& argument : structure->genericArguments)
                    wirType.arguments.push_back(mapType(argument, source));
                wirType.nominalKind = structure->isFlagset
                    ? NominalKind::Flagset
                    : structure->isEnum
                        ? NominalKind::Enum
                        : structure->isInterface
                            ? NominalKind::Interface
                            : structure->isObject
                                ? NominalKind::Object
                                : NominalKind::Component;
                wirType.nominalRepresentation = structure->isNativePodComponent
                    ? NominalRepresentation::NativePod
                    : NominalRepresentation::Wio;
                if (structure->isNativePodComponent)
                {
                    wirType.nativeBinding = NativeTypeBinding{
                        .cppName = structure->nativeCppName.empty() ? structure->name : structure->nativeCppName,
                        .header = structure->nativeCppHeader
                    };
                }
                if (wirType.nominalKind == NominalKind::Object ||
                    wirType.nominalKind == NominalKind::Interface)
                {
                    wirType.ownership = OwnershipModel::ReferenceCounted;
                    wirType.cleanup = CleanupKind::ReleaseReference;
                }
                else if (wirType.nominalKind == NominalKind::Component)
                {
                    wirType.ownership = OwnershipModel::OwnedValue;
                }
                if (structure->name == "Tuple")
                    wirType.nominalValueModel = NominalValueModel::Tuple;
                else if (structure->name == "Span")
                    wirType.nominalValueModel = NominalValueModel::Span;
                else if (structure->name == "Option")
                    wirType.nominalValueModel = NominalValueModel::Option;
                else if (structure->name == "Result")
                    wirType.nominalValueModel = NominalValueModel::Result;
                for (const TypeId argument : wirType.arguments)
                {
                    if (!argument)
                        return {};
                }

                const TypeId id = result_.module_.types.internNominal(std::move(wirType));
                typesBySemanticType_[type.Get()] = id;

                std::vector<TypeId> baseTypes;
                baseTypes.reserve(structure->baseTypes.size());
                for (const Ref<sema::Type>& baseType : structure->baseTypes)
                    baseTypes.push_back(mapType(baseType, source));

                std::vector<FieldLayout> fields;
                fields.reserve(structure->fieldNames.size());
                const Ref<sema::Scope> structScope = structure->structScope.Lock();
                for (std::size_t index = 0;
                     index < structure->fieldNames.size() && index < structure->fieldTypes.size();
                     ++index)
                {
                    const std::string& fieldName = structure->fieldNames[index];
                    const Ref<sema::Symbol> fieldSymbol = structScope
                        ? structScope->resolveLocally(fieldName)
                        : nullptr;
                    FieldVisibility visibility = FieldVisibility::Private;
                    if (fieldSymbol && fieldSymbol->flags.get_isPublic())
                        visibility = FieldVisibility::Public;
                    else if (fieldSymbol && fieldSymbol->flags.get_isProtected())
                        visibility = FieldVisibility::Protected;
                    fields.push_back(FieldLayout{
                        .name = fieldName,
                        .type = mapType(structure->fieldTypes[index], source),
                        .isMutable = !fieldSymbol || !fieldSymbol->flags.get_isReadOnly(),
                        .visibility = visibility
                    });
                }

                std::vector<MethodLayout> methods;
                for (const TypeId baseTypeId : baseTypes)
                {
                    const Type* baseType = result_.module_.types.tryGet(baseTypeId);
                    if (!baseType)
                        continue;
                    for (const MethodLayout& inherited : baseType->methods)
                    {
                        if (std::ranges::none_of(methods, [&](const MethodLayout& existing)
                            { return sameMethodSignature(existing, inherited); }))
                        {
                            MethodLayout copy = inherited;
                            copy.slot = static_cast<std::uint32_t>(methods.size());
                            methods.push_back(std::move(copy));
                        }
                    }
                }
                if (structScope)
                {
                    for (const auto& [name, member] : structScope->getSymbols())
                    {
                        WIO_UNUSED(name);
                        appendMethodLayout(member, methods, source);
                    }
                }

                Type& storedType = result_.module_.types.getMutable(id);
                storedType.baseTypes = std::move(baseTypes);
                storedType.fields = std::move(fields);
                storedType.methods = std::move(methods);
                storedType.hasConstructor = structScope && structScope->resolveLocally("OnConstruct");
                storedType.hasDestructor = structScope && structScope->resolveLocally("OnDestruct");
                if (storedType.nominalKind == NominalKind::Component)
                {
                    const bool managedField = std::ranges::any_of(storedType.fields, [&](const FieldLayout& field)
                    {
                        const Type* fieldType = result_.module_.types.tryGet(field.type);
                        return fieldType && requiresCleanup(*fieldType);
                    });
                    const bool managedArgument = std::ranges::any_of(storedType.arguments, [&](const TypeId argument)
                    {
                        const Type* argumentType = result_.module_.types.tryGet(argument);
                        return argumentType && requiresCleanup(*argumentType);
                    });
                    if (storedType.hasDestructor || managedField || managedArgument)
                        storedType.cleanup = CleanupKind::DestroyValue;
                }
                return id;
            }
            default:
                report("WIR2003", "Semantic type '" + type->toString() + "' is not representable in the initial Typed WIR slice.", source);
                return {};
            }

            for (const TypeId argument : wirType.arguments)
            {
                if (!argument)
                    return {};
            }
            const TypeId id = result_.module_.types.intern(std::move(wirType));
            typesBySemanticType_[type.Get()] = id;
            return id;
        }

        Ref<sema::Symbol> resolveCallableSymbol(
            Ref<sema::Symbol> symbol,
            const Ref<sema::Type>& selectedType = nullptr) const
        {
            if (!symbol)
                return nullptr;
            if (symbol->kind == sema::SymbolKind::FunctionGroup)
            {
                for (const Ref<sema::Symbol>& overload : symbol->overloads)
                {
                    if (overload && (!selectedType || sema::Type::matchTypes(overload->type, selectedType)))
                    {
                        symbol = overload;
                        break;
                    }
                }
            }
            if (symbol->flags.get_isExtension() && symbol->extensionImplementation)
                symbol = symbol->extensionImplementation;
            return symbol;
        }

        void bindGenericArguments(
            Ref<sema::Type> pattern,
            Ref<sema::Type> concrete,
            std::unordered_map<std::string, Ref<sema::Type>>& bindings) const
        {
            while (pattern && pattern->kind() == sema::TypeKind::Alias)
                pattern = pattern.AsFast<sema::AliasType>()->aliasedType;
            while (concrete && concrete->kind() == sema::TypeKind::Alias)
                concrete = concrete.AsFast<sema::AliasType>()->aliasedType;
            if (!pattern || !concrete)
                return;
            if (pattern->kind() == sema::TypeKind::GenericParameter)
            {
                bindings.try_emplace(pattern.AsFast<sema::GenericParameterType>()->name, concrete);
                return;
            }
            if (pattern->kind() != concrete->kind())
                return;
            switch (pattern->kind())
            {
            case sema::TypeKind::Reference:
                bindGenericArguments(
                    pattern.AsFast<sema::ReferenceType>()->referredType,
                    concrete.AsFast<sema::ReferenceType>()->referredType,
                    bindings);
                break;
            case sema::TypeKind::Nullable:
                bindGenericArguments(
                    pattern.AsFast<sema::NullableType>()->valueType,
                    concrete.AsFast<sema::NullableType>()->valueType,
                    bindings);
                break;
            case sema::TypeKind::Array:
                bindGenericArguments(
                    pattern.AsFast<sema::ArrayType>()->elementType,
                    concrete.AsFast<sema::ArrayType>()->elementType,
                    bindings);
                break;
            case sema::TypeKind::Dictionary:
            {
                const auto patternDictionary = pattern.AsFast<sema::DictionaryType>();
                const auto concreteDictionary = concrete.AsFast<sema::DictionaryType>();
                bindGenericArguments(patternDictionary->keyType, concreteDictionary->keyType, bindings);
                bindGenericArguments(patternDictionary->valueType, concreteDictionary->valueType, bindings);
                break;
            }
            case sema::TypeKind::Function:
            {
                const auto patternFunction = pattern.AsFast<sema::FunctionType>();
                const auto concreteFunction = concrete.AsFast<sema::FunctionType>();
                for (std::size_t index = 0;
                     index < patternFunction->paramTypes.size() && index < concreteFunction->paramTypes.size();
                     ++index)
                {
                    bindGenericArguments(patternFunction->paramTypes[index], concreteFunction->paramTypes[index], bindings);
                }
                bindGenericArguments(patternFunction->returnType, concreteFunction->returnType, bindings);
                break;
            }
            case sema::TypeKind::Struct:
            {
                const auto patternStructure = pattern.AsFast<sema::StructType>();
                const auto concreteStructure = concrete.AsFast<sema::StructType>();
                for (std::size_t index = 0;
                     index < patternStructure->genericArguments.size() && index < concreteStructure->genericArguments.size();
                     ++index)
                {
                    bindGenericArguments(
                        patternStructure->genericArguments[index],
                        concreteStructure->genericArguments[index],
                        bindings);
                }
                break;
            }
            default:
                break;
            }
        }

        std::vector<TypeId> genericArguments(
            const FunctionCallExpression& call,
            const Ref<sema::Symbol>& symbol,
            const Ref<sema::Type>& selectedCallableType)
        {
            std::unordered_map<std::string, Ref<sema::Type>> bindings;
            for (std::size_t index = 0; index < call.explicitTypeArguments.size() &&
                 symbol && index < symbol->genericParameterNames.size(); ++index)
            {
                if (const Ref<sema::Type> argument = call.explicitTypeArguments[index]
                    ? call.explicitTypeArguments[index]->refType.Lock()
                    : nullptr)
                {
                    bindings[symbol->genericParameterNames[index]] = argument;
                }
            }
            for (std::size_t index = 0; index < call.resolvedGenericArguments.size(); ++index)
            {
                if (const Ref<sema::Type> argument = call.resolvedGenericArguments[index].Lock();
                    symbol && index < symbol->genericParameterNames.size())
                {
                    bindings.try_emplace(symbol->genericParameterNames[index], argument);
                }
            }
            if (symbol)
                bindGenericArguments(symbol->type, selectedCallableType, bindings);

            std::vector<TypeId> arguments;
            if (symbol)
            {
                for (const std::string& name : symbol->genericParameterNames)
                {
                    const auto found = bindings.find(name);
                    if (found != bindings.end())
                        arguments.push_back(mapType(found->second, &call));
                }
            }
            return arguments;
        }

        static std::string specializationKey(
            const FunctionId callee,
            const std::vector<TypeId>& genericArguments,
            const std::vector<TypeId>& signature,
            const TypeId resultType)
        {
            std::ostringstream key;
            key << 'f' << callee.value() << '<';
            for (std::size_t index = 0; index < genericArguments.size(); ++index)
            {
                if (index != 0)
                    key << ',';
                key << 't' << genericArguments[index].value();
            }
            key << ">(";
            for (std::size_t index = 0; index < signature.size(); ++index)
            {
                if (index != 0)
                    key << ',';
                key << 't' << signature[index].value();
            }
            key << ")->t" << resultType.value();
            return key.str();
        }

        FunctionId buildLambdaFunction(
            const LambdaExpression& lambda,
            const std::vector<CaptureLayout>& captures,
            const TypeId selfCaptureType)
        {
            if (const auto found = lambdaFunctions_.find(&lambda); found != lambdaFunctions_.end())
                return found->second;

            const FunctionId id{nextFunctionId_++};
            lambdaFunctions_[&lambda] = id;
            const Ref<sema::Type> semanticCallable = lambda.refType.Lock();
            const auto callable = semanticCallable && semanticCallable->kind() == sema::TypeKind::Function
                ? semanticCallable.AsFast<sema::FunctionType>()
                : nullptr;

            Function function;
            function.id = id;
            function.name = "$lambda." + std::to_string(id.value());
            function.callableType = mapType(semanticCallable, &lambda);
            function.returnType = callable ? mapType(callable->returnType, &lambda) : TypeId{};
            function.captureParameterCount = static_cast<std::uint32_t>(captures.size());
            function.captures = captures;
            function.source = SourceSpan::at(lambda.location());
            function.isClosureBody = true;

            FunctionState state{.function = &function};
            state.blockIndex = createBlock(state, "entry", SourceSpan::at(lambda.location()));

            std::size_t captureIndex = 0;
            for (const WeakRef<sema::Symbol>& weakSymbol : lambda.capturedSymbols)
            {
                const Ref<sema::Symbol> symbol = weakSymbol.Lock();
                if (!symbol || captureIndex >= captures.size())
                    continue;
                const CaptureLayout& capture = captures[captureIndex++];
                const TypeId environmentPlaceType = referenceType(capture.type, true);
                const ValueId parameter{state.nextValue++};
                function.parameters.push_back(Parameter{
                    .id = parameter,
                    .name = "$capture." + capture.name,
                    .type = environmentPlaceType,
                    .ownership = ValueOwnership::Borrowed,
                    .borrowLifetime = BorrowLifetime::Caller,
                    .source = SourceSpan::at(lambda.location())
                });
                rememberOwnership(state, parameter, ValueOwnership::Borrowed);
                state.places[symbol.Get()] = parameter;
            }
            if (lambda.capturesSelf)
            {
                const ValueId parameter{state.nextValue++};
                function.parameters.push_back(Parameter{
                    .id = parameter,
                    .name = "$capture.self",
                    .type = selfCaptureType,
                    .ownership = ValueOwnership::Borrowed,
                    .borrowLifetime = BorrowLifetime::Caller,
                    .source = SourceSpan::at(lambda.location())
                });
                rememberOwnership(state, parameter, ValueOwnership::Borrowed);
                state.selfValue = parameter;
                state.selfType = selfCaptureType;
            }

            for (std::size_t index = 0; index < lambda.parameters.size(); ++index)
            {
                const wio::Parameter& parameter = lambda.parameters[index];
                const Ref<sema::Symbol> symbol = parameter.name
                    ? parameter.name->referencedSymbol.Lock()
                    : nullptr;
                if (!symbol || !callable || index >= callable->paramTypes.size())
                {
                    report("WIR2340", "Lambda parameter is missing its resolved callable type.", parameter.name.Get());
                    continue;
                }
                const TypeId parameterType = mapType(callable->paramTypes[index], parameter.name.Get());
                const ValueId value{state.nextValue++};
                function.parameters.push_back(Parameter{
                    .id = value,
                    .name = symbol->name,
                    .type = parameterType,
                    .ownership = ownershipForType(parameterType),
                    .borrowLifetime = ownershipForType(parameterType) == ValueOwnership::Borrowed
                        ? BorrowLifetime::Caller : BorrowLifetime::None,
                    .source = SourceSpan::at(parameter.name->location())
                });
                rememberOwnership(state, value, ownershipForType(parameterType));
                const Type* parameterTypeInfo = result_.module_.types.tryGet(parameterType);
                if (parameterTypeInfo && parameterTypeInfo->kind == TypeKind::Reference)
                {
                    state.values[symbol.Get()] = value;
                    state.valueOrder.push_back(symbol.Get());
                }
                else
                {
                    const ValueId place{state.nextValue++};
                    currentBlock(state).instructions.push_back(Instruction{
                        .opcode = Opcode::LocalPlace,
                        .result = place,
                        .resultType = referenceType(parameterType, symbol->flags.get_isMutable()),
                        .selector = symbol->name,
                        .source = SourceSpan::at(parameter.name->location())
                    });
                    currentBlock(state).instructions.push_back(Instruction{
                        .opcode = Opcode::PlaceInit,
                        .operands = {place, value},
                        .source = SourceSpan::at(parameter.name->location())
                    });
                    state.places[symbol.Get()] = place;
                    state.placeOrder.push_back(symbol.Get());
                }
            }

            std::vector<LoopContext> savedLoops = std::move(loopContexts_);
            loopContexts_.clear();
            if (const auto* expressionBody = lambda.body ? lambda.body->as<ExpressionStatement>() : nullptr)
            {
                Instruction resultInstruction{.opcode = Opcode::Return, .source = SourceSpan::at(lambda.location())};
                const Type* returnType = result_.module_.types.tryGet(function.returnType);
                if (returnType && returnType->kind != TypeKind::Void)
                {
                    const ValueId value = buildExpressionAs(expressionBody->expression, function.returnType, state);
                    if (value)
                        resultInstruction.operands.push_back(value);
                }
                emitDropsFrom(0, state, &lambda);
                currentBlock(state).instructions.push_back(std::move(resultInstruction));
            }
            else
            {
                buildStatement(lambda.body, state);
                if (!blockIsTerminated(state))
                {
                    const Type* returnType = result_.module_.types.tryGet(function.returnType);
                    if (returnType && returnType->kind == TypeKind::Void)
                    {
                        emitDropsFrom(0, state, &lambda);
                        currentBlock(state).instructions.push_back(Instruction{.opcode = Opcode::Return});
                    }
                    else
                    {
                        report("WIR2341", "Non-void lambda does not end with a return.", &lambda);
                        currentBlock(state).instructions.push_back(Instruction{.opcode = Opcode::Unreachable});
                    }
                }
            }
            loopContexts_ = std::move(savedLoops);
            result_.module_.functions.push_back(std::move(function));
            return id;
        }

        void bindLifecycleFunction(const FunctionDeclaration& declaration, const FunctionId function)
        {
            ModuleLifecycle& lifecycle = result_.module_.contract.lifecycle;
            if (hasBuiltinAttribute(declaration.attributes, Attribute::ModuleApiVersion)) lifecycle.apiVersion = function;
            if (hasBuiltinAttribute(declaration.attributes, Attribute::ModuleLoad)) lifecycle.load = function;
            if (hasBuiltinAttribute(declaration.attributes, Attribute::ModuleUpdate)) lifecycle.update = function;
            if (hasBuiltinAttribute(declaration.attributes, Attribute::ModuleUnload)) lifecycle.unload = function;
            if (hasBuiltinAttribute(declaration.attributes, Attribute::ModuleSaveState)) lifecycle.saveState = function;
            if (hasBuiltinAttribute(declaration.attributes, Attribute::ModuleRestoreState)) lifecycle.restoreState = function;
        }

        void appendFunctionExport(
            const FunctionDeclaration& declaration,
            const Ref<sema::Symbol>& symbol,
            const Function& function,
            std::vector<TypeId> genericArguments)
        {
            ModuleExport entry;
            entry.function = function.id;
            entry.kind = genericArguments.empty()
                ? ModuleExportKind::Function
                : ModuleExportKind::GenericFunctionSpecialization;
            entry.logicalName = symbol->name;
            if (!genericArguments.empty())
            {
                entry.logicalName += "<";
                for (std::size_t index = 0; index < genericArguments.size(); ++index)
                {
                    if (index > 0) entry.logicalName += ",";
                    entry.logicalName += typeStableKey(genericArguments[index]);
                }
                entry.logicalName += ">";
            }
            entry.symbolName = attributeValue(declaration.attributes, Attribute::CppName);
            if (entry.symbolName.empty())
                entry.symbolName = entry.logicalName;
            if (hasBuiltinAttribute(declaration.attributes, Attribute::Command))
            {
                entry.role = ModuleExportRole::Command;
                entry.roleName = attributeValue(declaration.attributes, Attribute::Command);
            }
            else if (hasBuiltinAttribute(declaration.attributes, Attribute::Event))
            {
                entry.role = ModuleExportRole::EventHook;
                entry.roleName = attributeValue(declaration.attributes, Attribute::Event);
            }
            if (entry.role != ModuleExportRole::Ordinary && entry.roleName.empty())
                entry.roleName = entry.logicalName;
            const std::size_t hidden = function.captureParameterCount + (function.isMethod ? 1u : 0u);
            for (std::size_t index = hidden; index < function.parameters.size(); ++index)
                entry.parameterTypes.push_back(substituteGenericType(
                    function.parameters[index].type, function.genericParameters, genericArguments));
            entry.returnType = substituteGenericType(
                function.returnType, function.genericParameters, genericArguments);
            entry.genericArguments = std::move(genericArguments);
            entry.callTableSlot = static_cast<std::uint32_t>(result_.module_.contract.exports.size());
            entry.isAsync = function.isAsync;
            entry.stableKey = exportStableKey(function, entry.logicalName, entry.genericArguments);
            entry.stableId = stableHash(entry.stableKey);
            result_.module_.contract.callTable.entries.push_back(entry.stableId);
            result_.module_.contract.exports.push_back(std::move(entry));
        }

        void buildFunctionContract(
            const FunctionDeclaration& declaration,
            const Ref<sema::Symbol>& symbol,
            const Function& function)
        {
            bindLifecycleFunction(declaration, function.id);
            if (!hasBuiltinAttribute(declaration.attributes, Attribute::Export))
                return;
            if (!symbol->genericParameterTypes.empty() && !symbol->resolvedGenericInstantiations.empty())
            {
                for (const auto& instantiation : symbol->resolvedGenericInstantiations)
                {
                    std::vector<TypeId> arguments;
                    arguments.reserve(instantiation.size());
                    for (const Ref<sema::Type>& argument : instantiation)
                        arguments.push_back(mapType(argument, &declaration));
                    appendFunctionExport(declaration, symbol, function, std::move(arguments));
                }
                return;
            }
            appendFunctionExport(declaration, symbol, function, {});
        }

        void buildFunction(const DeclarationInfo& declarationInfo)
        {
            const FunctionDeclaration& declaration = *declarationInfo.declaration;
            const Ref<sema::Symbol> symbol = declaration.name
                ? declaration.name->referencedSymbol.Lock()
                : nullptr;
            const auto functionType = symbol && symbol->type && symbol->type->kind() == sema::TypeKind::Function
                ? symbol->type.AsFast<sema::FunctionType>()
                : nullptr;
            if (!symbol || !functionType)
            {
                report("WIR2100", "Function declaration is missing its resolved semantic function type.", &declaration);
                return;
            }

            Function function;
            function.id = functionsBySymbol_.at(symbol.Get());
            function.name = symbol->scopePath.empty()
                ? symbol->name
                : symbol->scopePath + "::" + symbol->name;
            function.returnType = mapType(functionType->returnType, &declaration);
            function.callableType = mapType(functionType, &declaration);
            function.ownerType = declarationInfo.ownerType
                ? mapType(declarationInfo.ownerType, &declaration)
                : TypeId{};
            function.source = SourceSpan::at(declaration.location());
            function.isAsync = declaration.isAsync;
            function.isExternal = declaration.body == nullptr;
            function.isMethod = static_cast<bool>(function.ownerType);
            function.isExtension = declarationInfo.isExtension;
            if (function.isAsync)
            {
                const Type* task = result_.module_.types.tryGet(function.returnType);
                if (!task || task->kind != TypeKind::AsyncTask || task->arguments.size() != 1)
                    report("WIR2103", "Async function must have a resolved coroutine<T> return type.", &declaration);
                else
                    function.coroutine = CoroutineLayout{.resultType = task->arguments.front()};
            }
            for (const Ref<sema::Type>& genericParameter : symbol->genericParameterTypes)
                function.genericParameters.push_back(mapType(genericParameter, &declaration));
            const Type* ownerType = function.isMethod
                ? result_.module_.types.tryGet(function.ownerType)
                : nullptr;
            function.isAbstract = function.isExternal && ownerType && ownerType->nominalKind == NominalKind::Interface;
            if (ownerType)
            {
                const auto method = std::ranges::find_if(ownerType->methods, [&](const MethodLayout& layout)
                    { return layout.function == function.id; });
                if (method != ownerType->methods.end())
                    function.methodSlot = method->slot;
            }

            FunctionState state{.function = &function};
            if (!function.isExternal)
            {
                state.blockIndex = createBlock(
                    state,
                    "entry",
                    SourceSpan::at(declaration.body->location()));
            }
            if (function.isMethod)
            {
                const TypeId receiverType = referenceType(function.ownerType, true);
                const ValueId receiver{state.nextValue++};
                function.parameters.push_back(Parameter{
                    .id = receiver,
                    .name = "self",
                    .type = receiverType,
                    .ownership = ValueOwnership::Borrowed,
                    .borrowLifetime = BorrowLifetime::Caller,
                    .source = SourceSpan::at(declaration.location())
                });
                rememberOwnership(state, receiver, ValueOwnership::Borrowed);
                state.selfValue = receiver;
                state.selfType = receiverType;
            }
            for (std::size_t index = 0; index < declaration.parameters.size(); ++index)
            {
                const wio::Parameter& parameter = declaration.parameters[index];
                const Ref<sema::Symbol> parameterSymbol = parameter.name
                    ? parameter.name->referencedSymbol.Lock()
                    : nullptr;
                if (!parameterSymbol || index >= functionType->paramTypes.size())
                {
                    report("WIR2101", "Function parameter is missing its resolved semantic symbol or type.", parameter.name.Get());
                    continue;
                }
                const ValueId value{state.nextValue++};
                function.parameters.push_back(Parameter{
                    .id = value,
                    .name = parameterSymbol->name,
                    .type = mapType(functionType->paramTypes[index], parameter.name.Get()),
                    .ownership = ownershipForType(mapType(functionType->paramTypes[index], parameter.name.Get())),
                    .borrowLifetime = ownershipForType(mapType(functionType->paramTypes[index], parameter.name.Get())) == ValueOwnership::Borrowed
                        ? BorrowLifetime::Caller : BorrowLifetime::None,
                    .source = SourceSpan::at(parameter.name->location())
                });
                const TypeId parameterType = mapType(functionType->paramTypes[index], parameter.name.Get());
                rememberOwnership(state, value, ownershipForType(parameterType));
                const Type* parameterTypeInfo = result_.module_.types.tryGet(parameterType);
                if (function.isExtension && index == 0)
                {
                    state.selfValue = value;
                    state.selfType = parameterType;
                }
                if (function.isExternal || (parameterTypeInfo && parameterTypeInfo->kind == TypeKind::Reference))
                {
                    state.values[parameterSymbol.Get()] = value;
                    state.valueOrder.push_back(parameterSymbol.Get());
                }
                else
                {
                    const TypeId placeType = referenceType(parameterType, parameterSymbol->flags.get_isMutable());
                    const ValueId place{state.nextValue++};
                    currentBlock(state).instructions.push_back(Instruction{
                        .opcode = Opcode::LocalPlace,
                        .result = place,
                        .resultType = placeType,
                        .selector = parameterSymbol->name,
                        .source = SourceSpan::at(parameter.name->location())
                    });
                    currentBlock(state).instructions.push_back(Instruction{
                        .opcode = Opcode::PlaceInit,
                        .operands = {place, value},
                        .source = SourceSpan::at(parameter.name->location())
                    });
                    state.places[parameterSymbol.Get()] = place;
                    state.placeOrder.push_back(parameterSymbol.Get());
                }
            }

            if (hasBuiltinAttribute(declaration.attributes, Attribute::Native))
                function.nativeBinding = makeNativeBinding(declaration, function, *functionType);

            if (!function.isExternal)
            {
                buildStatement(declaration.body, state);
                if (!blockIsTerminated(state))
                {
                    const Type* returnType = result_.module_.types.tryGet(asyncResultType(function));
                    if (returnType && returnType->kind == TypeKind::Void)
                    {
                        emitDropsFrom(0, state, &declaration);
                        currentBlock(state).instructions.push_back(Instruction{.opcode = Opcode::Return});
                    }
                    else
                    {
                        report("WIR2102", "Non-void function does not end with a return in the initial Typed WIR slice.", &declaration);
                        currentBlock(state).instructions.push_back(Instruction{.opcode = Opcode::Unreachable});
                    }
                }
            }

            buildFunctionContract(declaration, symbol, function);
            result_.module_.functions.push_back(std::move(function));
        }

        void buildStatement(const NodePtr<Statement>& statement, FunctionState& state)
        {
            if (!statement)
                return;
            if (const auto* block = statement->as<BlockStatement>())
            {
                const std::size_t visibleValueCount = state.valueOrder.size();
                const std::size_t visiblePlaceCount = state.placeOrder.size();
                for (const auto& child : block->statements)
                {
                    if (blockIsTerminated(state))
                    {
                        report("WIR2200", "Statement appears after a terminator.", child.Get());
                        break;
                    }
                    buildStatement(child, state);
                }
                if (!blockIsTerminated(state))
                    emitDropsFrom(visiblePlaceCount, state, block);
                while (state.valueOrder.size() > visibleValueCount)
                {
                    state.values.erase(state.valueOrder.back());
                    state.valueOrder.pop_back();
                }
                while (state.placeOrder.size() > visiblePlaceCount)
                {
                    state.places.erase(state.placeOrder.back());
                    state.placeOrder.pop_back();
                }
                return;
            }
            if (const auto* declaration = statement->as<VariableDeclaration>())
            {
                const Ref<sema::Symbol> symbol = declaration->name
                    ? declaration->name->referencedSymbol.Lock()
                    : nullptr;
                if (!symbol)
                {
                    report("WIR2202", "Local variable declaration is missing its semantic symbol.", declaration);
                    return;
                }

                ValueId value;
                if (declaration->initializer)
                    value = buildExpressionAs(
                        declaration->initializer,
                        mapType(symbol->type, declaration),
                        state);
                else
                    value = buildDefaultValue(mapType(symbol->type, declaration), declaration, state);
                if (!value)
                    return;
                const TypeId valueType = mapType(symbol->type, declaration);
                const TypeId placeType = referenceType(valueType, symbol->flags.get_isMutable());
                const ValueId place{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::LocalPlace,
                    .result = place,
                    .resultType = placeType,
                    .selector = symbol->name,
                    .source = SourceSpan::at(declaration->location())
                });
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::PlaceInit,
                    .operands = {place, value},
                    .source = SourceSpan::at(declaration->location())
                });
                state.places[symbol.Get()] = place;
                state.placeOrder.push_back(symbol.Get());
                return;
            }
            if (const auto* ifStatement = statement->as<IfStatement>())
            {
                buildIfStatement(*ifStatement, state);
                return;
            }
            if (const auto* whileStatement = statement->as<WhileStatement>())
            {
                buildWhileStatement(*whileStatement, state);
                return;
            }
            if (const auto* cForStatement = statement->as<CForStatement>())
            {
                buildCForStatement(*cForStatement, state);
                return;
            }
            if (statement->is<BreakStatement>())
            {
                buildLoopTransfer(statement.Get(), false, state);
                return;
            }
            if (statement->is<ContinueStatement>())
            {
                buildLoopTransfer(statement.Get(), true, state);
                return;
            }
            if (const auto* returnStatement = statement->as<ReturnStatement>())
            {
                Instruction instruction{
                    .opcode = Opcode::Return,
                    .source = SourceSpan::at(returnStatement->location())
                };
                if (returnStatement->value)
                {
                    ValueId value;
                    const TypeId expectedReturnType = asyncResultType(*state.function);
                    if (const auto* identifier = returnStatement->value->as<Identifier>())
                    {
                        const Ref<sema::Symbol> symbol = identifier->referencedSymbol.Lock();
                        const auto place = symbol ? state.places.find(symbol.Get()) : state.places.end();
                        const TypeId valueType = symbol ? mapType(symbol->type, returnStatement) : TypeId{};
                        if (place != state.places.end() && valueType == expectedReturnType &&
                            typeRequiresCleanup(valueType))
                        {
                            value = ValueId{state.nextValue++};
                            currentBlock(state).instructions.push_back(Instruction{
                                .opcode = Opcode::Move,
                                .result = value,
                                .resultType = valueType,
                                .operands = {place->second},
                                .resultOwnership = ValueOwnership::Owned,
                                .source = SourceSpan::at(returnStatement->location())
                            });
                            rememberOwnership(state, value, ValueOwnership::Owned);
                            state.movedPlaces.insert(symbol.Get());
                        }
                    }
                    if (!value)
                        value = buildExpressionAs(returnStatement->value, expectedReturnType, state);
                    if (value)
                        instruction.operands.push_back(value);
                }
                emitDropsFrom(0, state, returnStatement);
                currentBlock(state).instructions.push_back(std::move(instruction));
                return;
            }
            if (const auto* expressionStatement = statement->as<ExpressionStatement>())
            {
                const ValueId discarded = buildExpression(expressionStatement->expression, state);
                const TypeId discardedType = expressionStatement->expression
                    ? mapExpressionType(expressionStatement->expression, expressionStatement)
                    : TypeId{};
                if (discarded && typeRequiresCleanup(discardedType) &&
                    valueOwnership(state, discarded) == ValueOwnership::Owned)
                {
                    currentBlock(state).instructions.push_back(Instruction{
                        .opcode = Opcode::Release,
                        .operands = {discarded},
                        .source = SourceSpan::at(expressionStatement->location())
                    });
                }
                return;
            }
            report("WIR2201", "Statement kind '" + getKindNameStr(statement->kind()) + "' is not supported by the initial Typed WIR slice.", statement.Get());
        }

        ValueId buildExpression(const NodePtr<Expression>& expression, FunctionState& state)
        {
            if (!expression)
                return {};
            auto appendValue = [&](Instruction instruction)
            {
                instruction.result = ValueId{state.nextValue++};
                instruction.resultType = mapExpressionType(expression, expression.Get());
                if (instruction.resultOwnership == ValueOwnership::Trivial)
                    instruction.resultOwnership = ownershipForType(instruction.resultType);
                if (instruction.resultOwnership == ValueOwnership::Borrowed &&
                    instruction.borrowLifetime == BorrowLifetime::None)
                {
                    instruction.borrowLifetime = BorrowLifetime::Caller;
                }
                const ValueId result = instruction.result;
                currentBlock(state).instructions.push_back(std::move(instruction));
                rememberOwnership(state, result, currentBlock(state).instructions.back().resultOwnership);
                return result;
            };

            if (const auto* integer = expression->as<IntegerLiteral>())
            {
                const TypeId typeId = mapType(expression->refType.Lock(), expression.Get());
                const Type* type = result_.module_.types.tryGet(typeId);
                const auto integerType = type ? mapIntegerType(type->kind) : std::nullopt;
                if (!integerType)
                {
                    report("WIR2300", "Integer literal has no representable WIR integer type.", expression.Get());
                    return {};
                }
                const IntegerResult parsed = common::getIntegerAsType(integer->token.value, *integerType);
                const auto literal = integerLiteral(parsed);
                if (!parsed.isValid || !literal)
                {
                    report("WIR2300", "Integer literal cannot be represented by the initial Typed WIR literal model.", expression.Get());
                    return {};
                }
                return appendValue(Instruction{
                    .opcode = Opcode::Constant,
                    .literal = *literal,
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* floating = expression->as<FloatLiteral>())
            {
                const TypeId typeId = mapType(expression->refType.Lock(), expression.Get());
                const Type* type = result_.module_.types.tryGet(typeId);
                const auto floatType = type ? mapFloatType(type->kind) : std::nullopt;
                if (!floatType)
                {
                    report("WIR2309", "Float literal has no representable WIR floating-point type.", expression.Get());
                    return {};
                }
                const FloatResult parsed = common::getFloatAsType(floating->token.value, *floatType);
                if (!parsed.isValid)
                {
                    report("WIR2309", "Float literal cannot be represented by the Typed WIR literal model.", expression.Get());
                    return {};
                }
                const double value = parsed.type == FloatType::f32
                    ? static_cast<double>(parsed.value.v_f32)
                    : parsed.value.v_f64;
                return appendValue(Instruction{
                    .opcode = Opcode::Constant,
                    .literal = value,
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* boolean = expression->as<BoolLiteral>())
            {
                return appendValue(Instruction{
                    .opcode = Opcode::Constant,
                    .literal = boolean->token.type == TokenType::kwTrue,
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* string = expression->as<StringLiteral>())
            {
                return appendValue(Instruction{
                    .opcode = Opcode::Constant,
                    .literal = string->token.value,
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* interpolated = expression->as<InterpolatedStringLiteral>())
            {
                Instruction instruction{
                    .opcode = Opcode::Interpolate,
                    .intrinsicFamily = interpolated->isUnicode
                        ? IntrinsicFamily::Text
                        : IntrinsicFamily::String,
                    .source = SourceSpan::at(expression->location())
                };
                std::vector<ValueId> ownedTemporaries;
                instruction.stringSegments.emplace_back();
                for (const auto& part : interpolated->parts)
                {
                    if (const auto* literalPart = part ? part->as<StringLiteral>() : nullptr)
                    {
                        instruction.stringSegments.back() += literalPart->token.value;
                        continue;
                    }
                    const ValueId value = buildAutoReadableExpression(part, state);
                    const TypeId valueType = part ? mapType(part->refType.Lock(), part.Get()) : TypeId{};
                    if (!value || !valueType)
                        return {};
                    instruction.operands.push_back(value);
                    if (valueOwnership(state, value) == ValueOwnership::Owned)
                        ownedTemporaries.push_back(value);
                    instruction.signatureTypes.push_back(valueType);
                    instruction.stringSegments.emplace_back();
                }
                const ValueId result = appendValue(std::move(instruction));
                for (const ValueId temporary : ownedTemporaries)
                    releaseOwnedTemporary(temporary, expression.Get(), state);
                return result;
            }
            if (const auto* character = expression->as<CharLiteral>())
            {
                if (character->token.value.size() != 1)
                {
                    report("WIR2310", "Char literal must contain exactly one byte-sized character.", expression.Get());
                    return {};
                }
                return appendValue(Instruction{
                    .opcode = Opcode::Constant,
                    .literal = static_cast<std::uint64_t>(
                        static_cast<unsigned char>(character->token.value.front())),
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* byte = expression->as<ByteLiteral>())
            {
                const IntegerResult parsed = common::getIntegerAsType(byte->token.value, IntegerType::u8);
                if (!parsed.isValid)
                {
                    report("WIR2311", "Byte literal is outside the u8 range.", expression.Get());
                    return {};
                }
                return appendValue(Instruction{
                    .opcode = Opcode::Constant,
                    .literal = static_cast<std::uint64_t>(parsed.value.v_u8),
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* array = expression->as<ArrayLiteral>())
            {
                const TypeId arrayTypeId = mapType(expression->refType.Lock(), expression.Get());
                const Type* arrayType = result_.module_.types.tryGet(arrayTypeId);
                if (!arrayType || arrayType->kind != TypeKind::Array || arrayType->arguments.size() != 1)
                {
                    report("WIR2320", "Array literal requires a resolved WIR array element type.", expression.Get());
                    return {};
                }
                const TypeId elementType = arrayType->arguments.front();

                Instruction instruction{
                    .opcode = Opcode::ArrayCreate,
                    .source = SourceSpan::at(expression->location())
                };
                instruction.operands.reserve(array->elements.size());
                for (const auto& element : array->elements)
                {
                    const ValueId value = buildExpressionAs(element, elementType, state);
                    if (!value)
                        return {};
                    instruction.operands.push_back(value);
                }
                return appendValue(std::move(instruction));
            }
            if (const auto* dictionary = expression->as<DictionaryLiteral>())
            {
                const TypeId dictionaryTypeId = mapType(expression->refType.Lock(), expression.Get());
                const Type* dictionaryType = result_.module_.types.tryGet(dictionaryTypeId);
                if (!dictionaryType || dictionaryType->kind != TypeKind::Dictionary ||
                    dictionaryType->arguments.size() != 2)
                {
                    report("WIR2345", "Dictionary literal requires resolved key and value types.", expression.Get());
                    return {};
                }
                const TypeId keyType = dictionaryType->arguments[0];
                const TypeId valueType = dictionaryType->arguments[1];
                Instruction instruction{
                    .opcode = Opcode::DictionaryCreate,
                    .selector = dictionary->isOrdered ? "ordered" : "unordered",
                    .source = SourceSpan::at(expression->location())
                };
                instruction.operands.reserve(dictionary->pairs.size() * 2);
                instruction.signatureTypes.reserve(dictionary->pairs.size() * 2);
                for (const auto& [key, valueExpression] : dictionary->pairs)
                {
                    const ValueId keyValue = buildExpressionAs(key, keyType, state);
                    const ValueId mappedValue = buildExpressionAs(valueExpression, valueType, state);
                    if (!keyValue || !mappedValue)
                        return {};
                    instruction.operands.push_back(keyValue);
                    instruction.operands.push_back(mappedValue);
                    instruction.signatureTypes.push_back(keyType);
                    instruction.signatureTypes.push_back(valueType);
                }
                return appendValue(std::move(instruction));
            }
            if (expression->is<NullExpression>())
            {
                return appendValue(Instruction{
                    .opcode = Opcode::Constant,
                    .literal = NullLiteral{},
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* lambda = expression->as<LambdaExpression>())
            {
                std::vector<CaptureLayout> captures;
                std::vector<ValueId> operands;
                std::vector<TypeId> signatureTypes;
                std::vector<CaptureKind> captureKinds;
                for (const WeakRef<sema::Symbol>& weakSymbol : lambda->capturedSymbols)
                {
                    const Ref<sema::Symbol> symbol = weakSymbol.Lock();
                    if (!symbol)
                        continue;
                    const TypeId captureType = mapType(symbol->type, expression.Get());
                    ValueId value;
                    if (const auto place = state.places.find(symbol.Get()); place != state.places.end())
                        value = emitLoad(place->second, captureType, expression.Get(), state);
                    else if (const auto available = state.values.find(symbol.Get()); available != state.values.end())
                        value = available->second;
                    if (!value)
                    {
                        report("WIR2342", "Lambda capture '" + symbol->name + "' is unavailable in the enclosing callable.", expression.Get());
                        return {};
                    }
                    const Type* captureTypeInfo = result_.module_.types.tryGet(captureType);
                    const CaptureKind kind = captureTypeInfo && captureTypeInfo->kind == TypeKind::Reference
                        ? CaptureKind::Reference
                        : CaptureKind::Value;
                    if (kind == CaptureKind::Value)
                        value = ensureOwned(value, captureType, expression.Get(), state);
                    captures.push_back(CaptureLayout{.name = symbol->name, .type = captureType, .kind = kind});
                    operands.push_back(value);
                    signatureTypes.push_back(captureType);
                    captureKinds.push_back(kind);
                }
                TypeId selfCaptureType;
                if (lambda->capturesSelf)
                {
                    if (!state.selfValue || !state.selfType)
                    {
                        report("WIR2343", "Lambda requested a self capture outside a receiver-aware callable.", expression.Get());
                        return {};
                    }
                    selfCaptureType = state.selfType;
                    captures.push_back(CaptureLayout{
                        .name = "self",
                        .type = selfCaptureType,
                        .kind = CaptureKind::RetainedSelf
                    });
                    operands.push_back(state.selfValue);
                    signatureTypes.push_back(selfCaptureType);
                    captureKinds.push_back(CaptureKind::RetainedSelf);
                }
                const FunctionId lambdaFunction = buildLambdaFunction(*lambda, captures, selfCaptureType);
                return appendValue(Instruction{
                    .opcode = Opcode::ClosureCreate,
                    .operands = std::move(operands),
                    .callee = lambdaFunction,
                    .signatureTypes = std::move(signatureTypes),
                    .captureKinds = std::move(captureKinds),
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (expression->is<SelfExpression>())
            {
                if (!state.selfValue)
                    report("WIR2331", "'self' is unavailable outside a receiver-aware WIR function.", expression.Get());
                return state.selfValue;
            }
            if (expression->is<SuperExpression>())
            {
                if (!state.selfValue)
                {
                    report("WIR2332", "'super' is unavailable outside a receiver-aware WIR function.", expression.Get());
                    return {};
                }
                return appendValue(Instruction{
                    .opcode = Opcode::Upcast,
                    .operands = {state.selfValue},
                    .targetType = mapType(expression->refType.Lock(), expression.Get()),
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* fit = expression->as<FitExpression>())
            {
                if (fit->operatorDispatchKind != OperatorDispatchKind::None)
                {
                    report("WIR2312", "Overloaded fit conversion is not yet supported by Typed WIR.", expression.Get());
                    return {};
                }
                const TypeId sourceTypeId = mapType(fit->operand->refType.Lock(), fit->operand.Get());
                const TypeId destinationTypeId = mapType(expression->refType.Lock(), expression.Get());
                const Type* sourceType = result_.module_.types.tryGet(sourceTypeId);
                const Type* destinationType = result_.module_.types.tryGet(destinationTypeId);
                const bool numeric = sourceType && destinationType &&
                    isNumericType(sourceType->kind) && isNumericType(destinationType->kind);
                const bool anyCast = sourceType && sourceType->kind == TypeKind::Any;
                const bool objectLike = underlyingNominalType(sourceTypeId) &&
                    underlyingNominalType(destinationTypeId);
                if (!numeric && !objectLike && !anyCast)
                {
                    report("WIR2313", "Typed WIR fit lowering requires numeric or object/interface operands.", expression.Get());
                    return {};
                }
                const ValueId operand = buildExpression(fit->operand, state);
                if (!operand)
                    return {};
                return appendValue(Instruction{
                    .opcode = anyCast
                        ? Opcode::AnyCheckedCast
                        : objectLike ? Opcode::CheckedCast : Opcode::Convert,
                    .operands = {operand},
                    .conversionKind = ConversionKind::NumericFit,
                    .targetType = destinationTypeId,
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* identifier = expression->as<Identifier>())
            {
                const Ref<sema::Symbol> symbol = identifier->referencedSymbol.Lock();
                const auto place = symbol ? state.places.find(symbol.Get()) : state.places.end();
                if (place != state.places.end())
                    return emitLoad(place->second, mapType(symbol->type, expression.Get()), expression.Get(), state);
                const auto found = symbol ? state.values.find(symbol.Get()) : state.values.end();
                if (found != state.values.end())
                    return found->second;
                const Ref<sema::Symbol> callable = resolveCallableSymbol(symbol, expression->refType.Lock());
                const auto function = callable
                    ? functionsBySymbol_.find(callable.Get())
                    : functionsBySymbol_.end();
                if (function != functionsBySymbol_.end())
                {
                    return appendValue(Instruction{
                        .opcode = Opcode::FunctionReference,
                        .callee = function->second,
                        .specializationKey = specializationKey(
                            function->second,
                            {},
                            {},
                            mapType(expression->refType.Lock(), expression.Get())),
                        .source = SourceSpan::at(expression->location())
                    });
                }
                report("WIR2301", "Identifier is not a value available in the current Typed WIR function.", expression.Get());
                return {};
            }
            if (const auto* reference = expression->as<RefExpression>())
            {
                const TypeId referenceTypeId = mapType(expression->refType.Lock(), expression.Get());
                const Type* referenceTypeInfo = result_.module_.types.tryGet(referenceTypeId);
                return buildPlace(
                    reference->operand,
                    referenceTypeInfo && referenceTypeInfo->kind == TypeKind::Reference && referenceTypeInfo->isMutable,
                    state);
            }
            if (const auto* member = expression->as<MemberAccessExpression>())
            {
                const Ref<sema::Symbol> ownerSymbol = member->object
                    ? member->object->referencedSymbol.Lock()
                    : nullptr;
                const TypeId ownerType = member->object
                    ? mapType(member->object->refType.Lock(), member->object.Get())
                    : TypeId{};
                const Type* ownerTypeInfo = result_.module_.types.tryGet(ownerType);
                const bool isStaticNominalAccess = member->object &&
                    (member->object->is<TypeExpression>() ||
                     (ownerSymbol && (ownerSymbol->kind == sema::SymbolKind::Struct ||
                                      ownerSymbol->kind == sema::SymbolKind::TypeAlias)));
                if (isStaticNominalAccess && ownerTypeInfo && ownerTypeInfo->kind == TypeKind::Named &&
                    (ownerTypeInfo->nominalKind == NominalKind::Enum ||
                     ownerTypeInfo->nominalKind == NominalKind::Flagset))
                {
                    return appendValue(Instruction{
                        .opcode = Opcode::EnumConstant,
                        .selector = member->member ? member->member->token.value : std::string{},
                        .intrinsicFamily = ownerTypeInfo->nominalKind == NominalKind::Enum
                            ? IntrinsicFamily::Enum
                            : IntrinsicFamily::Flagset,
                        .targetType = ownerType,
                        .source = SourceSpan::at(expression->location())
                    });
                }
                if (!member->referencedSymbol.Lock() ||
                    member->referencedSymbol.Lock()->kind != sema::SymbolKind::Variable)
                {
                    report("WIR2325", "Initial Typed WIR member reads currently require a resolved data field.", expression.Get());
                    return {};
                }
                const ValueId place = buildPlace(expression, false, state);
                if (!place)
                    return {};
                return emitLoad(place, mapType(expression->refType.Lock(), expression.Get()), expression.Get(), state);
            }
            if (const auto* access = expression->as<ArrayAccessExpression>())
            {
                if (access->operatorDispatchKind != OperatorDispatchKind::None)
                {
                    report("WIR2321", "Overloaded index access is not yet supported by Typed WIR.", expression.Get());
                    return {};
                }
                TypeId objectTypeId = mapType(access->object->refType.Lock(), access->object.Get());
                const Type* objectType = result_.module_.types.tryGet(objectTypeId);
                if (objectType && objectType->kind == TypeKind::Reference && autoReadableReference(*objectType))
                {
                    objectTypeId = objectType->arguments.front();
                    objectType = result_.module_.types.tryGet(objectTypeId);
                }
                if (!objectType || (objectType->kind != TypeKind::Array &&
                    objectType->kind != TypeKind::Dictionary &&
                    objectType->kind != TypeKind::String && objectType->kind != TypeKind::Text))
                {
                    report("WIR2322", "Typed WIR index access requires an array, dictionary, string, or text value.", expression.Get());
                    return {};
                }
                const ValueId object = buildAutoReadableExpression(access->object, state);
                const ValueId index = buildAutoReadableExpression(access->index, state);
                if (!object || !index)
                    return {};
                const ValueId result = appendValue(Instruction{
                    .opcode = objectType->kind == TypeKind::Array
                        ? Opcode::ArrayGet
                        : objectType->kind == TypeKind::Dictionary
                            ? Opcode::DictionaryGet
                            : Opcode::IntrinsicCall,
                    .operands = {object, index},
                    .selector = objectType->kind == TypeKind::String || objectType->kind == TypeKind::Text
                        ? "Get"
                        : std::string{},
                    .signatureTypes = {objectTypeId, mapType(access->index->refType.Lock(), access->index.Get())},
                    .intrinsicFamily = intrinsicFamilyFor(objectTypeId),
                    .targetType = objectTypeId,
                    .source = SourceSpan::at(expression->location())
                });
                releaseOwnedTemporary(object, expression.Get(), state);
                return result;
            }
            if (const auto* assignment = expression->as<AssignmentExpression>())
            {
                TypeId targetType = mapType(assignment->left->refType.Lock(), assignment->left.Get());
                const Type* targetTypeInfo = result_.module_.types.tryGet(targetType);
                const bool assignsThroughReadableReference = targetTypeInfo &&
                    targetTypeInfo->kind == TypeKind::Reference && autoReadableReference(*targetTypeInfo);
                const ValueId target = assignsThroughReadableReference
                    ? buildExpression(assignment->left, state)
                    : buildPlace(assignment->left, true, state);
                if (!target)
                    return {};
                if (assignsThroughReadableReference)
                    targetType = targetTypeInfo->arguments.front();
                const ValueId right = buildExpressionAs(assignment->right, targetType, state);
                if (!right)
                    return {};
                ValueId assigned = right;
                if (assignment->op.type != TokenType::opAssign)
                {
                    const auto compoundOperator = mapCompoundAssignmentOperator(assignment->op.type);
                    if (!compoundOperator)
                    {
                        report("WIR2308", "Compound assignment operator is not supported by Typed WIR.", expression.Get());
                        return {};
                    }
                    assigned = ValueId{state.nextValue++};
                    const ValueId currentValue = emitLoad(target, targetType, assignment->left.Get(), state);
                    currentBlock(state).instructions.push_back(Instruction{
                        .opcode = Opcode::Binary,
                        .result = assigned,
                        .resultType = targetType,
                        .operands = {currentValue, right},
                        .binaryOperator = *compoundOperator,
                        .resultOwnership = ownershipForType(targetType),
                        .source = SourceSpan::at(expression->location())
                    });
                    rememberOwnership(state, assigned, ownershipForType(targetType));
                }
                const bool managedTarget = typeRequiresCleanup(targetType);
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = managedTarget ? Opcode::Replace : Opcode::Store,
                    .operands = {target, assigned},
                    .source = SourceSpan::at(expression->location())
                });
                return managedTarget
                    ? emitLoad(target, targetType, expression.Get(), state)
                    : assigned;
            }
            if (const auto* unary = expression->as<UnaryExpression>())
            {
                if (unary->op.type == TokenType::kwAwait)
                {
                    if (unary->isMainExecutorAwait)
                    {
                        currentBlock(state).instructions.push_back(Instruction{
                            .opcode = Opcode::ExecutorSwitch,
                            .asyncOperation = AsyncOperation::SwitchExecutor,
                            .asyncExecutor = AsyncExecutorKind::Main,
                            .source = SourceSpan::at(expression->location())
                        });
                        return {};
                    }
                    const ValueId taskValue = buildExpression(unary->operand, state);
                    const TypeId taskTypeId = mapType(unary->operand->refType.Lock(), unary->operand.Get());
                    const Type* taskType = result_.module_.types.tryGet(taskTypeId);
                    if (!taskValue || !taskType || taskType->kind != TypeKind::AsyncTask || taskType->arguments.size() != 1)
                    {
                        report("WIR2306", "Await requires a resolved coroutine<T> operand.", expression.Get());
                        return {};
                    }
                    Instruction instruction{
                        .opcode = Opcode::Await,
                        .operands = {taskValue},
                        .asyncOperation = AsyncOperation::AwaitTask,
                        .asyncExecutor = AsyncExecutorKind::Inherit,
                        .source = SourceSpan::at(expression->location())
                    };
                    const Type* resultType = result_.module_.types.tryGet(taskType->arguments.front());
                    if (resultType && resultType->kind == TypeKind::Void)
                    {
                        currentBlock(state).instructions.push_back(std::move(instruction));
                        releaseOwnedTemporary(taskValue, expression.Get(), state);
                        return {};
                    }
                    const ValueId result = appendValue(std::move(instruction));
                    releaseOwnedTemporary(taskValue, expression.Get(), state);
                    return result;
                }
                if (unary->op.type == TokenType::kwDeref)
                {
                    const ValueId place = buildExpression(unary->operand, state);
                    if (!place)
                        return {};
                    return emitLoad(place, mapType(expression->refType.Lock(), expression.Get()), expression.Get(), state);
                }
                const auto op = mapUnaryOperator(unary->op.type);
                const ValueId operand = buildAutoReadableExpression(unary->operand, state);
                if (!op || !operand)
                {
                    report("WIR2302", "Unary expression is not supported by the initial Typed WIR slice.", expression.Get());
                    return {};
                }
                const ValueId result = appendValue(Instruction{
                    .opcode = Opcode::Unary,
                    .operands = {operand},
                    .unaryOperator = *op,
                    .source = SourceSpan::at(expression->location())
                });
                releaseOwnedTemporary(operand, expression.Get(), state);
                return result;
            }
            if (const auto* binary = expression->as<BinaryExpression>())
            {
                if (isLogicalAnd(binary->op.type) || isLogicalOr(binary->op.type))
                {
                    return buildShortCircuitExpression(
                        *binary,
                        isLogicalAnd(binary->op.type),
                        state);
                }
                if (binary->op.type == TokenType::kwIs)
                {
                    const ValueId operand = buildExpression(binary->left, state);
                    const TypeId sourceType = mapType(binary->left->refType.Lock(), binary->left.Get());
                    const TypeId targetType = mapType(binary->right->refType.Lock(), binary->right.Get());
                    const Type* sourceTypeInfo = result_.module_.types.tryGet(sourceType);
                    const bool anyTest = sourceTypeInfo && sourceTypeInfo->kind == TypeKind::Any;
                    if (!operand || (!anyTest && !underlyingNominalType(targetType)))
                    {
                        report("WIR2333", "Object/interface type test is missing a representable target type.", expression.Get());
                        return {};
                    }
                    const ValueId result = appendValue(Instruction{
                        .opcode = anyTest ? Opcode::AnyTypeTest : Opcode::TypeTest,
                        .operands = {operand},
                        .targetType = targetType,
                        .source = SourceSpan::at(expression->location())
                    });
                    releaseOwnedTemporary(operand, expression.Get(), state);
                    return result;
                }
                const auto op = mapBinaryOperator(binary->op.type);
                const ValueId left = buildAutoReadableExpression(binary->left, state);
                const ValueId right = buildAutoReadableExpression(binary->right, state);
                if (!op || !left || !right)
                {
                    report("WIR2303", "Binary expression is not supported by the initial Typed WIR slice.", expression.Get());
                    return {};
                }
                const bool identityComparison =
                    (binary->op.type == TokenType::opEqual || binary->op.type == TokenType::opNotEqual) &&
                    underlyingNominalType(mapType(binary->left->refType.Lock(), binary->left.Get())) &&
                    underlyingNominalType(mapType(binary->right->refType.Lock(), binary->right.Get()));
                const ValueId result = appendValue(Instruction{
                    .opcode = identityComparison ? Opcode::IdentityEqual : Opcode::Binary,
                    .operands = {left, right},
                    .binaryOperator = *op,
                    .source = SourceSpan::at(expression->location())
                });
                releaseOwnedTemporary(left, expression.Get(), state);
                releaseOwnedTemporary(right, expression.Get(), state);
                return result;
            }
            if (const auto* conditional = expression->as<ConditionalExpression>())
            {
                const TypeId resultType = mapType(expression->refType.Lock(), expression.Get());
                if (typeRequiresCleanup(resultType) ||
                    !isSideEffectFree(conditional->whenTrue) || !isSideEffectFree(conditional->whenFalse))
                    return buildConditionalControlFlow(*conditional, state);
                const ValueId condition = buildExpression(conditional->condition, state);
                const ValueId whenTrue = buildExpressionAs(conditional->whenTrue, resultType, state);
                const ValueId whenFalse = buildExpressionAs(conditional->whenFalse, resultType, state);
                if (!condition || !whenTrue || !whenFalse)
                    return {};
                return appendValue(Instruction{
                    .opcode = Opcode::Select,
                    .operands = {condition, whenTrue, whenFalse},
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* match = expression->as<MatchExpression>())
                return buildMatchExpression(*match, state);
            if (const auto* call = expression->as<FunctionCallExpression>())
            {
                Ref<sema::Symbol> calleeSymbol = call->referencedSymbol.Lock();
                if (!calleeSymbol && call->callee)
                    calleeSymbol = call->callee->referencedSymbol.Lock();
                const TypeId callResultType = mapExpressionType(expression, expression.Get());
                const Type* callResultTypeInfo = result_.module_.types.tryGet(callResultType);
                const bool hasNominalConstructionResult = callResultTypeInfo && callResultTypeInfo->kind == TypeKind::Named &&
                    (callResultTypeInfo->nominalKind == NominalKind::Component ||
                     callResultTypeInfo->nominalKind == NominalKind::Object);
                const bool isConstructor = hasNominalConstructionResult && calleeSymbol &&
                    (calleeSymbol->name == "OnConstruct" || calleeSymbol->kind == sema::SymbolKind::Struct ||
                     calleeSymbol->kind == sema::SymbolKind::TypeAlias);
                if (isConstructor)
                {
                    const NominalKind nominalKind = callResultTypeInfo->nominalKind;
                    Ref<sema::Symbol> constructorSymbol = calleeSymbol;
                    Ref<sema::Type> constructorOwnerType = calleeSymbol->kind == sema::SymbolKind::TypeAlias
                        ? calleeSymbol->aliasTargetType
                        : calleeSymbol->type;
                    while (constructorOwnerType && constructorOwnerType->kind() == sema::TypeKind::Alias)
                        constructorOwnerType = constructorOwnerType.AsFast<sema::AliasType>()->aliasedType;
                    if (calleeSymbol->name != "OnConstruct" && constructorOwnerType &&
                        constructorOwnerType->kind() == sema::TypeKind::Struct)
                    {
                        const Ref<sema::Scope> ownerScope = constructorOwnerType.AsFast<sema::StructType>()->structScope.Lock();
                        if (ownerScope)
                            constructorSymbol = ownerScope->resolveLocally("OnConstruct");
                    }
                    const auto constructorType = constructorSymbol && constructorSymbol->type &&
                        constructorSymbol->type->kind() == sema::TypeKind::Function
                        ? constructorSymbol->type.AsFast<sema::FunctionType>()
                        : nullptr;
                    Instruction instruction{
                        .opcode = nominalKind == NominalKind::Object
                            ? Opcode::ConstructObject
                            : Opcode::ConstructComponent,
                        .selector = callResultTypeInfo->name + "::OnConstruct",
                        .source = SourceSpan::at(expression->location())
                    };
                    for (std::size_t index = 0; index < call->arguments.size(); ++index)
                    {
                        const auto& argument = call->arguments[index];
                        const TypeId expectedType = constructorType && index < constructorType->paramTypes.size()
                            ? mapType(constructorType->paramTypes[index], argument.Get())
                            : mapType(argument->refType.Lock(), argument.Get());
                        const ValueId value = buildExpressionAs(argument, expectedType, state);
                        if (!value)
                            return {};
                        instruction.operands.push_back(value);
                        instruction.signatureTypes.push_back(expectedType);
                    }
                    return appendValue(std::move(instruction));
                }

                const auto* memberCallee = call->callee
                    ? call->callee->as<MemberAccessExpression>()
                    : nullptr;
                if (memberCallee && memberCallee->object &&
                    memberCallee->intrinsicMember != IntrinsicMember::None)
                {
                    TypeId receiverType = mapType(
                        memberCallee->object->refType.Lock(), memberCallee->object.Get());
                    const Type* receiverTypeInfo = result_.module_.types.tryGet(receiverType);
                    if (receiverTypeInfo && receiverTypeInfo->kind == TypeKind::Reference &&
                        receiverTypeInfo->arguments.size() == 1)
                    {
                        receiverType = receiverTypeInfo->arguments.front();
                    }
                    const bool mutating = sema::isMutatingIntrinsicMember(memberCallee->intrinsicMember);
                    const ValueId receiver = mutating
                        ? buildPlace(memberCallee->object, true, state)
                        : buildAutoReadableExpression(memberCallee->object, state);
                    if (!receiver)
                        return {};
                    Instruction instruction{
                        .opcode = Opcode::IntrinsicCall,
                        .operands = {receiver},
                        .selector = memberCallee->member
                            ? memberCallee->member->token.value
                            : std::string{},
                        .intrinsicFamily = intrinsicFamilyFor(receiverType),
                        .targetType = receiverType,
                        .source = SourceSpan::at(expression->location())
                    };
                    instruction.signatureTypes.push_back(mutating
                        ? referenceType(receiverType, true)
                        : receiverType);
                    const Ref<sema::Type> selectedCallableType = call->callee->refType.Lock();
                    const auto functionType = selectedCallableType &&
                        selectedCallableType->kind() == sema::TypeKind::Function
                        ? selectedCallableType.AsFast<sema::FunctionType>()
                        : nullptr;
                    for (std::size_t index = 0; index < call->arguments.size(); ++index)
                    {
                        const auto& argument = call->arguments[index];
                        const TypeId expectedType = functionType && index < functionType->paramTypes.size()
                            ? mapType(functionType->paramTypes[index], argument.Get())
                            : mapType(argument->refType.Lock(), argument.Get());
                        const ValueId argumentValue = buildExpressionAs(argument, expectedType, state);
                        if (!argumentValue)
                            return {};
                        instruction.operands.push_back(argumentValue);
                        instruction.signatureTypes.push_back(expectedType);
                    }
                    if (callResultTypeInfo && callResultTypeInfo->kind == TypeKind::Void)
                    {
                        currentBlock(state).instructions.push_back(std::move(instruction));
                        if (!mutating)
                            releaseOwnedTemporary(receiver, expression.Get(), state);
                        return {};
                    }
                    const ValueId result = appendValue(std::move(instruction));
                    if (!mutating)
                        releaseOwnedTemporary(receiver, expression.Get(), state);
                    return result;
                }
                if (memberCallee && memberCallee->object &&
                    !memberCallee->object->is<TypeExpression>() &&
                    (!memberCallee->object->referencedSymbol.Lock() ||
                     memberCallee->object->referencedSymbol.Lock()->kind != sema::SymbolKind::Namespace))
                {
                    const Ref<sema::Symbol> selectedMember = calleeSymbol;
                    const Ref<sema::Symbol> extensionImplementation = selectedMember &&
                        selectedMember->flags.get_isExtension()
                        ? resolveCallableSymbol(selectedMember, call->callee->refType.Lock())
                        : nullptr;
                    const auto extensionFunction = extensionImplementation
                        ? functionsBySymbol_.find(extensionImplementation.Get())
                        : functionsBySymbol_.end();
                    if (extensionFunction != functionsBySymbol_.end())
                    {
                        const auto implementationType = extensionImplementation->type &&
                            extensionImplementation->type->kind() == sema::TypeKind::Function
                            ? extensionImplementation->type.AsFast<sema::FunctionType>()
                            : nullptr;
                        const auto visibleType = call->callee->refType.Lock() &&
                            call->callee->refType.Lock()->kind() == sema::TypeKind::Function
                            ? call->callee->refType.Lock().AsFast<sema::FunctionType>()
                            : nullptr;
                        Instruction instruction{
                            .opcode = isNativeFunction(extensionFunction->second)
                                ? Opcode::NativeCall
                                : Opcode::ExtensionCall,
                            .callee = extensionFunction->second,
                            .selector = selectedMember->extensionMemberName.empty()
                                ? selectedMember->name
                                : selectedMember->extensionMemberName,
                            .targetType = mapType(selectedMember->extensionTargetType, expression.Get()),
                            .source = SourceSpan::at(expression->location())
                        };
                        const TypeId receiverType = implementationType && !implementationType->paramTypes.empty()
                            ? mapType(implementationType->paramTypes.front(), memberCallee->object.Get())
                            : mapType(memberCallee->object->refType.Lock(), memberCallee->object.Get());
                        const Type* receiverTypeInfo = result_.module_.types.tryGet(receiverType);
                        const bool borrowedReceiver = receiverTypeInfo && receiverTypeInfo->kind == TypeKind::Reference;
                        const ValueId receiver = borrowedReceiver
                            ? buildExpression(memberCallee->object, state)
                            : buildExpressionAs(memberCallee->object, receiverType, state);
                        if (!receiver)
                            return {};
                        instruction.operands.push_back(receiver);
                        instruction.signatureTypes.push_back(receiverType);
                        for (std::size_t index = 0; index < call->arguments.size(); ++index)
                        {
                            const auto& argument = call->arguments[index];
                            const TypeId expectedType = visibleType && index < visibleType->paramTypes.size()
                                ? mapType(visibleType->paramTypes[index], argument.Get())
                                : mapType(argument->refType.Lock(), argument.Get());
                            const ValueId value = buildExpressionAs(argument, expectedType, state);
                            if (!value)
                                return {};
                            instruction.operands.push_back(value);
                            instruction.signatureTypes.push_back(expectedType);
                        }
                        instruction.genericArguments = genericArguments(
                            *call,
                            extensionImplementation,
                            implementationType);
                        instruction.specializationKey = specializationKey(
                            instruction.callee,
                            instruction.genericArguments,
                            instruction.signatureTypes,
                            callResultType);
                        decorateAsyncOperation(instruction);
                        const bool nativeCall = instruction.opcode == Opcode::NativeCall;
                        const std::vector<ValueId> nativeArguments = nativeCall
                            ? instruction.operands : std::vector<ValueId>{};
                        if (callResultTypeInfo && callResultTypeInfo->kind == TypeKind::Void)
                        {
                            currentBlock(state).instructions.push_back(std::move(instruction));
                            for (const ValueId argument : nativeArguments)
                                releaseOwnedTemporary(argument, expression.Get(), state);
                            if (borrowedReceiver)
                                releaseOwnedTemporary(receiver, expression.Get(), state);
                            return {};
                        }
                        const ValueId result = appendValue(std::move(instruction));
                        for (const ValueId argument : nativeArguments)
                            releaseOwnedTemporary(argument, expression.Get(), state);
                        if (borrowedReceiver)
                            releaseOwnedTemporary(receiver, expression.Get(), state);
                        return result;
                    }

                    TypeId receiverNominalType;
                    const TypeId rawReceiverType = mapType(
                        memberCallee->object->refType.Lock(), memberCallee->object.Get());
                    const Type* receiverOwner = underlyingNominalType(rawReceiverType, &receiverNominalType);
                    const auto functionIt = calleeSymbol
                        ? functionsBySymbol_.find(calleeSymbol.Get())
                        : functionsBySymbol_.end();
                    const MethodLayout* method = findMethodLayout(receiverNominalType, calleeSymbol);
                    if (receiverOwner && functionIt != functionsBySymbol_.end() && method)
                    {
                        const ValueId receiver = buildExpression(memberCallee->object, state);
                        if (!receiver)
                            return {};
                        Instruction instruction{
                            .opcode = memberCallee->object->is<SuperExpression>()
                                ? Opcode::MethodCall
                                : receiverOwner->nominalKind == NominalKind::Interface
                                    ? Opcode::InterfaceCall
                                    : receiverOwner->nominalKind == NominalKind::Object
                                        ? Opcode::VirtualCall
                                        : Opcode::MethodCall,
                            .operands = {receiver},
                            .callee = functionIt->second,
                            .selector = method->name,
                            .projectionIndex = method->slot,
                            .targetType = receiverNominalType,
                            .source = SourceSpan::at(expression->location())
                        };
                        instruction.signatureTypes.push_back(referenceType(receiverNominalType, method->receiverMutable));
                        const auto functionType = calleeSymbol->type &&
                            calleeSymbol->type->kind() == sema::TypeKind::Function
                            ? calleeSymbol->type.AsFast<sema::FunctionType>()
                            : nullptr;
                        for (std::size_t index = 0; index < call->arguments.size(); ++index)
                        {
                            const auto& argument = call->arguments[index];
                            const TypeId expectedType = functionType && index < functionType->paramTypes.size()
                                ? mapType(functionType->paramTypes[index], argument.Get())
                                : mapType(argument->refType.Lock(), argument.Get());
                            const ValueId value = buildExpressionAs(argument, expectedType, state);
                            if (!value)
                                return {};
                            instruction.operands.push_back(value);
                            instruction.signatureTypes.push_back(expectedType);
                        }
                        const TypeId resultType = mapType(expression->refType.Lock(), expression.Get());
                        instruction.genericArguments = genericArguments(
                            *call,
                            calleeSymbol,
                            call->callee->refType.Lock());
                        instruction.specializationKey = specializationKey(
                            instruction.callee,
                            instruction.genericArguments,
                            instruction.signatureTypes,
                            resultType);
                        decorateAsyncOperation(instruction);
                        const Type* type = result_.module_.types.tryGet(resultType);
                        if (type && type->kind == TypeKind::Void)
                        {
                            currentBlock(state).instructions.push_back(std::move(instruction));
                            releaseOwnedTemporary(receiver, expression.Get(), state);
                            return {};
                        }
                        const ValueId result = appendValue(std::move(instruction));
                        releaseOwnedTemporary(receiver, expression.Get(), state);
                        return result;
                    }
                }

                if (call->callee)
                {
                    if (const Ref<sema::Symbol> directSymbol = call->callee->referencedSymbol.Lock())
                        calleeSymbol = directSymbol;
                }
                const Ref<sema::Type> selectedCallableType = call->callee
                    ? call->callee->refType.Lock()
                    : nullptr;
                const auto visibleFunctionType = selectedCallableType &&
                    selectedCallableType->kind() == sema::TypeKind::Function
                    ? selectedCallableType.AsFast<sema::FunctionType>()
                    : nullptr;
                const bool isIndirect = !calleeSymbol ||
                    calleeSymbol->kind == sema::SymbolKind::Variable ||
                    calleeSymbol->kind == sema::SymbolKind::Parameter ||
                    (call->callee && call->callee->is<LambdaExpression>());
                if (isIndirect)
                {
                    const ValueId callableValue = buildExpression(call->callee, state);
                    if (!callableValue || !visibleFunctionType)
                    {
                        report("WIR2344", "Indirect call requires a resolved function value.", expression.Get());
                        return {};
                    }
                    Instruction instruction{
                        .opcode = Opcode::IndirectCall,
                        .operands = {callableValue},
                        .source = SourceSpan::at(expression->location())
                    };
                    instruction.signatureTypes.push_back(mapType(selectedCallableType, call->callee.Get()));
                    for (std::size_t index = 0; index < call->arguments.size(); ++index)
                    {
                        const auto& argument = call->arguments[index];
                        const TypeId expectedType = index < visibleFunctionType->paramTypes.size()
                            ? mapType(visibleFunctionType->paramTypes[index], argument.Get())
                            : mapType(argument->refType.Lock(), argument.Get());
                        const ValueId value = buildExpressionAs(argument, expectedType, state);
                        if (!value)
                            return {};
                        instruction.operands.push_back(value);
                        instruction.signatureTypes.push_back(expectedType);
                    }
                    const Type* type = result_.module_.types.tryGet(callResultType);
                    if (type && type->kind == TypeKind::Void)
                    {
                        currentBlock(state).instructions.push_back(std::move(instruction));
                        releaseOwnedTemporary(callableValue, expression.Get(), state);
                        return {};
                    }
                    const ValueId result = appendValue(std::move(instruction));
                    releaseOwnedTemporary(callableValue, expression.Get(), state);
                    return result;
                }

                calleeSymbol = resolveCallableSymbol(calleeSymbol, selectedCallableType);
                const auto functionIt = calleeSymbol
                    ? functionsBySymbol_.find(calleeSymbol.Get())
                    : functionsBySymbol_.end();
                if (functionIt == functionsBySymbol_.end())
                {
                    report("WIR2304", "Call target is not a function indexed by the initial Typed WIR slice.", expression.Get());
                    return {};
                }
                Instruction instruction{
                    .opcode = isNativeFunction(functionIt->second) ? Opcode::NativeCall : Opcode::Call,
                    .callee = functionIt->second,
                    .source = SourceSpan::at(expression->location())
                };
                for (std::size_t index = 0; index < call->arguments.size(); ++index)
                {
                    const auto& argument = call->arguments[index];
                    const TypeId expectedType = visibleFunctionType && index < visibleFunctionType->paramTypes.size()
                        ? mapType(visibleFunctionType->paramTypes[index], argument.Get())
                        : mapType(argument->refType.Lock(), argument.Get());
                    const ValueId value = buildExpressionAs(argument, expectedType, state);
                    if (!value)
                        return {};
                    instruction.operands.push_back(value);
                    instruction.signatureTypes.push_back(expectedType);
                }
                instruction.genericArguments = genericArguments(*call, calleeSymbol, selectedCallableType);
                instruction.specializationKey = specializationKey(
                    instruction.callee,
                    instruction.genericArguments,
                    instruction.signatureTypes,
                    callResultType);
                decorateAsyncOperation(instruction);
                const bool nativeCall = instruction.opcode == Opcode::NativeCall;
                const std::vector<ValueId> nativeArguments = nativeCall
                    ? instruction.operands : std::vector<ValueId>{};
                const Type* type = result_.module_.types.tryGet(callResultType);
                if (type && type->kind == TypeKind::Void)
                {
                    currentBlock(state).instructions.push_back(std::move(instruction));
                    for (const ValueId argument : nativeArguments)
                        releaseOwnedTemporary(argument, expression.Get(), state);
                    return {};
                }
                const ValueId result = appendValue(std::move(instruction));
                for (const ValueId argument : nativeArguments)
                    releaseOwnedTemporary(argument, expression.Get(), state);
                return result;
            }

            report("WIR2305", "Expression kind '" + getKindNameStr(expression->kind()) + "' is not supported by the initial Typed WIR slice.", expression.Get());
            return {};
        }

        ValueId buildPlace(
            const NodePtr<Expression>& expression,
            const bool needsMutable,
            FunctionState& state)
        {
            if (!expression)
                return {};

            if (expression->is<SelfExpression>())
            {
                if (!state.selfValue)
                {
                    report("WIR2331", "'self' is unavailable outside a receiver-aware WIR function.", expression.Get());
                    return {};
                }
                return adaptPlaceMutability(
                    state.selfValue,
                    state.selfType,
                    needsMutable,
                    expression.Get(),
                    state);
            }

            if (expression->is<SuperExpression>())
            {
                const ValueId value = buildExpression(expression, state);
                const TypeId type = mapType(expression->refType.Lock(), expression.Get());
                return value ? adaptPlaceMutability(value, type, needsMutable, expression.Get(), state) : ValueId{};
            }

            if (const auto* identifier = expression->as<Identifier>())
            {
                const Ref<sema::Symbol> symbol = identifier->referencedSymbol.Lock();
                if (!symbol)
                {
                    report("WIR2326", "Addressable identifier is missing its semantic symbol.", expression.Get());
                    return {};
                }
                if (const auto place = state.places.find(symbol.Get()); place != state.places.end())
                {
                    return adaptPlaceMutability(
                        place->second,
                        referenceType(mapType(symbol->type, expression.Get()), symbol->flags.get_isMutable()),
                        needsMutable,
                        expression.Get(),
                        state);
                }
                if (const auto value = state.values.find(symbol.Get()); value != state.values.end())
                {
                    const TypeId valueType = mapType(symbol->type, expression.Get());
                    return adaptPlaceMutability(value->second, valueType, needsMutable, expression.Get(), state);
                }
                report("WIR2327", "Addressable identifier is not available in the current Typed WIR function.", expression.Get());
                return {};
            }

            if (const auto* unary = expression->as<UnaryExpression>();
                unary && unary->op.type == TokenType::kwDeref)
            {
                const ValueId place = buildExpression(unary->operand, state);
                const TypeId placeType = mapType(unary->operand->refType.Lock(), unary->operand.Get());
                return place ? adaptPlaceMutability(place, placeType, needsMutable, expression.Get(), state) : ValueId{};
            }

            if (const auto* access = expression->as<ArrayAccessExpression>())
            {
                if (access->operatorDispatchKind != OperatorDispatchKind::None)
                {
                    report("WIR2328", "Overloaded index places are not yet supported by Typed WIR.", expression.Get());
                    return {};
                }
                TypeId containerType = mapType(access->object->refType.Lock(), access->object.Get());
                const Type* container = result_.module_.types.tryGet(containerType);
                if (container && container->kind == TypeKind::Reference && container->arguments.size() == 1)
                {
                    containerType = container->arguments.front();
                    container = result_.module_.types.tryGet(containerType);
                }
                if (!container || (container->kind != TypeKind::Array &&
                    container->kind != TypeKind::Dictionary))
                {
                    report("WIR2328", "Only array and dictionary index expressions are addressable WIR places.", expression.Get());
                    return {};
                }
                const ValueId base = buildPlace(access->object, needsMutable, state);
                const ValueId index = buildAutoReadableExpression(access->index, state);
                if (!base || !index)
                    return {};
                const ValueId result{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = container->kind == TypeKind::Dictionary
                        ? Opcode::DictionaryPlace
                        : Opcode::ArrayPlace,
                    .result = result,
                    .resultType = referenceType(mapType(expression->refType.Lock(), expression.Get()), needsMutable),
                    .operands = {base, index},
                    .source = SourceSpan::at(expression->location())
                });
                return result;
            }

            if (const auto* member = expression->as<MemberAccessExpression>())
            {
                const Ref<sema::Symbol> memberSymbol = member->referencedSymbol.Lock();
                if (!memberSymbol || memberSymbol->kind != sema::SymbolKind::Variable)
                {
                    report("WIR2329", "Addressable member must resolve to a data field.", expression.Get());
                    return {};
                }
                const ValueId base = buildPlace(member->object, needsMutable, state);
                if (!base)
                    return {};
                const ValueId result{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::FieldPlace,
                    .result = result,
                    .resultType = referenceType(mapType(expression->refType.Lock(), expression.Get()), needsMutable),
                    .operands = {base},
                    .selector = memberSymbol->name,
                    .source = SourceSpan::at(expression->location())
                });
                return result;
            }

            if (const auto* call = expression->as<FunctionCallExpression>())
            {
                const TypeId callType = mapType(expression->refType.Lock(), expression.Get());
                const Type* callTypeInfo = result_.module_.types.tryGet(callType);
                if (callTypeInfo && callTypeInfo->kind == TypeKind::Reference)
                {
                    const ValueId place = buildExpression(expression, state);
                    return place ? adaptPlaceMutability(place, callType, needsMutable, expression.Get(), state) : ValueId{};
                }
            }

            report("WIR2330", "Expression kind '" + getKindNameStr(expression->kind()) + "' is not an addressable Typed WIR place.", expression.Get());
            return {};
        }

        ValueId buildAutoReadableExpression(
            const NodePtr<Expression>& expression,
            FunctionState& state)
        {
            ValueId value = buildExpression(expression, state);
            TypeId currentType = mapType(expression->refType.Lock(), expression.Get());
            const Type* type = result_.module_.types.tryGet(currentType);
            while (value && type && autoReadableReference(*type))
            {
                currentType = type->arguments.front();
                value = emitLoad(value, currentType, expression.Get(), state);
                type = result_.module_.types.tryGet(currentType);
            }
            return value;
        }

        TypeId mapExpressionType(const NodePtr<Expression>& expression, const ASTNode* source)
        {
            Ref<sema::Type> semanticType = expression ? expression->refType.Lock() : nullptr;
            const auto primitive = semanticType && semanticType->kind() == sema::TypeKind::Primitive
                ? semanticType.AsFast<sema::PrimitiveType>()
                : nullptr;
            if (primitive && primitive->name == "<unknown>")
            {
                const auto* call = expression->as<FunctionCallExpression>();
                const Ref<sema::Type> callableType = call && call->callee
                    ? call->callee->refType.Lock()
                    : nullptr;
                if (callableType && callableType->kind() == sema::TypeKind::Function)
                    semanticType = callableType.AsFast<sema::FunctionType>()->returnType;
            }
            return mapType(semanticType, source);
        }

        ValueId buildExpressionAs(
            const NodePtr<Expression>& expression,
            const TypeId destinationType,
            FunctionState& state)
        {
            // Ref expressions are contextual: semantic analysis intentionally
            // leaves their placeholder type unresolved and the selected
            // parameter supplies view/ref mutability. Do not leak that
            // placeholder into WIR; materialize the canonical destination
            // place directly.
            if (const auto* reference = expression ? expression->as<RefExpression>() : nullptr)
            {
                const Type* destination = result_.module_.types.tryGet(destinationType);
                if (destination && destination->kind == TypeKind::Reference &&
                    destination->arguments.size() == 1)
                {
                    return buildPlace(reference->operand, destination->isMutable, state);
                }
            }
            ValueId value = buildExpression(expression, state);
            if (!value)
                return {};
            TypeId sourceType = mapExpressionType(expression, expression.Get());
            if (sourceType == destinationType)
                return ensureOwned(value, destinationType, expression.Get(), state);

            const Type* source = result_.module_.types.tryGet(sourceType);
            const Type* destination = result_.module_.types.tryGet(destinationType);
            if (source && destination && source->kind == TypeKind::Reference &&
                destination->kind == TypeKind::Reference && source->arguments == destination->arguments &&
                source->isMutable && !destination->isMutable)
            {
                const ValueId borrowed{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::Borrow,
                    .result = borrowed,
                    .resultType = destinationType,
                    .operands = {value},
                    .source = SourceSpan::at(expression->location())
                });
                return borrowed;
            }
            TypeId sourceNominalType;
            TypeId destinationNominalType;
            if (underlyingNominalType(sourceType, &sourceNominalType) &&
                underlyingNominalType(destinationType, &destinationNominalType) &&
                nominalDerivesFrom(sourceNominalType, destinationNominalType))
            {
                const ValueId upcast{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::Upcast,
                    .result = upcast,
                    .resultType = destinationType,
                    .operands = {value},
                    .targetType = destinationType,
                    .resultOwnership = valueOwnership(state, value),
                    .borrowLifetime = valueOwnership(state, value) == ValueOwnership::Borrowed
                        ? BorrowLifetime::Caller : BorrowLifetime::None,
                    .source = SourceSpan::at(expression->location())
                });
                rememberOwnership(state, upcast, valueOwnership(state, value));
                return ensureOwned(upcast, destinationType, expression.Get(), state);
            }
            while (source && autoReadableReference(*source) && sourceType != destinationType)
            {
                sourceType = source->arguments.front();
                value = emitLoad(value, sourceType, expression.Get(), state);
                source = result_.module_.types.tryGet(sourceType);
            }
            if (sourceType == destinationType)
                return ensureOwned(value, destinationType, expression.Get(), state);

            if (destination && destination->kind == TypeKind::Any)
            {
                value = ensureOwned(value, sourceType, expression.Get(), state);
                const ValueId boxed{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::AnyBox,
                    .result = boxed,
                    .resultType = destinationType,
                    .operands = {value},
                    .signatureTypes = {sourceType},
                    .targetType = sourceType,
                    .resultOwnership = ValueOwnership::Owned,
                    .source = SourceSpan::at(expression->location())
                });
                rememberOwnership(state, boxed, ValueOwnership::Owned);
                return boxed;
            }

            if (source && destination && destination->kind == TypeKind::Nullable &&
                destination->arguments.size() == 1 && destination->arguments.front() == sourceType)
            {
                value = ensureOwned(value, sourceType, expression.Get(), state);
                const ValueId wrapped{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::NullableWrap,
                    .result = wrapped,
                    .resultType = destinationType,
                    .operands = {value},
                    .targetType = destinationType,
                    .resultOwnership = ownershipForType(destinationType),
                    .source = SourceSpan::at(expression->location())
                });
                rememberOwnership(state, wrapped, ownershipForType(destinationType));
                return wrapped;
            }

            if (!source || !destination ||
                !isSafeNumericWiden(source->kind, destination->kind))
            {
                report(
                    "WIR2314",
                    "Expression requires a conversion that is not a safe implicit numeric widening.",
                    expression.Get());
                return {};
            }

            const ValueId converted{state.nextValue++};
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::Convert,
                .result = converted,
                .resultType = destinationType,
                .operands = {value},
                .conversionKind = ConversionKind::NumericWiden,
                .source = SourceSpan::at(expression->location())
            });
            return converted;
        }

        ValueId buildDefaultValue(
            const TypeId typeId,
            const ASTNode* source,
            FunctionState& state)
        {
            const Type* type = result_.module_.types.tryGet(typeId);
            if (!type)
                return {};

            Literal literal;
            switch (type->kind)
            {
            case TypeKind::Bool: literal = false; break;
            case TypeKind::I8:
            case TypeKind::I16:
            case TypeKind::I32:
            case TypeKind::I64:
            case TypeKind::ISize: literal = std::int64_t{0}; break;
            case TypeKind::U8:
            case TypeKind::U16:
            case TypeKind::U32:
            case TypeKind::U64:
            case TypeKind::USize:
            case TypeKind::Byte:
            case TypeKind::Char: literal = std::uint64_t{0}; break;
            case TypeKind::F32:
            case TypeKind::F64: literal = 0.0; break;
            case TypeKind::String:
            case TypeKind::Text: literal = std::string{}; break;
            case TypeKind::Any: literal = NullLiteral{}; break;
            default:
                report(
                    "WIR2203",
                    "Default initialization for type '" + std::string(typeKindName(type->kind)) +
                        "' is not supported by Typed WIR.",
                    source);
                return {};
            }

            const ValueId result{state.nextValue++};
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::Constant,
                .result = result,
                .resultType = typeId,
                .literal = std::move(literal),
                .source = source ? SourceSpan::at(source->location()) : SourceSpan{}
            });
            return result;
        }

        ValueId appendBinaryValue(
            const BinaryOperator op,
            const ValueId left,
            const ValueId right,
            const TypeId resultType,
            const ASTNode& source,
            FunctionState& state)
        {
            const ValueId result{state.nextValue++};
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::Binary,
                .result = result,
                .resultType = resultType,
                .operands = {left, right},
                .binaryOperator = op,
                .source = SourceSpan::at(source.location())
            });
            return result;
        }

        ValueId buildDestructuringMatchTest(
            const MatchCase& matchCase,
            const ValueId subject,
            const ASTNode& source,
            FunctionState& state)
        {
            if (matchCase.variantName == "__array")
            {
                const TypeId usizeType = result_.module_.types.intern(Type{.kind = TypeKind::USize});
                const ValueId length{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::ArrayLength,
                    .result = length,
                    .resultType = usizeType,
                    .operands = {subject},
                    .source = SourceSpan::at(source.location())
                });
                const ValueId expected{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::Constant,
                    .result = expected,
                    .resultType = usizeType,
                    .literal = static_cast<std::uint64_t>(matchCase.bindings.size()),
                    .source = SourceSpan::at(source.location())
                });
                return appendBinaryValue(
                    BinaryOperator::Equal,
                    length,
                    expected,
                    result_.module_.types.boolType(),
                    source,
                    state);
            }

            if (matchCase.variantName != "Some" && matchCase.variantName != "None" &&
                matchCase.variantName != "Ok" && matchCase.variantName != "Err")
            {
                report(
                    "WIR2316",
                    "Typed WIR does not recognize destructuring variant '" + matchCase.variantName + "'.",
                    &source);
                return {};
            }

            const ValueId result{state.nextValue++};
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::VariantTest,
                .result = result,
                .resultType = result_.module_.types.boolType(),
                .operands = {subject},
                .selector = matchCase.variantName,
                .source = SourceSpan::at(source.location())
            });
            return result;
        }

        bool buildMatchBindings(
            const MatchCase& matchCase,
            const ValueId subject,
            FunctionState& state,
            std::unordered_map<const sema::Symbol*, ValueId>* captured = nullptr)
        {
            for (std::size_t index = 0; index < matchCase.bindings.size(); ++index)
            {
                const auto& binding = matchCase.bindings[index];
                const Ref<sema::Symbol> symbol = binding ? binding->referencedSymbol.Lock() : nullptr;
                if (!symbol)
                {
                    report("WIR2319", "Match binding is missing its semantic symbol.", binding.Get());
                    return false;
                }

                const TypeId bindingType = mapType(symbol->type, binding.Get());
                if (!bindingType)
                    return false;
                const ValueId value{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = matchCase.variantName == "__array"
                        ? Opcode::ArrayElement
                        : Opcode::VariantPayload,
                    .result = value,
                    .resultType = bindingType,
                    .operands = {subject},
                    .selector = matchCase.variantName,
                    .projectionIndex = static_cast<std::uint32_t>(index),
                    .source = SourceSpan::at(binding->location())
                });
                state.values[symbol.Get()] = value;
                if (captured)
                    (*captured)[symbol.Get()] = value;
            }
            return true;
        }

        void buildMatchCaseTests(
            const MatchCase& matchCase,
            const ValueId subject,
            const TypeId subjectType,
            const BlockId successTarget,
            const BlockId failureTarget,
            FunctionState& state)
        {
            for (std::size_t index = 0; index < matchCase.matchValues.size(); ++index)
            {
                const auto& pattern = matchCase.matchValues[index];
                const bool hasNextPattern = index + 1 < matchCase.matchValues.size();
                const std::size_t nextPatternIndex = hasNextPattern
                    ? createBlock(state, "match.pattern.next", SourceSpan::at(pattern->location()))
                    : state.blockIndex;
                const BlockId patternFailure = hasNextPattern
                    ? currentBlockAt(state, nextPatternIndex).id
                    : failureTarget;

                if (const auto* range = pattern->as<RangeExpression>())
                {
                    const ValueId start = buildExpressionAs(range->start, subjectType, state);
                    if (!start)
                        return;
                    const ValueId lower = appendBinaryValue(
                        BinaryOperator::GreaterEqual,
                        subject,
                        start,
                        result_.module_.types.boolType(),
                        *pattern,
                        state);
                    const std::size_t upperBlockIndex = createBlock(
                        state, "match.range.upper", SourceSpan::at(range->end->location()));
                    const BlockId upperBlock = currentBlockAt(state, upperBlockIndex).id;
                    currentBlock(state).instructions.push_back(Instruction{
                        .opcode = Opcode::CondBranch,
                        .operands = {lower},
                        .targets = {upperBlock, patternFailure},
                        .source = SourceSpan::at(pattern->location())
                    });

                    state.blockIndex = upperBlockIndex;
                    const ValueId end = buildExpressionAs(range->end, subjectType, state);
                    if (!end)
                        return;
                    const ValueId upper = appendBinaryValue(
                        range->isInclusive ? BinaryOperator::LessEqual : BinaryOperator::Less,
                        subject,
                        end,
                        result_.module_.types.boolType(),
                        *pattern,
                        state);
                    currentBlock(state).instructions.push_back(Instruction{
                        .opcode = Opcode::CondBranch,
                        .operands = {upper},
                        .targets = {successTarget, patternFailure},
                        .source = SourceSpan::at(pattern->location())
                    });
                }
                else
                {
                    const ValueId expected = buildExpressionAs(pattern, subjectType, state);
                    if (!expected)
                        return;
                    const ValueId equal = appendBinaryValue(
                        BinaryOperator::Equal,
                        subject,
                        expected,
                        result_.module_.types.boolType(),
                        *pattern,
                        state);
                    currentBlock(state).instructions.push_back(Instruction{
                        .opcode = Opcode::CondBranch,
                        .operands = {equal},
                        .targets = {successTarget, patternFailure},
                        .source = SourceSpan::at(pattern->location())
                    });
                }

                if (hasNextPattern)
                    state.blockIndex = nextPatternIndex;
            }
        }

        ValueId buildMatchExpression(const MatchExpression& expression, FunctionState& state)
        {
            if (expression.cases.empty())
            {
                report("WIR2315", "Match expression requires at least one case.", &expression);
                return {};
            }
            const TypeId resultType = mapType(expression.refType.Lock(), &expression);
            const Type* resultTypeInfo = result_.module_.types.tryGet(resultType);
            if (!resultTypeInfo)
                return {};
            const bool producesValue = resultTypeInfo->kind != TypeKind::Void;
            if (producesValue)
            {
                for (const MatchCase& matchCase : expression.cases)
                {
                    if (!matchCase.body || !matchCase.body->is<ExpressionStatement>())
                    {
                        report("WIR2317", "Value-producing Typed WIR match cases require expression bodies.", &expression);
                        return {};
                    }
                }
            }

            const ValueId subject = buildExpression(expression.value, state);
            if (!subject)
                return {};
            const TypeId subjectType = mapType(expression.value->refType.Lock(), expression.value.Get());
            const auto incomingValues = state.values;
            const auto incomingOrder = state.valueOrder;
            const std::size_t subjectBlockIndex = state.blockIndex;

            std::vector<std::size_t> caseEntryIndices;
            std::vector<std::size_t> caseBodyIndices;
            caseEntryIndices.reserve(expression.cases.size());
            caseBodyIndices.reserve(expression.cases.size());
            for (std::size_t index = 0; index < expression.cases.size(); ++index)
            {
                caseEntryIndices.push_back(createBlock(
                    state, "match.case." + std::to_string(index) + ".test", SourceSpan::at(expression.location())));
                caseBodyIndices.push_back(createBlock(
                    state, "match.case." + std::to_string(index) + ".body", SourceSpan::at(expression.cases[index].body->location())));
            }
            const std::size_t unmatchedBlockIndex = createBlock(
                state, "match.unmatched", SourceSpan::at(expression.location()));
            const std::size_t mergeBlockIndex = createBlock(
                state, "match.merge", SourceSpan::at(expression.location()));
            const BlockId mergeBlock = currentBlockAt(state, mergeBlockIndex).id;
            ValueId result;
            if (producesValue)
            {
                result = ValueId{state.nextValue++};
                currentBlockAt(state, mergeBlockIndex).parameters.push_back(Parameter{
                    .id = result,
                    .name = "match.result",
                    .type = resultType,
                    .ownership = ownershipForType(resultType),
                    .borrowLifetime = ownershipForType(resultType) == ValueOwnership::Borrowed
                        ? BorrowLifetime::Caller : BorrowLifetime::None,
                    .source = SourceSpan::at(expression.location())
                });
                rememberOwnership(state, result, ownershipForType(resultType));
            }
            const bool hasAssumed = std::ranges::any_of(
                expression.cases,
                [](const MatchCase& matchCase)
                {
                    return matchCase.matchValues.empty() && matchCase.variantName.empty();
                });
            const auto hasUnguardedVariant = [&](const std::string_view name)
            {
                return std::ranges::any_of(
                    expression.cases,
                    [&](const MatchCase& matchCase)
                    {
                        return matchCase.variantName == name && !matchCase.guard;
                    });
            };
            const bool hasExhaustiveDestructuring =
                (hasUnguardedVariant("Some") && hasUnguardedVariant("None")) ||
                (hasUnguardedVariant("Ok") && hasUnguardedVariant("Err"));
            const bool closesUnmatchedPath = hasAssumed || hasExhaustiveDestructuring;
            if (producesValue || closesUnmatchedPath)
            {
                currentBlockAt(state, unmatchedBlockIndex).instructions.push_back(Instruction{
                    .opcode = Opcode::Unreachable,
                    .source = SourceSpan::at(expression.location())
                });
            }
            currentBlockAt(state, subjectBlockIndex).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .targets = {currentBlockAt(state, caseEntryIndices.front()).id},
                .source = SourceSpan::at(expression.location())
            });

            std::vector<FunctionState> fallthroughStates;
            std::vector<ValueId> pathResults;
            for (std::size_t index = 0; index < expression.cases.size(); ++index)
            {
                const MatchCase& matchCase = expression.cases[index];
                const BlockId bodyBlock = currentBlockAt(state, caseBodyIndices[index]).id;
                const BlockId nextCase = index + 1 < expression.cases.size()
                    ? currentBlockAt(state, caseEntryIndices[index + 1]).id
                    : currentBlockAt(state, unmatchedBlockIndex).id;
                const bool destructuring = !matchCase.variantName.empty();
                const bool assumed = matchCase.matchValues.empty() && !destructuring;
                std::unordered_map<const sema::Symbol*, ValueId> caseBindings;

                FunctionState testState = state;
                testState.blockIndex = caseEntryIndices[index];
                testState.values = incomingValues;
                testState.valueOrder = incomingOrder;
                if (assumed)
                {
                    currentBlock(testState).instructions.push_back(Instruction{
                        .opcode = Opcode::Branch,
                        .targets = {bodyBlock},
                        .source = SourceSpan::at(matchCase.body->location())
                    });
                }
                else
                {
                    BlockId successTarget = bodyBlock;
                    std::optional<std::size_t> guardBlockIndex;
                    if (matchCase.guard)
                    {
                        guardBlockIndex = createBlock(
                            testState, "match.case." + std::to_string(index) + ".guard", SourceSpan::at(matchCase.guard->location()));
                        successTarget = currentBlockAt(testState, *guardBlockIndex).id;
                    }
                    if (destructuring)
                    {
                        const ValueId matched = buildDestructuringMatchTest(
                            matchCase, subject, *matchCase.body.Get(), testState);
                        if (!matched)
                            return {};
                        currentBlock(testState).instructions.push_back(Instruction{
                            .opcode = Opcode::CondBranch,
                            .operands = {matched},
                            .targets = {successTarget, nextCase},
                            .source = SourceSpan::at(matchCase.body->location())
                        });
                    }
                    else
                    {
                        buildMatchCaseTests(
                            matchCase, subject, subjectType, successTarget, nextCase, testState);
                    }
                    if (guardBlockIndex)
                    {
                        testState.blockIndex = *guardBlockIndex;
                        if (destructuring &&
                            !buildMatchBindings(matchCase, subject, testState, &caseBindings))
                        {
                            return {};
                        }
                        const ValueId guard = buildExpression(matchCase.guard, testState);
                        if (!guard)
                            return {};
                        currentBlock(testState).instructions.push_back(Instruction{
                            .opcode = Opcode::CondBranch,
                            .operands = {guard},
                            .targets = {bodyBlock, nextCase},
                            .source = SourceSpan::at(matchCase.guard->location())
                        });
                    }
                }
                state.nextValue = testState.nextValue;
                state.nextBlock = testState.nextBlock;

                FunctionState bodyState = state;
                bodyState.blockIndex = caseBodyIndices[index];
                bodyState.values = incomingValues;
                bodyState.valueOrder = incomingOrder;
                if (destructuring)
                {
                    if (matchCase.guard)
                    {
                        bodyState.values.insert(caseBindings.begin(), caseBindings.end());
                    }
                    else if (!buildMatchBindings(matchCase, subject, bodyState))
                    {
                        return {};
                    }
                }
                if (producesValue)
                {
                    const auto* expressionBody = matchCase.body->as<ExpressionStatement>();
                    const ValueId bodyValue = buildExpressionAs(expressionBody->expression, resultType, bodyState);
                    if (!bodyValue)
                        return {};
                    fallthroughStates.push_back(bodyState);
                    pathResults.push_back(bodyValue);
                }
                else
                {
                    buildStatement(matchCase.body, bodyState);
                    if (!blockIsTerminated(bodyState))
                        fallthroughStates.push_back(bodyState);
                }
                state.nextValue = bodyState.nextValue;
                state.nextBlock = bodyState.nextBlock;
            }

            if (!producesValue && !closesUnmatchedPath)
            {
                FunctionState unmatchedState = state;
                unmatchedState.blockIndex = unmatchedBlockIndex;
                unmatchedState.values = incomingValues;
                unmatchedState.valueOrder = incomingOrder;
                fallthroughStates.push_back(std::move(unmatchedState));
            }

            if (fallthroughStates.empty())
            {
                currentBlockAt(state, mergeBlockIndex).instructions.push_back(Instruction{
                    .opcode = Opcode::Unreachable,
                    .source = SourceSpan::at(expression.location())
                });
                state.blockIndex = mergeBlockIndex;
                state.values = incomingValues;
                state.valueOrder = incomingOrder;
                return {};
            }

            BasicBlock& merge = currentBlockAt(state, mergeBlockIndex);
            auto mergedValues = incomingValues;
            std::vector<const sema::Symbol*> mergedSymbols;
            for (const sema::Symbol* symbol : incomingOrder)
            {
                const ValueId incoming = incomingValues.at(symbol);
                const auto valueIn = [symbol, incoming](const FunctionState& path)
                {
                    const auto found = path.values.find(symbol);
                    return found != path.values.end() ? found->second : incoming;
                };
                const ValueId firstValue = valueIn(fallthroughStates.front());
                const bool differs = std::ranges::any_of(
                    fallthroughStates,
                    [&](const FunctionState& path) { return valueIn(path) != firstValue; });
                if (!differs)
                {
                    mergedValues[symbol] = firstValue;
                    continue;
                }

                const ValueId merged{state.nextValue++};
                merge.parameters.push_back(Parameter{
                    .id = merged,
                    .name = symbol->name + ".match",
                    .type = mapType(symbol->type, &expression),
                    .ownership = ownershipForType(mapType(symbol->type, &expression)),
                    .borrowLifetime = ownershipForType(mapType(symbol->type, &expression)) == ValueOwnership::Borrowed
                        ? BorrowLifetime::Caller : BorrowLifetime::None,
                    .source = SourceSpan::at(expression.location())
                });
                rememberOwnership(state, merged, ownershipForType(mapType(symbol->type, &expression)));
                mergedSymbols.push_back(symbol);
                mergedValues[symbol] = merged;
            }

            for (std::size_t index = 0; index < fallthroughStates.size(); ++index)
            {
                FunctionState& path = fallthroughStates[index];
                std::vector<ValueId> arguments;
                arguments.reserve(mergedSymbols.size() + (producesValue ? 1 : 0));
                if (producesValue)
                    arguments.push_back(pathResults[index]);
                for (const sema::Symbol* symbol : mergedSymbols)
                {
                    const auto found = path.values.find(symbol);
                    arguments.push_back(found != path.values.end()
                        ? found->second
                        : incomingValues.at(symbol));
                }
                currentBlock(path).instructions.push_back(Instruction{
                    .opcode = Opcode::Branch,
                    .operands = std::move(arguments),
                    .targets = {mergeBlock},
                    .source = SourceSpan::at(expression.location())
                });
            }

            state.blockIndex = mergeBlockIndex;
            state.values = std::move(mergedValues);
            state.valueOrder = incomingOrder;
            return result;
        }

        ValueId buildConditionalControlFlow(
            const ConditionalExpression& expression,
            FunctionState& state)
        {
            const ValueId condition = buildExpression(expression.condition, state);
            if (!condition)
                return {};

            const auto incomingValues = state.values;
            const auto incomingOrder = state.valueOrder;
            const std::size_t conditionBlockIndex = state.blockIndex;
            const std::size_t trueBlockIndex = createBlock(
                state, "conditional.true", SourceSpan::at(expression.whenTrue->location()));
            const std::size_t falseBlockIndex = createBlock(
                state, "conditional.false", SourceSpan::at(expression.whenFalse->location()));
            const std::size_t mergeBlockIndex = createBlock(
                state, "conditional.merge", SourceSpan::at(expression.location()));
            const BlockId trueBlock = currentBlockAt(state, trueBlockIndex).id;
            const BlockId falseBlock = currentBlockAt(state, falseBlockIndex).id;
            const BlockId mergeBlock = currentBlockAt(state, mergeBlockIndex).id;

            currentBlockAt(state, conditionBlockIndex).instructions.push_back(Instruction{
                .opcode = Opcode::CondBranch,
                .operands = {condition},
                .targets = {trueBlock, falseBlock},
                .source = SourceSpan::at(expression.condition->location())
            });

            const TypeId resultType = mapType(expression.refType.Lock(), &expression);
            FunctionState trueState = state;
            trueState.blockIndex = trueBlockIndex;
            trueState.values = incomingValues;
            trueState.valueOrder = incomingOrder;
            const ValueId whenTrue = buildExpressionAs(expression.whenTrue, resultType, trueState);
            if (!whenTrue)
                return {};
            state.nextValue = trueState.nextValue;
            state.nextBlock = trueState.nextBlock;

            FunctionState falseState = state;
            falseState.blockIndex = falseBlockIndex;
            falseState.values = incomingValues;
            falseState.valueOrder = incomingOrder;
            const ValueId whenFalse = buildExpressionAs(expression.whenFalse, resultType, falseState);
            if (!whenFalse)
                return {};
            state.nextValue = falseState.nextValue;
            state.nextBlock = falseState.nextBlock;

            BasicBlock& merge = currentBlockAt(state, mergeBlockIndex);
            const ValueId result{state.nextValue++};
            merge.parameters.push_back(Parameter{
                .id = result,
                .name = "conditional.result",
                .type = resultType,
                .ownership = ownershipForType(resultType),
                .borrowLifetime = ownershipForType(resultType) == ValueOwnership::Borrowed
                    ? BorrowLifetime::Caller : BorrowLifetime::None,
                .source = SourceSpan::at(expression.location())
            });
            rememberOwnership(state, result, ownershipForType(resultType));
            std::vector<ValueId> trueArguments{whenTrue};
            std::vector<ValueId> falseArguments{whenFalse};
            auto mergedValues = incomingValues;

            for (const sema::Symbol* symbol : incomingOrder)
            {
                const ValueId incoming = incomingValues.at(symbol);
                const ValueId trueValue = trueState.values.contains(symbol)
                    ? trueState.values.at(symbol)
                    : incoming;
                const ValueId falseValue = falseState.values.contains(symbol)
                    ? falseState.values.at(symbol)
                    : incoming;
                if (trueValue == falseValue)
                    continue;

                const ValueId merged{state.nextValue++};
                merge.parameters.push_back(Parameter{
                    .id = merged,
                    .name = symbol->name + ".conditional",
                    .type = mapType(symbol->type, &expression),
                    .ownership = ownershipForType(mapType(symbol->type, &expression)),
                    .borrowLifetime = ownershipForType(mapType(symbol->type, &expression)) == ValueOwnership::Borrowed
                        ? BorrowLifetime::Caller : BorrowLifetime::None,
                    .source = SourceSpan::at(expression.location())
                });
                rememberOwnership(state, merged, ownershipForType(mapType(symbol->type, &expression)));
                trueArguments.push_back(trueValue);
                falseArguments.push_back(falseValue);
                mergedValues[symbol] = merged;
            }

            currentBlock(trueState).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = std::move(trueArguments),
                .targets = {mergeBlock},
                .source = SourceSpan::at(expression.whenTrue->location())
            });
            currentBlock(falseState).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = std::move(falseArguments),
                .targets = {mergeBlock},
                .source = SourceSpan::at(expression.whenFalse->location())
            });

            state.blockIndex = mergeBlockIndex;
            state.values = std::move(mergedValues);
            state.valueOrder = incomingOrder;
            return result;
        }

        ValueId buildShortCircuitExpression(
            const BinaryExpression& expression,
            const bool logicalAnd,
            FunctionState& state)
        {
            const ValueId left = buildExpression(expression.left, state);
            if (!left)
                return {};

            const auto incomingValues = state.values;
            const auto incomingOrder = state.valueOrder;
            const std::size_t conditionBlockIndex = state.blockIndex;
            const std::string prefix = logicalAnd ? "logical.and" : "logical.or";
            const std::size_t rightBlockIndex = createBlock(
                state, prefix + ".rhs", SourceSpan::at(expression.right->location()));
            const std::size_t shortBlockIndex = createBlock(
                state, prefix + ".short", SourceSpan::at(expression.left->location()));
            const std::size_t mergeBlockIndex = createBlock(
                state, prefix + ".merge", SourceSpan::at(expression.location()));

            const BlockId rightBlock = currentBlockAt(state, rightBlockIndex).id;
            const BlockId shortBlock = currentBlockAt(state, shortBlockIndex).id;
            const BlockId mergeBlock = currentBlockAt(state, mergeBlockIndex).id;
            currentBlockAt(state, conditionBlockIndex).instructions.push_back(Instruction{
                .opcode = Opcode::CondBranch,
                .operands = {left},
                .targets = logicalAnd
                    ? std::vector<BlockId>{rightBlock, shortBlock}
                    : std::vector<BlockId>{shortBlock, rightBlock},
                .source = SourceSpan::at(expression.location())
            });

            FunctionState rightState = state;
            rightState.blockIndex = rightBlockIndex;
            rightState.values = incomingValues;
            rightState.valueOrder = incomingOrder;
            const ValueId right = buildExpression(expression.right, rightState);
            if (!right)
                return {};
            state.nextValue = rightState.nextValue;
            state.nextBlock = rightState.nextBlock;

            FunctionState shortState = state;
            shortState.blockIndex = shortBlockIndex;
            shortState.values = incomingValues;
            shortState.valueOrder = incomingOrder;
            const ValueId shortValue{shortState.nextValue++};
            currentBlock(shortState).instructions.push_back(Instruction{
                .opcode = Opcode::Constant,
                .result = shortValue,
                .resultType = result_.module_.types.boolType(),
                .literal = !logicalAnd,
                .source = SourceSpan::at(expression.left->location())
            });
            state.nextValue = shortState.nextValue;
            state.nextBlock = shortState.nextBlock;

            BasicBlock& merge = currentBlockAt(state, mergeBlockIndex);
            const ValueId result{state.nextValue++};
            merge.parameters.push_back(Parameter{
                .id = result,
                .name = "logical.result",
                .type = mapType(expression.refType.Lock(), &expression),
                .ownership = ownershipForType(mapType(expression.refType.Lock(), &expression)),
                .borrowLifetime = ownershipForType(mapType(expression.refType.Lock(), &expression)) == ValueOwnership::Borrowed
                    ? BorrowLifetime::Caller : BorrowLifetime::None,
                .source = SourceSpan::at(expression.location())
            });
            rememberOwnership(state, result, ownershipForType(mapType(expression.refType.Lock(), &expression)));

            std::vector<ValueId> rightArguments{right};
            std::vector<ValueId> shortArguments{shortValue};
            auto mergedValues = incomingValues;
            for (const sema::Symbol* symbol : incomingOrder)
            {
                const ValueId incoming = incomingValues.at(symbol);
                const ValueId rightValue = rightState.values.contains(symbol)
                    ? rightState.values.at(symbol)
                    : incoming;
                if (rightValue == incoming)
                    continue;

                const ValueId merged{state.nextValue++};
                merge.parameters.push_back(Parameter{
                    .id = merged,
                    .name = symbol->name + ".logical",
                    .type = mapType(symbol->type, &expression),
                    .ownership = ownershipForType(mapType(symbol->type, &expression)),
                    .borrowLifetime = ownershipForType(mapType(symbol->type, &expression)) == ValueOwnership::Borrowed
                        ? BorrowLifetime::Caller : BorrowLifetime::None,
                    .source = SourceSpan::at(expression.location())
                });
                rememberOwnership(state, merged, ownershipForType(mapType(symbol->type, &expression)));
                rightArguments.push_back(rightValue);
                shortArguments.push_back(incoming);
                mergedValues[symbol] = merged;
            }

            currentBlock(rightState).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = std::move(rightArguments),
                .targets = {mergeBlock},
                .source = SourceSpan::at(expression.right->location())
            });
            currentBlock(shortState).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = std::move(shortArguments),
                .targets = {mergeBlock},
                .source = SourceSpan::at(expression.left->location())
            });

            state.blockIndex = mergeBlockIndex;
            state.values = std::move(mergedValues);
            state.valueOrder = incomingOrder;
            return result;
        }

        void buildIfStatement(const IfStatement& statement, FunctionState& state)
        {
            const ValueId condition = buildExpression(statement.condition, state);
            if (!condition)
                return;

            const auto incomingValues = state.values;
            const auto incomingOrder = state.valueOrder;
            const std::size_t conditionBlockIndex = state.blockIndex;
            const std::size_t thenBlockIndex = createBlock(
                state, "if.then", SourceSpan::at(statement.thenBranch->location()));
            const std::size_t elseBlockIndex = createBlock(
                state,
                statement.elseBranch ? "if.else" : "if.else.passthrough",
                SourceSpan::at(statement.elseBranch ? statement.elseBranch->location() : statement.location()));
            currentBlockAt(state, conditionBlockIndex).instructions.push_back(Instruction{
                .opcode = Opcode::CondBranch,
                .operands = {condition},
                .targets = {
                    currentBlockAt(state, thenBlockIndex).id,
                    currentBlockAt(state, elseBlockIndex).id
                },
                .source = SourceSpan::at(statement.location())
            });

            FunctionState thenState = state;
            thenState.blockIndex = thenBlockIndex;
            thenState.values = incomingValues;
            thenState.valueOrder = incomingOrder;
            buildStatement(statement.thenBranch, thenState);
            state.nextValue = thenState.nextValue;
            state.nextBlock = thenState.nextBlock;

            FunctionState elseState = state;
            elseState.blockIndex = elseBlockIndex;
            elseState.values = incomingValues;
            elseState.valueOrder = incomingOrder;
            if (statement.elseBranch)
                buildStatement(statement.elseBranch, elseState);
            state.nextValue = elseState.nextValue;
            state.nextBlock = elseState.nextBlock;

            const bool thenFallsThrough = !blockIsTerminated(thenState);
            const bool elseFallsThrough = !blockIsTerminated(elseState);
            if (!thenFallsThrough && !elseFallsThrough)
            {
                state.blockIndex = thenState.blockIndex;
                state.values = incomingValues;
                state.valueOrder = incomingOrder;
                return;
            }

            const std::size_t mergeBlockIndex = createBlock(
                state, "if.merge", SourceSpan::at(statement.location()));
            BasicBlock& mergeBlock = currentBlockAt(state, mergeBlockIndex);
            std::vector<ValueId> thenArguments;
            std::vector<ValueId> elseArguments;
            auto mergedValues = incomingValues;

            for (const sema::Symbol* symbol : incomingOrder)
            {
                const ValueId incoming = incomingValues.at(symbol);
                const ValueId thenValue = thenState.values.contains(symbol)
                    ? thenState.values.at(symbol)
                    : incoming;
                const ValueId elseValue = elseState.values.contains(symbol)
                    ? elseState.values.at(symbol)
                    : incoming;

                if (thenFallsThrough && elseFallsThrough && thenValue != elseValue)
                {
                    const ValueId merged{state.nextValue++};
                    mergeBlock.parameters.push_back(Parameter{
                        .id = merged,
                        .name = symbol->name,
                        .type = mapType(symbol->type, &statement),
                        .ownership = ownershipForType(mapType(symbol->type, &statement)),
                        .borrowLifetime = ownershipForType(mapType(symbol->type, &statement)) == ValueOwnership::Borrowed
                            ? BorrowLifetime::Caller : BorrowLifetime::None,
                        .source = SourceSpan::at(statement.location())
                    });
                    rememberOwnership(state, merged, ownershipForType(mapType(symbol->type, &statement)));
                    thenArguments.push_back(thenValue);
                    elseArguments.push_back(elseValue);
                    mergedValues[symbol] = merged;
                }
                else if (thenFallsThrough)
                    mergedValues[symbol] = thenValue;
                else
                    mergedValues[symbol] = elseValue;
            }

            if (thenFallsThrough)
            {
                currentBlock(thenState).instructions.push_back(Instruction{
                    .opcode = Opcode::Branch,
                    .operands = std::move(thenArguments),
                    .targets = {mergeBlock.id},
                    .source = SourceSpan::at(statement.thenBranch->location())
                });
            }
            if (elseFallsThrough)
            {
                currentBlock(elseState).instructions.push_back(Instruction{
                    .opcode = Opcode::Branch,
                    .operands = std::move(elseArguments),
                    .targets = {mergeBlock.id},
                    .source = SourceSpan::at(statement.elseBranch ? statement.elseBranch->location() : statement.location())
                });
            }

            state.blockIndex = mergeBlockIndex;
            state.values = std::move(mergedValues);
            state.valueOrder = incomingOrder;
        }

        std::unordered_map<const sema::Symbol*, ValueId> addBlockParameters(
            FunctionState& state,
            const std::size_t blockIndex,
            const std::vector<const sema::Symbol*>& symbols,
            const ASTNode& source,
            const std::string_view nameSuffix)
        {
            auto values = state.values;
            BasicBlock& block = currentBlockAt(state, blockIndex);
            for (const sema::Symbol* symbol : symbols)
            {
                const ValueId value{state.nextValue++};
                block.parameters.push_back(Parameter{
                    .id = value,
                    .name = symbol->name + std::string(nameSuffix),
                    .type = mapType(symbol->type, &source),
                    .ownership = ownershipForType(mapType(symbol->type, &source)),
                    .borrowLifetime = ownershipForType(mapType(symbol->type, &source)) == ValueOwnership::Borrowed
                        ? BorrowLifetime::Caller : BorrowLifetime::None,
                    .source = SourceSpan::at(source.location())
                });
                rememberOwnership(state, value, ownershipForType(mapType(symbol->type, &source)));
                values[symbol] = value;
            }
            return values;
        }

        std::vector<ValueId> collectCarriedValues(
            const std::unordered_map<const sema::Symbol*, ValueId>& availableValues,
            const std::vector<const sema::Symbol*>& symbols,
            const ASTNode* source)
        {
            std::vector<ValueId> values;
            values.reserve(symbols.size());
            for (const sema::Symbol* symbol : symbols)
            {
                const auto found = availableValues.find(symbol);
                if (found == availableValues.end())
                {
                    report("WIR2204", "Loop-carried local value is unavailable on a control-flow edge.", source);
                    values.push_back(ValueId{});
                }
                else
                    values.push_back(found->second);
            }
            return values;
        }

        void buildLoopTransfer(
            const ASTNode* statement,
            const bool isContinue,
            FunctionState& state)
        {
            if (loopContexts_.empty())
            {
                report(
                    isContinue ? "WIR2206" : "WIR2205",
                    isContinue
                        ? "Continue statement has no active Typed WIR loop target."
                        : "Break statement has no active Typed WIR loop target.",
                    statement);
                return;
            }

            const LoopContext& loop = loopContexts_.back();
            emitDropsFrom(loop.placeDepth, state, statement);
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = collectCarriedValues(state.values, loop.carriedSymbols, statement),
                .targets = {isContinue ? loop.continueTarget : loop.breakTarget},
                .source = statement ? SourceSpan::at(statement->location()) : SourceSpan{}
            });
        }

        void buildWhileStatement(const WhileStatement& statement, FunctionState& state)
        {
            const auto carriedSymbols = state.valueOrder;
            const auto incomingValues = state.values;
            const std::size_t preheaderBlockIndex = state.blockIndex;
            const std::size_t headerBlockIndex = createBlock(
                state, "while.header", SourceSpan::at(statement.location()));
            const std::size_t bodyBlockIndex = createBlock(
                state, "while.body", SourceSpan::at(statement.body->location()));
            const std::size_t conditionExitBlockIndex = createBlock(
                state, "while.condition-exit", SourceSpan::at(statement.condition->location()));
            const std::size_t exitBlockIndex = createBlock(
                state, "while.exit", SourceSpan::at(statement.location()));

            const auto headerValues = addBlockParameters(
                state, headerBlockIndex, carriedSymbols, statement, ".loop");
            const auto exitValues = addBlockParameters(
                state, exitBlockIndex, carriedSymbols, statement, ".after");

            currentBlockAt(state, preheaderBlockIndex).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = collectCarriedValues(incomingValues, carriedSymbols, &statement),
                .targets = {currentBlockAt(state, headerBlockIndex).id},
                .source = SourceSpan::at(statement.location())
            });

            state.blockIndex = headerBlockIndex;
            state.values = headerValues;
            state.valueOrder = carriedSymbols;
            const ValueId condition = buildExpression(statement.condition, state);
            if (!condition)
                return;

            const auto conditionValues = state.values;
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::CondBranch,
                .operands = {condition},
                .targets = {
                    currentBlockAt(state, bodyBlockIndex).id,
                    currentBlockAt(state, conditionExitBlockIndex).id
                },
                .source = SourceSpan::at(statement.condition->location())
            });
            currentBlockAt(state, conditionExitBlockIndex).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = collectCarriedValues(conditionValues, carriedSymbols, &statement),
                .targets = {currentBlockAt(state, exitBlockIndex).id},
                .source = SourceSpan::at(statement.condition->location())
            });

            FunctionState bodyState = state;
            bodyState.blockIndex = bodyBlockIndex;
            bodyState.values = conditionValues;
            bodyState.valueOrder = carriedSymbols;
            loopContexts_.push_back(LoopContext{
                .continueTarget = currentBlockAt(state, headerBlockIndex).id,
                .breakTarget = currentBlockAt(state, exitBlockIndex).id,
                .carriedSymbols = carriedSymbols,
                .placeDepth = state.placeOrder.size()
            });
            buildStatement(statement.body, bodyState);
            loopContexts_.pop_back();
            state.nextValue = bodyState.nextValue;
            state.nextBlock = bodyState.nextBlock;

            if (!blockIsTerminated(bodyState))
            {
                currentBlock(bodyState).instructions.push_back(Instruction{
                    .opcode = Opcode::Branch,
                    .operands = collectCarriedValues(bodyState.values, carriedSymbols, &statement),
                    .targets = {currentBlockAt(state, headerBlockIndex).id},
                    .source = SourceSpan::at(statement.body->location())
                });
            }

            state.blockIndex = exitBlockIndex;
            state.values = exitValues;
            state.valueOrder = carriedSymbols;
        }

        void buildCForStatement(const CForStatement& statement, FunctionState& state)
        {
            const std::size_t outerValueCount = state.valueOrder.size();
            const std::size_t outerPlaceCount = state.placeOrder.size();
            if (statement.initializer)
                buildStatement(statement.initializer, state);
            if (blockIsTerminated(state))
                return;

            const auto carriedSymbols = state.valueOrder;
            const auto incomingValues = state.values;
            const std::size_t preheaderBlockIndex = state.blockIndex;
            const std::size_t headerBlockIndex = createBlock(
                state, "for.header", SourceSpan::at(statement.location()));
            const std::size_t bodyBlockIndex = createBlock(
                state, "for.body", SourceSpan::at(statement.body->location()));
            const std::size_t incrementBlockIndex = createBlock(
                state, "for.increment", SourceSpan::at(
                    statement.increment ? statement.increment->location() : statement.location()));
            const std::size_t conditionExitBlockIndex = createBlock(
                state, "for.condition-exit", SourceSpan::at(
                    statement.condition ? statement.condition->location() : statement.location()));
            const std::size_t exitBlockIndex = createBlock(
                state, "for.exit", SourceSpan::at(statement.location()));

            const auto headerValues = addBlockParameters(
                state, headerBlockIndex, carriedSymbols, statement, ".loop");
            const auto incrementValues = addBlockParameters(
                state, incrementBlockIndex, carriedSymbols, statement, ".next");
            const auto exitValues = addBlockParameters(
                state, exitBlockIndex, carriedSymbols, statement, ".after");

            currentBlockAt(state, preheaderBlockIndex).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = collectCarriedValues(incomingValues, carriedSymbols, &statement),
                .targets = {currentBlockAt(state, headerBlockIndex).id},
                .source = SourceSpan::at(statement.location())
            });

            state.blockIndex = headerBlockIndex;
            state.values = headerValues;
            state.valueOrder = carriedSymbols;
            ValueId condition;
            if (statement.condition)
                condition = buildExpression(statement.condition, state);
            else
            {
                condition = ValueId{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::Constant,
                    .result = condition,
                    .resultType = result_.module_.types.boolType(),
                    .literal = true,
                    .source = SourceSpan::at(statement.location())
                });
            }
            if (!condition)
                return;

            const auto conditionValues = state.values;
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::CondBranch,
                .operands = {condition},
                .targets = {
                    currentBlockAt(state, bodyBlockIndex).id,
                    currentBlockAt(state, conditionExitBlockIndex).id
                },
                .source = SourceSpan::at(
                    statement.condition ? statement.condition->location() : statement.location())
            });
            currentBlockAt(state, conditionExitBlockIndex).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = collectCarriedValues(conditionValues, carriedSymbols, &statement),
                .targets = {currentBlockAt(state, exitBlockIndex).id},
                .source = SourceSpan::at(statement.location())
            });

            FunctionState bodyState = state;
            bodyState.blockIndex = bodyBlockIndex;
            bodyState.values = conditionValues;
            bodyState.valueOrder = carriedSymbols;
            loopContexts_.push_back(LoopContext{
                .continueTarget = currentBlockAt(state, incrementBlockIndex).id,
                .breakTarget = currentBlockAt(state, exitBlockIndex).id,
                .carriedSymbols = carriedSymbols,
                .placeDepth = state.placeOrder.size()
            });
            buildStatement(statement.body, bodyState);
            loopContexts_.pop_back();
            state.nextValue = bodyState.nextValue;
            state.nextBlock = bodyState.nextBlock;
            if (!blockIsTerminated(bodyState))
            {
                currentBlock(bodyState).instructions.push_back(Instruction{
                    .opcode = Opcode::Branch,
                    .operands = collectCarriedValues(bodyState.values, carriedSymbols, &statement),
                    .targets = {currentBlockAt(state, incrementBlockIndex).id},
                    .source = SourceSpan::at(statement.body->location())
                });
            }

            FunctionState incrementState = state;
            incrementState.blockIndex = incrementBlockIndex;
            incrementState.values = incrementValues;
            incrementState.valueOrder = carriedSymbols;
            if (statement.increment)
                buildExpression(statement.increment, incrementState);
            if (!blockIsTerminated(incrementState))
            {
                currentBlock(incrementState).instructions.push_back(Instruction{
                    .opcode = Opcode::Branch,
                    .operands = collectCarriedValues(incrementState.values, carriedSymbols, &statement),
                    .targets = {currentBlockAt(state, headerBlockIndex).id},
                    .source = SourceSpan::at(
                        statement.increment ? statement.increment->location() : statement.location())
                });
            }
            state.nextValue = incrementState.nextValue;
            state.nextBlock = incrementState.nextBlock;

            state.blockIndex = exitBlockIndex;
            state.values = exitValues;
            state.valueOrder = carriedSymbols;
            emitDropsFrom(outerPlaceCount, state, &statement);
            while (state.placeOrder.size() > outerPlaceCount)
            {
                state.places.erase(state.placeOrder.back());
                state.placeOrder.pop_back();
            }
            while (state.valueOrder.size() > outerValueCount)
            {
                state.values.erase(state.valueOrder.back());
                state.valueOrder.pop_back();
            }
        }

        static BasicBlock& currentBlockAt(FunctionState& state, const std::size_t index)
        {
            return state.function->blocks.at(index);
        }

        static bool isSideEffectFree(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return true;
            if (expression->is<IntegerLiteral>() || expression->is<FloatLiteral>() ||
                expression->is<StringLiteral>() || expression->is<BoolLiteral>() ||
                expression->is<CharLiteral>() || expression->is<ByteLiteral>() ||
                expression->is<Identifier>())
            {
                return true;
            }
            if (const auto* unary = expression->as<UnaryExpression>())
                return isSideEffectFree(unary->operand);
            if (const auto* binary = expression->as<BinaryExpression>())
                return isSideEffectFree(binary->left) && isSideEffectFree(binary->right);
            if (const auto* conditional = expression->as<ConditionalExpression>())
            {
                return isSideEffectFree(conditional->condition) &&
                       isSideEffectFree(conditional->whenTrue) &&
                       isSideEffectFree(conditional->whenFalse);
            }
            return false;
        }

        static std::optional<UnaryOperator> mapUnaryOperator(const TokenType type)
        {
            switch (type)
            {
            case TokenType::opMinus: return UnaryOperator::Negate;
            case TokenType::opLogicalNot:
            case TokenType::kwNot: return UnaryOperator::LogicalNot;
            case TokenType::opBitNot: return UnaryOperator::BitwiseNot;
            default: return std::nullopt;
            }
        }

        static bool isLogicalAnd(const TokenType type)
        {
            return type == TokenType::opLogicalAnd || type == TokenType::kwAnd;
        }

        static bool isLogicalOr(const TokenType type)
        {
            return type == TokenType::opLogicalOr || type == TokenType::kwOr;
        }

        static std::optional<IntegerType> mapIntegerType(const TypeKind kind)
        {
            switch (kind)
            {
            case TypeKind::I8: return IntegerType::i8;
            case TypeKind::I16: return IntegerType::i16;
            case TypeKind::I32: return IntegerType::i32;
            case TypeKind::I64: return IntegerType::i64;
            case TypeKind::ISize: return IntegerType::isize;
            case TypeKind::U8:
            case TypeKind::Byte: return IntegerType::u8;
            case TypeKind::U16: return IntegerType::u16;
            case TypeKind::U32: return IntegerType::u32;
            case TypeKind::U64: return IntegerType::u64;
            case TypeKind::USize: return IntegerType::usize;
            default: return std::nullopt;
            }
        }

        static std::optional<FloatType> mapFloatType(const TypeKind kind)
        {
            if (kind == TypeKind::F32)
                return FloatType::f32;
            if (kind == TypeKind::F64)
                return FloatType::f64;
            return std::nullopt;
        }

        static bool isNumericType(const TypeKind kind)
        {
            return mapIntegerType(kind).has_value() || mapFloatType(kind).has_value();
        }

        struct NumericTypeInfo
        {
            int bits;
            bool isSigned;
            bool isFloat;
        };

        static std::optional<NumericTypeInfo> numericTypeInfo(const TypeKind kind)
        {
            switch (kind)
            {
            case TypeKind::I8: return NumericTypeInfo{8, true, false};
            case TypeKind::I16: return NumericTypeInfo{16, true, false};
            case TypeKind::I32: return NumericTypeInfo{32, true, false};
            case TypeKind::I64:
            case TypeKind::ISize: return NumericTypeInfo{64, true, false};
            case TypeKind::U8: return NumericTypeInfo{8, false, false};
            case TypeKind::U16: return NumericTypeInfo{16, false, false};
            case TypeKind::U32: return NumericTypeInfo{32, false, false};
            case TypeKind::U64:
            case TypeKind::USize: return NumericTypeInfo{64, false, false};
            case TypeKind::F32: return NumericTypeInfo{32, true, true};
            case TypeKind::F64: return NumericTypeInfo{64, true, true};
            default: return std::nullopt;
            }
        }

        static bool isSafeNumericWiden(const TypeKind sourceKind, const TypeKind destinationKind)
        {
            if (sourceKind == destinationKind)
                return true;
            const auto source = numericTypeInfo(sourceKind);
            const auto destination = numericTypeInfo(destinationKind);
            if (!source || !destination)
                return false;
            if (destination->isFloat)
            {
                if (source->isFloat)
                    return destination->bits >= source->bits;
                const int exactIntegerBits = destination->bits == 32 ? 24 : 53;
                return source->bits - (source->isSigned ? 1 : 0) <= exactIntegerBits;
            }
            if (source->isFloat)
                return false;
            if (destination->isSigned == source->isSigned)
                return destination->bits >= source->bits;
            return destination->isSigned && !source->isSigned && destination->bits > source->bits;
        }

        static std::optional<Literal> integerLiteral(const IntegerResult& value)
        {
            if (!value.isValid)
                return std::nullopt;
            switch (value.type)
            {
            case IntegerType::i8: return Literal{static_cast<std::int64_t>(value.value.v_i8)};
            case IntegerType::i16: return Literal{static_cast<std::int64_t>(value.value.v_i16)};
            case IntegerType::i32: return Literal{static_cast<std::int64_t>(value.value.v_i32)};
            case IntegerType::i64: return Literal{static_cast<std::int64_t>(value.value.v_i64)};
            case IntegerType::isize: return Literal{static_cast<std::int64_t>(value.value.v_isize)};
            case IntegerType::u8: return Literal{static_cast<std::uint64_t>(value.value.v_u8)};
            case IntegerType::u16: return Literal{static_cast<std::uint64_t>(value.value.v_u16)};
            case IntegerType::u32: return Literal{static_cast<std::uint64_t>(value.value.v_u32)};
            case IntegerType::u64: return Literal{static_cast<std::uint64_t>(value.value.v_u64)};
            case IntegerType::usize: return Literal{static_cast<std::uint64_t>(value.value.v_usize)};
            case IntegerType::Unknown: return std::nullopt;
            }
            return std::nullopt;
        }

        static std::optional<BinaryOperator> mapBinaryOperator(const TokenType type)
        {
            switch (type)
            {
            case TokenType::opPlus: return BinaryOperator::Add;
            case TokenType::opMinus: return BinaryOperator::Subtract;
            case TokenType::opStar: return BinaryOperator::Multiply;
            case TokenType::opSlash: return BinaryOperator::Divide;
            case TokenType::opPercent: return BinaryOperator::Remainder;
            case TokenType::opEqual: return BinaryOperator::Equal;
            case TokenType::opNotEqual: return BinaryOperator::NotEqual;
            case TokenType::opLess: return BinaryOperator::Less;
            case TokenType::opLessEqual: return BinaryOperator::LessEqual;
            case TokenType::opGreater: return BinaryOperator::Greater;
            case TokenType::opGreaterEqual: return BinaryOperator::GreaterEqual;
            case TokenType::opBitAnd: return BinaryOperator::BitwiseAnd;
            case TokenType::opBitOr: return BinaryOperator::BitwiseOr;
            case TokenType::opBitXor: return BinaryOperator::BitwiseXor;
            case TokenType::opShiftLeft: return BinaryOperator::ShiftLeft;
            case TokenType::opShiftRight: return BinaryOperator::ShiftRight;
            default: return std::nullopt;
            }
        }

        static std::optional<BinaryOperator> mapCompoundAssignmentOperator(const TokenType type)
        {
            switch (type)
            {
            case TokenType::opPlusAssign: return BinaryOperator::Add;
            case TokenType::opMinusAssign: return BinaryOperator::Subtract;
            case TokenType::opStarAssign: return BinaryOperator::Multiply;
            case TokenType::opSlashAssign: return BinaryOperator::Divide;
            case TokenType::opPercentAssign: return BinaryOperator::Remainder;
            case TokenType::opBitAndAssign: return BinaryOperator::BitwiseAnd;
            case TokenType::opBitOrAssign: return BinaryOperator::BitwiseOr;
            case TokenType::opBitXorAssign: return BinaryOperator::BitwiseXor;
            case TokenType::opShiftLeftAssign: return BinaryOperator::ShiftLeft;
            case TokenType::opShiftRightAssign: return BinaryOperator::ShiftRight;
            default: return std::nullopt;
            }
        }
    };

    BuildResult Builder::build(const Ref<Program>& program, const BuildOptions& options) const
    {
        BuildResult result;
        BuildContext{result, options}.build(program);
        return result;
    }
}
