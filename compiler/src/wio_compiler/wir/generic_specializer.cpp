#include "wio/wir/generic_specializer.h"
#include "wio/wir/native_abi_types.h"

#include <algorithm>
#include <charconv>
#include <functional>
#include <map>
#include <set>
#include <sstream>

namespace wio::wir
{
    namespace
    {
        using namespace typed;
        using Bindings = std::map<TypeId, TypeId>;

        struct ExpandedPackValue
        {
            std::vector<ValueId> values;
            std::vector<TypeId> types;
        };

        bool parameterKind(TypeKind kind)
        {
            return kind == TypeKind::GenericParameter || kind == TypeKind::ConstGenericParameter ||
                   kind == TypeKind::GenericParameterPack;
        }

        class Materializer
        {
        public:
            Materializer(Module& module, std::size_t maximumBodies) : module_(module), maximumBodies_(maximumBodies)
            {
                for (const Function& function : module.functions)
                {
                    templates_.emplace(function.id, function);
                    if (function.ownerType && function.parameters.size() == 1 && !function.isExternal &&
                        (function.name == "OnDestruct" || function.name == "OnConstruct" ||
                         function.name.ends_with("::OnDestruct") || function.name.ends_with("::OnConstruct")))
                        lifecycleTemplates_[module.types.get(function.ownerType).name].push_back(function.id);
                    nextId_ = std::max(nextId_, function.id.value() + 1);
                    if (function.genericOrigin && !function.specializationKey.empty())
                    {
                        instances_.emplace(function.specializationKey, function.id);
                        instanceKeys_.emplace(function.id, function.specializationKey);
                    }
                }
            }

            std::vector<SpecializationDiagnostic> run()
            {
                for (auto& entry : module_.contract.exports)
                {
                    if (entry.kind != ModuleExportKind::GenericFunctionSpecialization)
                        continue;
                    const auto source = templates_.find(entry.function);
                    if (source == templates_.end() || !openFunction(source->second) ||
                        (source->second.isExternal && !source->second.nativeBinding))
                        continue;
                    Instruction request;
                    request.callee = entry.function;
                    request.genericArguments = entry.genericArguments;
                    request.signatureTypes = entry.parameterTypes;
                    request.resultType = entry.returnType;
                    entry.function = instantiate(request);
                }
                // Appending a body grows this worklist. Cache entries are registered
                // before visiting it, so self/mutual recursion closes on existing IDs.
                materializeMethods();
                for (std::size_t index = 0; index < module_.functions.size() && diagnostics_.empty(); ++index)
                {
                    if (openFunction(module_.functions[index]))
                        continue;
                    Function function = module_.functions[index];
                    for (auto& block : function.blocks)
                        for (auto& instruction : block.instructions)
                        {
                            if (!instruction.callee)
                                continue;
                            const auto source = templates_.find(instruction.callee);
                            if (source == templates_.end() ||
                                (source->second.isExternal && !source->second.nativeBinding) ||
                                !openFunction(source->second))
                                continue;
                            instruction.callee = instantiate(instruction);
                            if (const auto key = instanceKeys_.find(instruction.callee); key != instanceKeys_.end())
                                instruction.specializationKey = key->second;
                        }
                    module_.functions[index] = std::move(function);
                    materializeMethods();
                }
                if (diagnostics_.empty())
                    for (Function& function : module_.functions)
                        materializeOwnedLoads(function);
                return std::move(diagnostics_);
            }

