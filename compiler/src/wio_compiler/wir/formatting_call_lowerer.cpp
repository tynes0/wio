#include "wio/wir/formatting_call_lowerer.h"

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <utility>

namespace wio::wir
{
    namespace
    {
        using namespace typed;
        using FunctionIndex = std::unordered_map<FunctionId::ValueType, const Function*>;

        enum class Surface
        {
            Unsupported,
            Builtin,
            Object
        };

        struct Formatter
        {
            Surface surface = Surface::Unsupported;
            TypeId owner;
            NominalKind kind = NominalKind::None;
            MethodLayout method;
        };

        bool hasOpenType(const TypeTable& types, const TypeId id, const unsigned depth = 0)
        {
            if (!id)
                return false;
            const Type* type = types.tryGet(id);
            if (!type || depth > 128)
                return true;
            if (type->kind == TypeKind::GenericParameter || type->kind == TypeKind::ConstGenericParameter ||
                type->kind == TypeKind::GenericParameterPack)
                return true;
            return std::ranges::any_of(type->arguments,
                                       [&](const TypeId argument) { return hasOpenType(types, argument, depth + 1); });
        }

        Formatter formatter(const Module& module, const FunctionIndex& functions, TypeId id, const unsigned depth = 0)
        {
            const Type* type = module.types.tryGet(id);
            if (!type || depth > 128)
                return {};
            if (type->kind == TypeKind::Reference && type->arguments.size() == 1)
                return formatter(module, functions, type->arguments.front(), depth + 1);
            if ((type->kind >= TypeKind::Bool && type->kind <= TypeKind::Text))
                return {.surface = Surface::Builtin};
            if (type->kind == TypeKind::Array || type->kind == TypeKind::Dictionary)
            {
                if (type->arguments.size() != (type->kind == TypeKind::Array ? 1u : 2u))
                    return {};
                for (const auto argument : type->arguments)
                    if (formatter(module, functions, argument, depth + 1).surface != Surface::Builtin)
                        return {};
                return {.surface = Surface::Builtin};
            }
            if (type->kind != TypeKind::Named)
                return {};
            if (type->nominalKind == NominalKind::Enum || type->nominalKind == NominalKind::Flagset)
                return {.surface = Surface::Builtin};
            if (type->nominalKind != NominalKind::Object && type->nominalKind != NominalKind::Interface)
                return {};
            if (hasOpenType(module.types, id))
                return {};

            std::optional<MethodLayout> selected;
            for (const MethodLayout& method : type->methods)
            {
                if (method.name != "ToString" || !method.parameterTypes.empty() ||
                    method.returnType != module.types.stringType() || method.visibility != FieldVisibility::Public)
                    continue;
                const auto found = functions.find(method.function.value());
                const Function* callee = found == functions.end() ? nullptr : found->second;
                if (!callee || callee->isAsync || callee->nativeBinding || !callee->genericParameters.empty() ||
                    !callee->isMethod || hasOpenType(module.types, callee->ownerType))
                    continue;
                if (selected)
                    return {}; // Never guess an overload or inherited declaration.
                selected = method;
            }
            return selected ? Formatter{Surface::Object, id, type->nominalKind, *selected} : Formatter{};
        }

        bool eligible(const Module& module, const FunctionIndex& functions, const Instruction& instruction)
        {
            if (instruction.opcode != Opcode::Interpolate || instruction.operands.empty() ||
                instruction.signatureTypes.size() != instruction.operands.size() ||
                instruction.stringSegments.size() != instruction.operands.size() + 1)
                return false;
            bool hasObject = false;
            for (const TypeId type : instruction.signatureTypes)
            {
                const auto surface = formatter(module, functions, type).surface;
                if (surface == Surface::Unsupported)
                    return false;
                hasObject = hasObject || surface == Surface::Object;
            }
            return hasObject;
        }

