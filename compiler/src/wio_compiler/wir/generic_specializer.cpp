#include "wio/wir/generic_specializer.h"

#include <algorithm>
#include <charconv>
#include <map>
#include <set>
#include <sstream>

namespace wio::wir
{
    namespace
    {
        using namespace typed;
        using Bindings = std::map<TypeId, TypeId>;

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
                    if (entry.kind != ModuleExportKind::GenericFunctionSpecialization) continue;
                    const auto source = templates_.find(entry.function);
                    if (source == templates_.end() || !openFunction(source->second) || source->second.isExternal) continue;
                    Instruction request;
                    request.callee = entry.function;
                    request.genericArguments = entry.genericArguments;
                    request.signatureTypes = entry.parameterTypes;
                    request.resultType = entry.returnType;
                    entry.function = instantiate(request);
                }
                // Appending a body grows this worklist. Cache entries are registered
                // before visiting it, so self/mutual recursion closes on existing IDs.
                for (std::size_t index = 0; index < module_.functions.size() && diagnostics_.empty(); ++index)
                {
                    if (openFunction(module_.functions[index])) continue;
                    Function function = module_.functions[index];
                    for (auto& block : function.blocks)
                        for (auto& instruction : block.instructions)
                        {
                            if (!instruction.callee) continue;
                            const auto source = templates_.find(instruction.callee);
                            if (source == templates_.end() || source->second.isExternal || !openFunction(source->second)) continue;
                            instruction.callee = instantiate(instruction);
                            if (const auto key = instanceKeys_.find(instruction.callee); key != instanceKeys_.end())
                                instruction.specializationKey = key->second;
                        }
                    module_.functions[index] = std::move(function);
                }
                return std::move(diagnostics_);
            }

        private:
            Module& module_;
            std::map<FunctionId, Function> templates_;
            std::map<std::string, FunctionId> instances_;
            std::map<FunctionId, std::string> instanceKeys_;
            std::vector<SpecializationDiagnostic> diagnostics_;
            FunctionId::ValueType nextId_ = 0;
            std::size_t maximumBodies_;

            bool openType(TypeId id, std::set<TypeId>& visiting) const
            {
                const Type* type = module_.types.tryGet(id);
                if (!type || !visiting.insert(id).second) return false;
                if (parameterKind(type->kind) || type->kind == TypeKind::ValuePack || type->kind == TypeKind::TypePack) return true;
                return openType(type->extentParameter, visiting) ||
                    std::ranges::any_of(type->arguments, [&](TypeId nested) { return openType(nested, visiting); }) ||
                    std::ranges::any_of(type->baseTypes, [&](TypeId nested) { return openType(nested, visiting); }) ||
                    std::ranges::any_of(type->fields, [&](const FieldLayout& field) { return openType(field.type, visiting); });
            }
            bool openType(TypeId id) const { std::set<TypeId> visiting; return openType(id, visiting); }
            bool openFunction(const Function& function) const
            {
                return !function.genericParameters.empty() || openType(function.returnType) || openType(function.ownerType) ||
                    std::ranges::any_of(function.parameters, [&](const Parameter& p) { return openType(p.type); });
            }
            void fail(std::string message, SourceSpan source)
            {
                diagnostics_.push_back({"WIR3100", std::move(message), source});
            }
            bool bind(TypeId pattern, TypeId actual, Bindings& bindings)
            {
                if (!module_.types.tryGet(pattern) || !module_.types.tryGet(actual)) return pattern == actual;
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
                if (p->kind != a->kind || p->arguments.size() != a->arguments.size()) return false;
                if (p->extentParameter)
                {
                    if (!a->staticExtent) return false;
                    Type constant{.kind = TypeKind::ConstValue, .name = std::to_string(*a->staticExtent),
                        .arguments = module_.types.get(p->extentParameter).arguments};
                    if (!bind(p->extentParameter, module_.types.intern(std::move(constant)), bindings)) return false;
                }
                else if (p->staticExtent != a->staticExtent) return false;
                if (p->kind == TypeKind::Named && p->name != a->name) return false;
                if (p->arguments.empty()) return pattern == actual;
                for (std::size_t i = 0; i < p->arguments.size(); ++i)
                    if (!bind(p->arguments[i], a->arguments[i], bindings)) return false;
                return true;
            }

            TypeId substitute(TypeId id, const Bindings& bindings, Bindings& cache)
            {
                if (!id) return id;
                if (const auto it = bindings.find(id); it != bindings.end()) return it->second;
                if (const auto it = cache.find(id); it != cache.end()) return it->second;
                Type type = module_.types.get(id);
                if (!openType(id)) return id;
                for (TypeId& argument : type.arguments) argument = substitute(argument, bindings, cache);
                if (type.extentParameter)
                {
                    const TypeId extent = substitute(type.extentParameter, bindings, cache);
                    const Type constant = module_.types.get(extent);
                    std::size_t value = 0;
                    const auto parsed = std::from_chars(constant.name.data(), constant.name.data() + constant.name.size(), value);
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
                for (TypeId& base : type.baseTypes) base = substitute(base, bindings, cache);
                for (FieldLayout& field : type.fields) field.type = substitute(field.type, bindings, cache);
                for (MethodLayout& method : type.methods)
                {
                    method.returnType = substitute(method.returnType, bindings, cache);
                    for (TypeId& p : method.parameterTypes) p = substitute(p, bindings, cache);
                }
                if (result)
                {
                    // An existing concrete layout from semantic analysis is the
                    // canonical identity; never overwrite its resolved fields.
                    if (openType(result)) module_.types.getMutable(result) = std::move(type);
                }
                else result = module_.types.intern(std::move(type));
                cache[id] = result;
                return result;
            }

            ValueOwnership ownership(TypeId id, ValueOwnership old) const
            {
                const Type& type = module_.types.get(id);
                if (type.ownership == OwnershipModel::Trivial) return ValueOwnership::Trivial;
                if (type.ownership == OwnershipModel::Borrowed) return ValueOwnership::Borrowed;
                return old == ValueOwnership::Borrowed ? old : ValueOwnership::Owned;
            }

            void substituteBody(Function& function, const Bindings& bindings)
            {
                Bindings cache;
                const auto replace = [&](TypeId id) { return substitute(id, bindings, cache); };
                function.returnType = replace(function.returnType);
                function.callableType = replace(function.callableType);
                function.ownerType = replace(function.ownerType);
                function.genericParameters.clear();
                std::map<ValueId, TypeId> values;
                const auto parameter = [&](Parameter& p)
                {
                    p.type = replace(p.type);
                    p.ownership = ownership(p.type, p.ownership);
                    if (p.ownership != ValueOwnership::Borrowed) p.borrowLifetime = BorrowLifetime::None;
                    values[p.id] = p.type;
                };
                for (Parameter& p : function.parameters) parameter(p);
                for (CaptureLayout& capture : function.captures) capture.type = replace(capture.type);
                if (function.coroutine) function.coroutine->resultType = replace(function.coroutine->resultType);
                for (auto& block : function.blocks)
                {
                    for (Parameter& p : block.parameters) parameter(p);
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
                                else if (kind == TypeKind::String || kind == TypeKind::Text) instruction.literal = constant.name;
                                else if (kind == TypeKind::I8 || kind == TypeKind::I16 || kind == TypeKind::I32 ||
                                    kind == TypeKind::I64 || kind == TypeKind::ISize)
                                {
                                    std::int64_t value = 0;
                                    const auto parsed = std::from_chars(constant.name.data(), constant.name.data() + constant.name.size(), value);
                                    valid = parsed.ec == std::errc{} && parsed.ptr == constant.name.data() + constant.name.size();
                                    const unsigned bits = kind == TypeKind::I8 ? 8 : kind == TypeKind::I16 ? 16 :
                                        kind == TypeKind::I32 ? 32 : kind == TypeKind::ISize ? sizeof(std::ptrdiff_t) * 8 : 64;
                                    if (bits < 64) valid &= value >= -(std::int64_t{1} << (bits - 1)) &&
                                        value < (std::int64_t{1} << (bits - 1));
                                    instruction.literal = value;
                                }
                                else
                                {
                                    std::uint64_t value = 0;
                                    const auto parsed = std::from_chars(constant.name.data(), constant.name.data() + constant.name.size(), value);
                                    valid = parsed.ec == std::errc{} && parsed.ptr == constant.name.data() + constant.name.size();
                                    const unsigned bits = kind == TypeKind::U8 || kind == TypeKind::Byte || kind == TypeKind::Char ? 8 :
                                        kind == TypeKind::U16 ? 16 : kind == TypeKind::U32 ? 32 :
                                        kind == TypeKind::USize ? sizeof(std::size_t) * 8 : 64;
                                    if (bits < 64) valid &= value < (std::uint64_t{1} << bits);
                                    instruction.literal = value;
                                }
                            }
                            if (!valid) fail("Const generic argument has no representable concrete value.", instruction.source);
                            instruction.opcode = Opcode::Constant;
                        }
                        for (TypeId& type : instruction.signatureTypes) type = replace(type);
                        for (TypeId& type : instruction.genericArguments) type = replace(type);
                        if (instruction.result)
                        {
                            if (instruction.opcode != Opcode::LocalPlace)
                                instruction.resultOwnership = ownership(instruction.resultType, instruction.resultOwnership);
                            if (instruction.resultOwnership != ValueOwnership::Borrowed)
                            {
                                instruction.borrowLifetime = BorrowLifetime::None;
                                instruction.borrowOrigin = {};
                            }
                            values[instruction.result] = instruction.resultType;
                        }
                    }
                }
                std::map<ValueId, ValueId> aliases;
                for (auto& block : function.blocks)
                {
                    std::erase_if(block.instructions, [&](Instruction& instruction)
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
                            module_.types.get(values.at(instruction.operands[1])).cleanup == CleanupKind::None)
                            instruction.opcode = Opcode::Store;
                        if (instruction.opcode != Opcode::Drop && instruction.opcode != Opcode::Release) return false;
                        TypeId type = values.at(instruction.operands.front());
                        if (instruction.opcode == Opcode::Drop) type = module_.types.get(type).arguments.front();
                        return module_.types.get(type).cleanup == CleanupKind::None;
                    });
                }
                for (auto& block : function.blocks)
                    for (auto& instruction : block.instructions)
                    {
                        for (ValueId& operand : instruction.operands)
                            while (aliases.contains(operand)) operand = aliases.at(operand);
                        while (aliases.contains(instruction.borrowOrigin)) instruction.borrowOrigin = aliases.at(instruction.borrowOrigin);
                    }
            }

            FunctionId instantiate(const Instruction& request)
            {
                const auto found = templates_.find(request.callee);
                if (found == templates_.end()) return request.callee;
                const Function& source = found->second;
                Bindings bindings;
                bool valid = request.genericArguments.size() <= source.genericParameters.size();
                for (std::size_t i = 0; valid && i < request.genericArguments.size(); ++i)
                    if (!openType(request.genericArguments[i]))
                        valid = bind(source.genericParameters[i], request.genericArguments[i], bindings);
                std::vector<TypeId> signature = request.signatureTypes;
                TypeId result = request.resultType ? request.resultType : module_.types.voidType();
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
                            else signature.push_back(request.signatureTypes[i]);
                        }
                    }
                    signature.insert(signature.end(), callable.arguments.begin(), callable.arguments.end() - 1);
                    result = callable.arguments.back();
                }
                valid &= signature.size() == source.parameters.size();
                for (std::size_t i = 0; valid && i < signature.size(); ++i)
                    valid = bind(source.parameters[i].type, signature[i], bindings);
                valid &= bind(source.returnType, result, bindings);
                for (TypeId p : source.genericParameters) valid &= bindings.contains(p);
                for (const auto& [p, actual] : bindings) valid &= !openType(actual);
                if (!valid)
                {
                    fail("Cannot materialize generic function '" + source.name + "' from its pinned signature.", request.source);
                    return request.callee;
                }
                std::ostringstream key;
                key << source.id.value();
                for (const auto& [p, actual] : bindings) key << ':' << p.value() << '=' << actual.value();
                if (const auto it = instances_.find(key.str()); it != instances_.end()) return it->second;
                if (instances_.size() >= maximumBodies_)
                {
                    diagnostics_.push_back({"WIR3101", "Generic materialization exceeded its body limit (" +
                        std::to_string(maximumBodies_) + "); possible expanding polymorphic recursion.", request.source});
                    return request.callee;
                }
                const FunctionId id{nextId_++};
                instances_[key.str()] = id;
                instanceKeys_[id] = key.str();
                Function function = source;
                function.id = id;
                function.genericOrigin = source.id;
                function.specializationKey = key.str();
                for (const auto& [p, actual] : bindings) function.specializationArguments.push_back(actual);
                function.name += "$specialized." + key.str();
                substituteBody(function, bindings);
                if (openFunction(function))
                    fail("Generic function still contains an open signature after substitution: " + source.name, request.source);
                module_.functions.push_back(std::move(function));
                return id;
            }
        };
    }

    std::vector<SpecializationDiagnostic> GenericSpecializer::specialize(typed::Module& module) const
    {
        return Materializer{module, maximumBodies_}.run();
    }
}