        private:
            void materializeMethods()
            {
                for (std::size_t i = 0; i < module_.types.size() && diagnostics_.empty(); ++i)
                {
                    TypeId id{static_cast<TypeId::ValueType>(i)};
                    if (openType(id))
                        continue;
                    auto methods = module_.types.get(id).methods;
                    if (const auto found = processedMethods_.find(id);
                        found != processedMethods_.end() && found->second == methods)
                        continue;
                    for (auto& method : methods)
                    {
                        const auto source = templates_.find(method.function);
                        if (method.isAbstract || source == templates_.end() || source->second.isExternal ||
                            !openFunction(source->second))
                            continue;
                        TypeId owner = id;
                        const std::string ownerName = module_.types.get(source->second.ownerType).name;
                        std::set<TypeId> visited;
                        std::function<TypeId(TypeId)> findOwner = [&](TypeId candidate) -> TypeId
                        {
                            if (!visited.insert(candidate).second)
                                return {};
                            const auto type = module_.types.get(candidate);
                            if (type.name == ownerName)
                                return candidate;
                            for (TypeId base : type.baseTypes)
                                if (TypeId found = findOwner(base))
                                    return found;
                            return {};
                        };
                        owner = findOwner(id);
                        if (!owner)
                            continue;

                        Bindings ownerBindings;
                        if (!bind(source->second.ownerType, owner, ownerBindings))
                            continue;
                        Bindings cache;
                        method.returnType = substitute(method.returnType, ownerBindings, cache);
                        for (TypeId& parameter : method.parameterTypes)
                            parameter = substitute(parameter, ownerBindings, cache);
                        if (openType(method.returnType) ||
                            std::ranges::any_of(method.parameterTypes,
                                                [&](const TypeId parameter) { return openType(parameter); }))
                            continue;

                        Type receiver = module_.types.get(source->second.parameters.front().type);
                        receiver.arguments = {owner};
                        Instruction request;
                        request.callee = method.function;
                        request.signatureTypes = {module_.types.intern(std::move(receiver))};
                        request.signatureTypes.insert(request.signatureTypes.end(), method.parameterTypes.begin(),
                                                      method.parameterTypes.end());
                        request.resultType = method.returnType;
                        method.function = instantiate(request);
                    }
                    module_.types.getMutable(id).methods = std::move(methods);
                    processedMethods_[id] = module_.types.get(id).methods;
                    const auto concrete = module_.types.get(id);
                    if (concrete.nominalKind != NominalKind::Object)
                        continue;
                    for (const auto functionId : lifecycleTemplates_[concrete.name])
                    {
                        const auto& source = templates_.at(functionId);
                        if (!source.ownerType || source.parameters.size() != 1 || source.isExternal ||
                            !openFunction(source) || module_.types.get(source.ownerType).name != concrete.name ||
                            (source.name != "OnDestruct" && source.name != "OnConstruct" &&
                             !source.name.ends_with("::OnDestruct") && !source.name.ends_with("::OnConstruct")))
                            continue;
                        Type receiver = module_.types.get(source.parameters.front().type);
                        receiver.arguments = {id};
                        Instruction request;
                        request.callee = functionId;
                        request.signatureTypes = {module_.types.intern(std::move(receiver))};
                        request.resultType = module_.types.voidType();
                        (void)instantiate(request);
                    }
                }
            }

            Module& module_;
            std::map<FunctionId, Function> templates_;
            std::map<std::string, std::vector<FunctionId>> lifecycleTemplates_;
            std::map<TypeId, std::vector<MethodLayout>> processedMethods_;
            std::map<std::string, FunctionId> instances_;
            std::map<FunctionId, std::string> instanceKeys_;
            std::vector<SpecializationDiagnostic> diagnostics_;
            FunctionId::ValueType nextId_ = 0;
            std::size_t maximumBodies_;

            bool openType(TypeId id, std::set<TypeId>& visiting) const
            {
                const Type* type = module_.types.tryGet(id);
                if (!type || !visiting.insert(id).second)
                    return false;
                if (parameterKind(type->kind))
                    return true;
                if ((type->kind == TypeKind::ValuePack || type->kind == TypeKind::TypePack) && type->arguments.empty())
                    return !type->name.empty();
                return openType(type->extentParameter, visiting) ||
                       std::ranges::any_of(type->arguments,
                                           [&](TypeId nested) { return openType(nested, visiting); }) ||
                       std::ranges::any_of(type->baseTypes,
                                           [&](TypeId nested) { return openType(nested, visiting); }) ||
                       std::ranges::any_of(type->fields,
                                           [&](const FieldLayout& field) { return openType(field.type, visiting); });
            }
            bool openType(TypeId id) const
            {
                std::set<TypeId> visiting;
                return openType(id, visiting);
            }
            bool openFunction(const Function& function) const
            {
                return !function.genericParameters.empty() || openType(function.returnType) ||
                       openType(function.ownerType) ||
                       std::ranges::any_of(function.parameters, [&](const Parameter& p) { return openType(p.type); });
            }
            void fail(std::string message, SourceSpan source)
            {
                diagnostics_.push_back({"WIR3100", std::move(message), source});
            }
            bool bind(TypeId pattern, TypeId actual, Bindings& bindings)
            {
                if (!module_.types.tryGet(pattern) || !module_.types.tryGet(actual))
                    return pattern == actual;
                // Recursive binding may intern normalized constants; do not keep
                // pointers into the growable type table across recursive calls.
                const Type patternType = module_.types.get(pattern), actualType = module_.types.get(actual);
                const Type* p = &patternType;
                const Type* a = &actualType;
                if (parameterKind(p->kind))
                {
                    if (p->kind == TypeKind::ConstGenericParameter && a->kind == TypeKind::ConstValue &&
                        p->arguments.size() == 1)
                    {
                        Type normalized = *a;
                        normalized.arguments = p->arguments;
                        actual = module_.types.intern(std::move(normalized));
                    }
                    const auto [it, inserted] = bindings.emplace(pattern, actual);
                    return inserted || it->second == actual;
                }
                if (p->kind != a->kind || p->arguments.size() != a->arguments.size())
                    return false;
                if (p->extentParameter)
                {
                    if (!a->staticExtent)
                        return false;
                    Type constant{.kind = TypeKind::ConstValue,
                                  .name = std::to_string(*a->staticExtent),
                                  .arguments = module_.types.get(p->extentParameter).arguments};
                    if (!bind(p->extentParameter, module_.types.intern(std::move(constant)), bindings))
                        return false;
                }
                else if (p->staticExtent != a->staticExtent)
                    return false;
                if (p->kind == TypeKind::Named && p->name != a->name)
                    return false;
                if (p->arguments.empty())
                    return pattern == actual;
                for (std::size_t i = 0; i < p->arguments.size(); ++i)
                    if (!bind(p->arguments[i], a->arguments[i], bindings))
                        return false;
                return true;
            }