        std::size_t lowerFunction(Module& module, const FunctionIndex& functions, Function& function)
        {
            if (function.isAsync || function.coroutine || !function.genericParameters.empty() ||
                hasOpenType(module.types, function.ownerType))
                return 0;
            if (std::ranges::none_of(function.blocks,
                                     [&](const BasicBlock& block)
                                     {
                                         return std::ranges::any_of(block.instructions, [&](const Instruction& value)
                                                                    { return eligible(module, functions, value); });
                                     }))
                return 0;

            ValueId::ValueType nextValue = 0;
            BlockId::ValueType nextBlock = 0;
            const auto remember = [&](const ValueId value)
            {
                if (value)
                    nextValue = std::max(nextValue, value.value() + 1);
            };
            for (const Parameter& parameter : function.parameters)
                remember(parameter.id);
            for (const BasicBlock& block : function.blocks)
            {
                nextBlock = std::max(nextBlock, block.id.value() + 1);
                for (const Parameter& parameter : block.parameters)
                    remember(parameter.id);
                for (const Instruction& instruction : block.instructions)
                    remember(instruction.result);
            }
            const auto fresh = [&] { return ValueId{nextValue++}; };
            std::vector<BasicBlock> blocks;
            std::size_t changed = 0;
            const TypeId string = module.types.stringType();
            for (BasicBlock& original : function.blocks)
            {
                std::size_t current = blocks.size();
                blocks.push_back({.id = original.id,
                                  .name = std::move(original.name),
                                  .parameters = std::move(original.parameters),
                                  .source = original.source});
                for (Instruction& instruction : original.instructions)
                {
                    if (!eligible(module, functions, instruction))
                    {
                        blocks[current].instructions.push_back(std::move(instruction));
                        continue;
                    }
                    ++changed;
                    std::vector<ValueId> temporaries;
                    for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                    {
                        const Formatter selected = formatter(module, functions, instruction.signatureTypes[index]);
                        if (instruction.signatureTypes[index] == string)
                            continue;
                        ValueId formatted = fresh();
                        if (selected.surface == Surface::Builtin)
                        {
                            // Every source expression has already been evaluated.
                            // Format *all* operands in source order, not just user
                            // hooks, so stateful native numeric facets stay ordered.
                            blocks[current].instructions.push_back(
                                {.opcode = Opcode::Interpolate,
                                 .result = formatted,
                                 .resultType = string,
                                 .operands = {instruction.operands[index]},
                                 .signatureTypes = {instruction.signatureTypes[index]},
                                 .stringSegments = {"", ""},
                                 .intrinsicFamily = IntrinsicFamily::String,
                                 .resultOwnership = ValueOwnership::Owned,
                                 .source = instruction.source});
                        }
                        else
                        {
                            const TypeId receiver = module.types.intern({.kind = TypeKind::Reference,
                                                                         .arguments = {selected.owner},
                                                                         .isMutable = selected.method.receiverMutable,
                                                                         .ownership = OwnershipModel::Borrowed});
                            const ValueId null = fresh(), condition = fresh(), empty = fresh(), called = fresh();
                            const BlockId nullBlock{nextBlock++}, callBlock{nextBlock++}, mergeBlock{nextBlock++};
                            auto& code = blocks[current].instructions;
                            code.push_back({.opcode = Opcode::DefaultValue,
                                            .result = null,
                                            .resultType = selected.owner,
                                            .resultOwnership = ValueOwnership::Owned,
                                            .source = instruction.source});
                            code.push_back({.opcode = Opcode::IdentityEqual,
                                            .result = condition,
                                            .resultType = module.types.boolType(),
                                            .operands = {instruction.operands[index], null},
                                            .binaryOperator = BinaryOperator::Equal,
                                            .source = instruction.source});
                            code.push_back(
                                {.opcode = Opcode::Release, .operands = {null}, .source = instruction.source});
                            code.push_back({.opcode = Opcode::CondBranch,
                                            .operands = {condition},
                                            .targets = {nullBlock, callBlock},
                                            .source = instruction.source});
                            BasicBlock nullPath{.id = nullBlock, .name = "format.null", .source = instruction.source};
                            nullPath.instructions = {{.opcode = Opcode::Constant,
                                                      .result = empty,
                                                      .resultType = string,
                                                      .literal = std::string{"null"},
                                                      .resultOwnership = ValueOwnership::Owned,
                                                      .source = instruction.source},
                                                     {.opcode = Opcode::Branch,
                                                      .operands = {empty},
                                                      .targets = {mergeBlock},
                                                      .source = instruction.source}};
                            blocks.push_back(std::move(nullPath));
                            BasicBlock callPath{.id = callBlock, .name = "format.call", .source = instruction.source};
                            callPath.instructions = {{.opcode = selected.kind == NominalKind::Interface
                                                                    ? Opcode::InterfaceCall
                                                                    : Opcode::VirtualCall,
                                                      .result = called,
                                                      .resultType = string,
                                                      .operands = {instruction.operands[index]},
                                                      .callee = selected.method.function,
                                                      .selector = selected.method.name,
                                                      .projectionIndex = selected.method.slot,
                                                      .signatureTypes = {receiver},
                                                      .targetType = selected.owner,
                                                      .resultOwnership = ValueOwnership::Owned,
                                                      .source = instruction.source},
                                                     {.opcode = Opcode::Branch,
                                                      .operands = {called},
                                                      .targets = {mergeBlock},
                                                      .source = instruction.source}};
                            blocks.push_back(std::move(callPath));
                            current = blocks.size();
                            blocks.push_back({.id = mergeBlock,
                                              .name = "format.merge",
                                              .parameters = {{.id = formatted,
                                                              .type = string,
                                                              .ownership = ValueOwnership::Owned,
                                                              .source = instruction.source}},
                                              .source = instruction.source});
                        }
                        instruction.operands[index] = formatted;
                        instruction.signatureTypes[index] = string;
                        temporaries.push_back(formatted);
                    }
                    const SourceSpan source = instruction.source;
                    blocks[current].instructions.push_back(std::move(instruction));
                    for (const ValueId temporary : temporaries)
                        blocks[current].instructions.push_back(
                            {.opcode = Opcode::Release, .operands = {temporary}, .source = source});
                }
            }
            function.blocks = std::move(blocks);
            return changed;
        }
    } // namespace

    std::size_t FormattingCallLowerer::lower(typed::Module& module)
    {
        std::size_t changed = 0;
        FunctionIndex functions;
        functions.reserve(module.functions.size());
        for (const auto& function : module.functions)
            functions.emplace(function.id.value(), &function);
        for (auto& function : module.functions)
            changed += lowerFunction(module, functions, function);
        return changed;
    }
} // namespace wio::wir
