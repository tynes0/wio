#include "wio/wir/hierarchy_lowering.h"
#include <algorithm>
#include <functional>
#include <set>
#include <map>

namespace wio::wir
{
    std::vector<std::string> lowerHierarchy(lowered::Module& module)
    {
        std::vector<std::string> errors;
        for (std::size_t index = 0; index < module.types.size(); ++index)
        {
            const TypeId id{static_cast<TypeId::ValueType>(index)};
            auto& type = module.types.getMutable(id);
            if (type.nominalKind != NominalKind::Object && type.nominalKind != NominalKind::Interface)
                continue;
            type.castTypes.clear();
            type.dispatchEntries.clear();
            type.destructor = {};
            type.defaultConstructor = {};
            std::set<TypeId> active, seen;
            std::function<void(TypeId)> visit = [&](TypeId current)
            {
                if (active.contains(current))
                {
                    errors.push_back("Cyclic object/interface hierarchy: " + type.name);
                    return;
                }
                if (!seen.insert(current).second)
                    return;
                const auto* ancestor = module.types.tryGet(current);
                if (!ancestor)
                {
                    errors.push_back("Unknown hierarchy type: " + type.name);
                    return;
                }
                active.insert(current);
                type.castTypes.push_back(current);
                for (TypeId base : ancestor->baseTypes)
                    visit(base);
                active.erase(current);
            };
            visit(id);
            for (TypeId contract : type.castTypes)
                for (const auto& method : module.types.get(contract).methods)
                {
                    const auto declaration =
                        std::ranges::find(module.functions, method.function, &lowered::Function::id);
                    if (declaration != module.functions.end() && !declaration->genericParameters.empty())
                        continue;
                    const auto found =
                        std::ranges::find_if(type.methods,
                                             [&](const MethodLayout& candidate)
                                             {
                                                 return candidate.name == method.name &&
                                                        candidate.parameterTypes == method.parameterTypes &&
                                                        candidate.returnType == method.returnType;
                                             });
                    if (found == type.methods.end())
                        errors.push_back("Missing canonical method contract: " + type.name + "::" + method.name);
                    else
                        type.dispatchEntries.push_back(
                            {contract, method.slot, found->isAbstract ? FunctionId{} : found->function});
                }
            for (const auto& function : module.functions)
                if (function.ownerType == id)
                {
                    std::string name = function.name;
                    if (function.genericOrigin)
                    {
                        const auto origin =
                            std::ranges::find(module.functions, function.genericOrigin, &lowered::Function::id);
                        if (origin != module.functions.end())
                            name = origin->name;
                    }
                    if (name == "OnDestruct" || name.ends_with("::OnDestruct"))
                        type.destructor = function.id;
                    if ((name == "OnConstruct" || name.ends_with("::OnConstruct")) && function.parameters.size() == 1)
                        type.defaultConstructor = function.id;
                }
        }
        // Field storage indices are local to the declaring subobject, not to
        // the most-derived receiver. Pin both before any backend sees them.
        for (auto& function : module.functions)
        {
            std::map<ValueId, TypeId> values;
            for (const auto& parameter : function.parameters)
                values[parameter.id] = parameter.type;
            for (const auto& block : function.blocks)
            {
                for (const auto& parameter : block.parameters)
                    values[parameter.id] = parameter.type;
                for (const auto& instruction : block.instructions)
                    if (instruction.result)
                        values[instruction.result] = instruction.resultType;
            }
            for (auto& block : function.blocks)
                for (auto& instruction : block.instructions)
                {
                    if (instruction.opcode != lowered::Opcode::FieldPlace || instruction.operands.size() != 1)
                        continue;
                    TypeId receiver = values[instruction.operands.front()];
                    std::set<TypeId> wrappers;
                    while (const auto* type = module.types.tryGet(receiver))
                    {
                        if (!wrappers.insert(receiver).second)
                        {
                            errors.push_back("Cyclic field receiver reference type");
                            break;
                        }
                        if (type->kind != TypeKind::Reference || type->arguments.empty())
                            break;
                        receiver = type->arguments.front();
                    }
                    std::set<TypeId> visited;
                    std::function<bool(TypeId)> resolve = [&](TypeId owner)
                    {
                        if (!visited.insert(owner).second)
                            return false;
                        const auto* type = module.types.tryGet(owner);
                        if (!type)
                            return false;
                        for (std::size_t i = 0; i < type->fields.size(); ++i)
                            if (type->fields[i].name == instruction.selector)
                            {
                                instruction.targetType = owner;
                                instruction.projectionIndex = static_cast<std::uint32_t>(i);
                                return true;
                            }
                        for (TypeId base : type->baseTypes)
                            if (resolve(base))
                                return true;
                        return false;
                    };
                    if (!resolve(receiver))
                        errors.push_back("Field has no canonical storage: " + instruction.selector);
                }
        }
        return errors;
    }
} // namespace wio::wir