            TypeId substitute(TypeId id, const Bindings& bindings, Bindings& cache)
            {
                if (!id)
                    return id;
                if (const auto it = bindings.find(id); it != bindings.end())
                    return it->second;
                if (const auto it = cache.find(id); it != cache.end())
                    return it->second;
                Type type = module_.types.get(id);
                if (!openType(id))
                    return id;
                for (TypeId& argument : type.arguments)
                    argument = substitute(argument, bindings, cache);
                if (type.extentParameter)
                {
                    const TypeId extent = substitute(type.extentParameter, bindings, cache);
                    const Type constant = module_.types.get(extent);
                    std::size_t value = 0;
                    const auto parsed =
                        std::from_chars(constant.name.data(), constant.name.data() + constant.name.size(), value);
                    if (constant.kind == TypeKind::ConstValue && parsed.ec == std::errc{} &&
                        parsed.ptr == constant.name.data() + constant.name.size())
                    {
                        type.staticExtent = value;
                        type.extentParameter = {};
                    }
                }
                TypeId result;
                if (type.kind == TypeKind::Named)
                {
                    result = module_.types.internNominal(type);
                    cache[id] = result;
                }
                for (TypeId& base : type.baseTypes)
                    base = substitute(base, bindings, cache);
                for (FieldLayout& field : type.fields)
                    field.type = substitute(field.type, bindings, cache);
                for (MethodLayout& method : type.methods)
                {
                    method.returnType = substitute(method.returnType, bindings, cache);
                    for (TypeId& p : method.parameterTypes)
                        p = substitute(p, bindings, cache);
                }
                if (result)
                {
                    // An existing concrete layout from semantic analysis is the
                    // canonical identity; never overwrite its resolved fields.
                    if (openType(result))
                        module_.types.getMutable(result) = std::move(type);
                }
                else
                    result = module_.types.intern(std::move(type));
                cache[id] = result;
                return result;
            }

            ValueOwnership ownership(TypeId id, ValueOwnership old) const
            {
                const Type& type = module_.types.get(id);
                if (type.ownership == OwnershipModel::Trivial)
                    return ValueOwnership::Trivial;
                if (type.ownership == OwnershipModel::Borrowed)
                    return ValueOwnership::Borrowed;
                return old == ValueOwnership::Borrowed ? old : ValueOwnership::Owned;
            }

            const Type* boundPack(const TypeId id, const Bindings& bindings) const
            {
                const auto found = bindings.find(id);
                if (found == bindings.end())
                    return nullptr;
                const Type* pack = module_.types.tryGet(found->second);
                return pack && (pack->kind == TypeKind::TypePack || pack->kind == TypeKind::ValuePack ||
                                pack->kind == TypeKind::PackStorage)
                           ? pack
                           : nullptr;
            }

            bool expandPackParameters(Function& function, const Bindings& bindings)
            {
                ValueId::ValueType nextValue = 0;
                const auto observe = [&](const ValueId value)
                {
                    if (value)
                        nextValue = std::max(nextValue, static_cast<ValueId::ValueType>(value.value() + 1));
                };
                for (const Parameter& parameter : function.parameters)
                    observe(parameter.id);
                for (const BasicBlock& block : function.blocks)
                {
                    for (const Parameter& parameter : block.parameters)
                        observe(parameter.id);
                    for (const Instruction& instruction : block.instructions)
                        observe(instruction.result);
                }

                std::map<ValueId, ExpandedPackValue> expansions;
                std::vector<Parameter> parameters;
                for (const Parameter& parameter : function.parameters)
                {
                    const Type* parameterType = module_.types.tryGet(parameter.type);
                    const Type* pack = parameterType && parameterType->kind == TypeKind::GenericParameterPack
                                           ? boundPack(parameter.type, bindings)
                                           : nullptr;
                    if (!pack)
                    {
                        parameters.push_back(parameter);
                        continue;
                    }

                    ExpandedPackValue expansion;
                    expansion.types = pack->arguments;
                    for (std::size_t index = 0; index < pack->arguments.size(); ++index)
                    {
                        const TypeId type = pack->arguments[index];
                        const ValueId value{nextValue++};
                        expansion.values.push_back(value);
                        parameters.push_back(Parameter{.id = value,
                                                       .name = parameter.name + "[" + std::to_string(index) + "]",
                                                       .type = type,
                                                       .ownership = ownership(type, parameter.ownership),
                                                       .borrowLifetime = parameter.borrowLifetime,
                                                       .source = parameter.source});
                    }
                    expansions.emplace(parameter.id, std::move(expansion));
                }
                function.parameters = std::move(parameters);

                const auto referencesPack = [&](const Instruction& instruction)
                {
                    return std::ranges::any_of(instruction.operands,
                                               [&](const ValueId operand) { return expansions.contains(operand); });
                };
                for (BasicBlock& block : function.blocks)
                {
                    std::vector<Instruction> instructions;
                    for (Instruction instruction : block.instructions)
                    {
                        if (instruction.opcode == Opcode::LocalPlace)
                        {
                            const Type* reference = module_.types.tryGet(instruction.resultType);
                            const Type* pack =
                                reference && reference->kind == TypeKind::Reference && reference->arguments.size() == 1
                                    ? boundPack(reference->arguments.front(), bindings)
                                    : nullptr;
                            if (pack)
                            {
                                expansions[instruction.result] = ExpandedPackValue{.types = pack->arguments};
                                continue;
                            }
                        }
                        if (instruction.opcode == Opcode::PlaceInit && instruction.operands.size() == 2 &&
                            expansions.contains(instruction.operands[0]) &&
                            expansions.contains(instruction.operands[1]))
                        {
                            expansions[instruction.operands[0]] = expansions.at(instruction.operands[1]);
                            continue;
                        }
                        if (instruction.opcode == Opcode::Load && instruction.operands.size() == 1 &&
                            expansions.contains(instruction.operands.front()))
                        {
                            expansions[instruction.result] = expansions.at(instruction.operands.front());
                            continue;
                        }
                        if ((instruction.opcode == Opcode::Copy || instruction.opcode == Opcode::Move) &&
                            instruction.operands.size() == 1 && expansions.contains(instruction.operands.front()))
                        {
                            const ExpandedPackValue source = expansions.at(instruction.operands.front());
                            ExpandedPackValue result{.types = source.types};
                            for (std::size_t index = 0; index < source.values.size(); ++index)
                            {
                                const Type& type = module_.types.get(source.types[index]);
                                if (instruction.opcode == Opcode::Copy && type.cleanup != CleanupKind::None)
                                {
                                    const ValueId copied{nextValue++};
                                    instructions.push_back(Instruction{.opcode = Opcode::Copy,
                                                                       .result = copied,
                                                                       .resultType = source.types[index],
                                                                       .operands = {source.values[index]},
                                                                       .resultOwnership = ValueOwnership::Owned,
                                                                       .source = instruction.source});
                                    result.values.push_back(copied);
                                }
                                else
                                {
                                    result.values.push_back(source.values[index]);
                                }
                            }
                            expansions[instruction.result] = std::move(result);
                            continue;
                        }
                        if ((instruction.opcode == Opcode::Drop || instruction.opcode == Opcode::Release) &&
                            instruction.operands.size() == 1 && expansions.contains(instruction.operands.front()))
                        {
                            const ExpandedPackValue& expansion = expansions.at(instruction.operands.front());
                            for (std::size_t index = 0; index < expansion.values.size(); ++index)
                            {
                                if (module_.types.get(expansion.types[index]).cleanup == CleanupKind::None)
                                    continue;
                                instructions.push_back(Instruction{.opcode = Opcode::Release,
                                                                   .operands = {expansion.values[index]},
                                                                   .source = instruction.source});
                            }
                            continue;
                        }

                        if (!instruction.expandedOperands.empty())
                        {
                            std::vector<ValueId> operands;
                            std::vector<TypeId> signatures;
                            bool valid = instruction.expandedOperands.size() == instruction.operands.size() &&
                                         instruction.signatureTypes.size() == instruction.operands.size();
                            for (std::size_t index = 0; valid && index < instruction.operands.size(); ++index)
                            {
                                const ValueId operand = instruction.operands[index];
                                if (!instruction.expandedOperands[index])
                                {
                                    valid = !expansions.contains(operand);
                                    operands.push_back(operand);
                                    signatures.push_back(instruction.signatureTypes[index]);
                                    continue;
                                }
                                const auto found = expansions.find(operand);
                                if (found == expansions.end())
                                {
                                    valid = false;
                                    break;
                                }
                                operands.insert(operands.end(), found->second.values.begin(),
                                                found->second.values.end());
                                signatures.insert(signatures.end(), found->second.types.begin(),
                                                  found->second.types.end());
                            }
                            if (!valid)
                            {
                                fail("Concrete parameter-pack expansion does not match its symbolic WIR operand.",
                                     instruction.source);
                                return false;
                            }
                            instruction.operands = std::move(operands);
                            instruction.signatureTypes = std::move(signatures);
                            instruction.expandedOperands.clear();
                        }
                        if (referencesPack(instruction))
                        {
                            fail("Concrete parameter pack is used without an expansion-aware WIR operation.",
                                 instruction.source);
                            return false;
                        }
                        instructions.push_back(std::move(instruction));
                    }
                    block.instructions = std::move(instructions);
                }
                return true;
            }

            void materializeOwnedLoads(Function& function)
            {
                ValueId::ValueType nextValue = 0;
                const auto observe = [&](const ValueId value)
                {
                    if (value)
                        nextValue = std::max(nextValue, static_cast<ValueId::ValueType>(value.value() + 1));
                };
                for (const Parameter& parameter : function.parameters)
                    observe(parameter.id);
                for (const BasicBlock& block : function.blocks)
                {
                    for (const Parameter& parameter : block.parameters)
                        observe(parameter.id);
                    for (const Instruction& instruction : block.instructions)
                        observe(instruction.result);
                }

                for (BasicBlock& block : function.blocks)
                {
                    std::vector<Instruction> instructions;
                    for (Instruction instruction : block.instructions)
                    {
                        if (instruction.opcode == Opcode::Load && instruction.result &&
                            module_.types.get(instruction.resultType).cleanup == CleanupKind::None)
                        {
                            const Type& loaded = module_.types.get(instruction.resultType);
                            instruction.resultOwnership =
                                loaded.ownership == OwnershipModel::Borrowed  ? ValueOwnership::Borrowed
                                : loaded.ownership == OwnershipModel::Trivial ? ValueOwnership::Trivial
                                                                              : ValueOwnership::Owned;
                            if (instruction.resultOwnership != ValueOwnership::Borrowed)
                            {
                                instruction.borrowLifetime = BorrowLifetime::None;
                                instruction.borrowOrigin = {};
                            }
                        }
                        const bool copyRequired =
                            instruction.opcode == Opcode::Load && instruction.result &&
                            instruction.resultOwnership != ValueOwnership::Borrowed &&
                            module_.types.get(instruction.resultType).cleanup != CleanupKind::None;
                        if (!copyRequired)
                        {
                            instructions.push_back(std::move(instruction));
                            continue;
                        }

                        const ValueId ownedResult = instruction.result;
                        const ValueId borrowedResult{nextValue++};
                        instruction.result = borrowedResult;
                        instruction.resultOwnership = ValueOwnership::Borrowed;
                        instruction.borrowLifetime = BorrowLifetime::Lexical;
                        instruction.borrowOrigin = instruction.operands.front();
                        instructions.push_back(std::move(instruction));
                        instructions.push_back(Instruction{.opcode = Opcode::Copy,
                                                           .result = ownedResult,
                                                           .resultType = instructions.back().resultType,
                                                           .operands = {borrowedResult},
                                                           .resultOwnership = ValueOwnership::Owned,
                                                           .source = instructions.back().source});
                    }
                    block.instructions = std::move(instructions);
                }
            }

            void substituteBody(Function& function, const Bindings& bindings)
            {
                if (!expandPackParameters(function, bindings))
                    return;
                Bindings cache;
                const auto replace = [&](TypeId id) { return substitute(id, bindings, cache); };
                function.returnType = replace(function.returnType);
                function.ownerType = replace(function.ownerType);
                function.genericParameters.clear();
                std::map<ValueId, TypeId> values;
                const auto parameter = [&](Parameter& p)
                {
                    p.type = replace(p.type);
                    p.ownership = ownership(p.type, p.ownership);
                    if (p.ownership != ValueOwnership::Borrowed)
                        p.borrowLifetime = BorrowLifetime::None;
                    values[p.id] = p.type;
                };
                for (Parameter& p : function.parameters)
                    parameter(p);
                Type callable = module_.types.get(function.callableType);
                callable.arguments.clear();
                const std::size_t hidden = function.captureParameterCount + (function.isMethod ? 1u : 0u);
                for (std::size_t index = hidden; index < function.parameters.size(); ++index)
                    callable.arguments.push_back(function.parameters[index].type);
                callable.arguments.push_back(function.returnType);
                function.callableType = module_.types.intern(std::move(callable));
                if (function.nativeBinding)
                {
                    std::vector<NativeAbiValue> nativeParameters;
                    for (auto value : function.nativeBinding->parameters)
                    {
                        if (const Type* pack = boundPack(value.type, bindings))
                        {
                            for (const TypeId type : pack->arguments)
                            {
                                value.type = type;
                                refreshNativeAbiValue(module_.types, value, false);
                                nativeParameters.push_back(value);
                            }
                            continue;
                        }
                        value.type = replace(value.type);
                        refreshNativeAbiValue(module_.types, value, false);
                        nativeParameters.push_back(value);
                    }
                    function.nativeBinding->parameters = std::move(nativeParameters);
                    function.nativeBinding->result.type = replace(function.nativeBinding->result.type);
                    refreshNativeAbiValue(module_.types, function.nativeBinding->result, true);
                    function.nativeBinding->stableKey += ":specialized:";
                    for (const auto argument : function.nativeBinding->templateArguments)
                        function.nativeBinding->stableKey += nativeAbiTypeKey(module_.types, argument) + ";";
                    std::ostringstream symbol;
                    symbol << "_wio_native_" << std::hex << stableModuleHash(function.nativeBinding->stableKey);
                    function.nativeBinding->thunkSymbol = symbol.str();
                }
                for (CaptureLayout& capture : function.captures)
                    capture.type = replace(capture.type);
                if (function.coroutine)
                    function.coroutine->resultType = replace(function.coroutine->resultType);
                for (auto& block : function.blocks)
                {
                    for (Parameter& p : block.parameters)
                        parameter(p);
                    for (auto& instruction : block.instructions)
                    {
                        instruction.resultType = replace(instruction.resultType);
                        instruction.targetType = replace(instruction.targetType);
                        if (instruction.opcode == Opcode::GenericConstant)
                        {
                            const Type& constant = module_.types.get(instruction.targetType);
                            bool valid = constant.kind == TypeKind::ConstValue && constant.arguments.size() == 1;
                            if (valid)
                            {
                                const TypeKind kind = module_.types.get(instruction.resultType).kind;
                                if (kind == TypeKind::Bool)
                                {
                                    valid = constant.name == "true" || constant.name == "false";
                                    instruction.literal = constant.name == "true";
                                }
                                else if (kind == TypeKind::String || kind == TypeKind::Text)
                                    instruction.literal = constant.name;
                                else if (kind == TypeKind::I8 || kind == TypeKind::I16 || kind == TypeKind::I32 ||
                                         kind == TypeKind::I64 || kind == TypeKind::ISize)
                                {
                                    std::int64_t value = 0;
                                    const auto parsed = std::from_chars(
                                        constant.name.data(), constant.name.data() + constant.name.size(), value);
                                    valid = parsed.ec == std::errc{} &&
                                            parsed.ptr == constant.name.data() + constant.name.size();
                                    const unsigned bits = kind == TypeKind::I8      ? 8
                                                          : kind == TypeKind::I16   ? 16
                                                          : kind == TypeKind::I32   ? 32
                                                          : kind == TypeKind::ISize ? sizeof(std::ptrdiff_t) * 8
                                                                                    : 64;
                                    if (bits < 64)
                                        valid &= value >= -(std::int64_t{1} << (bits - 1)) &&
                                                 value < (std::int64_t{1} << (bits - 1));
                                    instruction.literal = value;
                                }
                                else
                                {
                                    std::uint64_t value = 0;
                                    const auto parsed = std::from_chars(
                                        constant.name.data(), constant.name.data() + constant.name.size(), value);
                                    valid = parsed.ec == std::errc{} &&
                                            parsed.ptr == constant.name.data() + constant.name.size();
                                    const unsigned bits =
                                        kind == TypeKind::U8 || kind == TypeKind::Byte || kind == TypeKind::Char ? 8
                                        : kind == TypeKind::U16                                                  ? 16
                                        : kind == TypeKind::U32                                                  ? 32
                                        : kind == TypeKind::USize ? sizeof(std::size_t) * 8
                                                                  : 64;
                                    if (bits < 64)
                                        valid &= value < (std::uint64_t{1} << bits);
                                    instruction.literal = value;
                                }
                            }
                            if (!valid)
                                fail("Const generic argument has no representable concrete value.", instruction.source);
                            instruction.opcode = Opcode::Constant;
                        }
                        for (TypeId& type : instruction.signatureTypes)
                            type = replace(type);
                        for (TypeId& type : instruction.genericArguments)
                            type = replace(type);
                        if (instruction.result)
                        {
                            if (!(module_.types.get(instruction.resultType).kind == TypeKind::Reference &&
                                  instruction.resultOwnership == ValueOwnership::Trivial))
                                instruction.resultOwnership =
                                    ownership(instruction.resultType, instruction.resultOwnership);
                            if (instruction.resultOwnership != ValueOwnership::Borrowed)
                            {
                                instruction.borrowLifetime = BorrowLifetime::None;
                                instruction.borrowOrigin = {};
                            }
                            values[instruction.result] = instruction.resultType;
                        }
                    }
                }
                materializeOwnedLoads(function);
                std::map<ValueId, ValueId> aliases;
                for (auto& block : function.blocks)
                {
                    std::erase_if(block.instructions,
                                  [&](Instruction& instruction)
                                  {
                                      if (instruction.opcode == Opcode::Copy &&
                                          module_.types.get(instruction.resultType).cleanup == CleanupKind::None)
                                      {
                                          aliases[instruction.result] = instruction.operands.front();
                                          return true;
                                      }
                                      if (instruction.opcode == Opcode::Move &&
                                          module_.types.get(instruction.resultType).cleanup == CleanupKind::None)
                                          instruction.opcode = Opcode::Load;
                                      if (instruction.opcode == Opcode::Replace &&
                                          module_.types.get(values.at(instruction.operands[1])).cleanup ==
                                              CleanupKind::None)
                                          instruction.opcode = Opcode::Store;
                                      if (instruction.opcode != Opcode::Drop && instruction.opcode != Opcode::Release)
                                          return false;
                                      TypeId type = values.at(instruction.operands.front());
                                      if (instruction.opcode == Opcode::Drop)
                                          type = module_.types.get(type).arguments.front();
                                      return module_.types.get(type).cleanup == CleanupKind::None;
                                  });
                }
                for (auto& block : function.blocks)
                    for (auto& instruction : block.instructions)
                    {
                        for (ValueId& operand : instruction.operands)
                            while (aliases.contains(operand))
                                operand = aliases.at(operand);
                        while (aliases.contains(instruction.borrowOrigin))
                            instruction.borrowOrigin = aliases.at(instruction.borrowOrigin);
                    }
            }

            FunctionId instantiate(const Instruction& request)
            {
                const auto found = templates_.find(request.callee);
                if (found == templates_.end())
                    return request.callee;
                const Function& source = found->second;
                Bindings bindings;
                bool valid = request.genericArguments.size() <= source.genericParameters.size();
                for (std::size_t i = 0; valid && i < request.genericArguments.size(); ++i)
                    if (!openType(request.genericArguments[i]))
                        valid = bind(source.genericParameters[i], request.genericArguments[i], bindings);
                std::vector<TypeId> signature = request.signatureTypes;
                TypeId result = request.resultType ? request.resultType : module_.types.voidType();
                if (request.opcode == Opcode::ConstructObject || request.opcode == Opcode::ConstructComponent)
                {
                    Type receiver = module_.types.get(source.parameters.front().type);
                    receiver.arguments = {request.resultType};
                    signature.insert(signature.begin(), module_.types.intern(std::move(receiver)));
                    result = source.returnType;
                }
                if (request.opcode == Opcode::FunctionReference || request.opcode == Opcode::ClosureCreate)
                {
                    const Type callable = module_.types.get(request.resultType);
                    signature.clear();
                    if (request.opcode == Opcode::ClosureCreate)
                    {
                        for (std::size_t i = 0; i < request.signatureTypes.size(); ++i)
                        {
                            Type type = module_.types.get(source.parameters.at(i).type);
                            if (type.kind == TypeKind::Reference)
                            {
                                type.arguments = {request.signatureTypes[i]};
                                signature.push_back(module_.types.intern(std::move(type)));
                            }
                            else
                                signature.push_back(request.signatureTypes[i]);
                        }
                    }
                    signature.insert(signature.end(), callable.arguments.begin(), callable.arguments.end() - 1);
                    result = callable.arguments.back();
                }
                const auto packParameter =
                    std::ranges::find_if(source.parameters,
                                         [&](const Parameter& parameter)
                                         {
                                             const Type* type = module_.types.tryGet(parameter.type);
                                             return type && type->kind == TypeKind::GenericParameterPack;
                                         });
                if (packParameter == source.parameters.end())
                {
                    valid &= signature.size() == source.parameters.size();
                    for (std::size_t i = 0; valid && i < signature.size(); ++i)
                        valid = bind(source.parameters[i].type, signature[i], bindings);
                }
                else
                {
                    const std::size_t packIndex =
                        static_cast<std::size_t>(std::distance(source.parameters.begin(), packParameter));
                    valid &= std::next(packParameter) == source.parameters.end() && signature.size() >= packIndex;
                    for (std::size_t i = 0; valid && i < packIndex; ++i)
                        valid = bind(source.parameters[i].type, signature[i], bindings);
                    if (valid)
                    {
                        Type pack{.kind = TypeKind::TypePack};
                        pack.arguments.insert(pack.arguments.end(), signature.begin() + packIndex, signature.end());
                        valid = bind(packParameter->type, module_.types.intern(std::move(pack)), bindings);
                    }
                }
                valid &= bind(source.returnType, result, bindings);
                for (TypeId p : source.genericParameters)
                    valid &= bindings.contains(p);
                for (const auto& [p, actual] : bindings)
                    valid &= !openType(actual);
                if (!valid)
                {
                    fail("Cannot materialize generic function '" + source.name + "' from its pinned signature.",
                         request.source);
                    return request.callee;
                }
                std::ostringstream key;
                key << source.id.value();
                for (const auto& [p, actual] : bindings)
                    key << ':' << p.value() << '=' << actual.value();
                if (const auto it = instances_.find(key.str()); it != instances_.end())
                    return it->second;
                if (instances_.size() >= maximumBodies_)
                {
                    diagnostics_.push_back({"WIR3101",
                                            "Generic materialization exceeded its body limit (" +
                                                std::to_string(maximumBodies_) +
                                                "); possible expanding polymorphic recursion.",
                                            request.source});
                    return request.callee;
                }
                const FunctionId id{nextId_++};
                instances_[key.str()] = id;
                instanceKeys_[id] = key.str();
                Function function = source;
                if (function.nativeBinding)
                {
                    for (TypeId parameter : source.genericParameters)
                    {
                        const TypeId argument = bindings.at(parameter);
                        const Type& type = module_.types.get(argument);
                        if (type.kind == TypeKind::TypePack || type.kind == TypeKind::ValuePack)
                        {
                            function.nativeBinding->templateArguments.insert(
                                function.nativeBinding->templateArguments.end(), type.arguments.begin(),
                                type.arguments.end());
                        }
                        else
                        {
                            function.nativeBinding->templateArguments.push_back(argument);
                        }
                    }
                }
                function.id = id;
                function.genericOrigin = source.id;
                function.specializationKey = key.str();
                for (const auto& [p, actual] : bindings)
                    function.specializationArguments.push_back(actual);
                function.name += "$specialized." + key.str();
                substituteBody(function, bindings);
                if (openFunction(function))
                    fail("Generic function still contains an open signature after substitution: " + source.name,
                         request.source);
                module_.functions.push_back(std::move(function));
                return id;
            }
        };
    } // namespace

    std::vector<SpecializationDiagnostic> GenericSpecializer::specialize(typed::Module& module) const
    {
        return Materializer{module, maximumBodies_}.run();
    }
} // namespace wio::wir
