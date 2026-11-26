#include "wio/wir/formatting_call_lowerer.h"
#include "wio/wir/native_abi_types.h"

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
            Object,
            Container,
            Native
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
                bool builtin = true;
                for (const auto argument : type->arguments)
                {
                    const auto child = formatter(module, functions, argument, depth + 1).surface;
                    if (child == Surface::Unsupported)
                        return {};
                    builtin &= child == Surface::Builtin;
                }
                return {.surface = builtin ? Surface::Builtin : Surface::Container, .owner = id};
            }
            if (type->kind != TypeKind::Named)
                return {};
            if (type->nominalKind == NominalKind::Enum || type->nominalKind == NominalKind::Flagset)
                return {.surface = Surface::Builtin};
            if (type->nominalKind == NominalKind::Component && type->nativeBinding && !hasOpenType(module.types, id))
                return {.surface = Surface::Native, .owner = id};
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
                if (!callee || callee->isAsync || !callee->genericParameters.empty() || !callee->isMethod ||
                    hasOpenType(module.types, callee->ownerType))
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
                hasObject = hasObject || surface != Surface::Builtin;
            }
            return hasObject;
        }

        using HelperIndex = std::unordered_map<TypeId::ValueType, FunctionId>;

        // Container traversal is ordinary WIR, so hooks use the same dispatcher,
        // cleanup and coroutine machinery as explicitly written user loops.
        Function containerHelper(Module& module, TypeId container, FunctionId id)
        {
            const Type shape = module.types.get(container);
            const auto string = module.types.stringType();
            const auto boolean = module.types.boolType();
            const auto place = module.types.intern({.kind = TypeKind::Reference,
                                                    .arguments = {string},
                                                    .isMutable = true,
                                                    .ownership = OwnershipModel::Borrowed});
            const auto iterator = module.types.intern({.kind = TypeKind::Iterator,
                                                       .arguments = {container},
                                                       .ownership = OwnershipModel::OwnedValue,
                                                       .cleanup = CleanupKind::DestroyValue});
            Function result{
                .id = id, .name = "$format.container." + std::to_string(container.value()), .returnType = string};
            result.callableType = module.types.intern({.kind = TypeKind::Function, .arguments = {container, string}});
            result.parameters = {{.id = ValueId{0}, .type = container, .ownership = ValueOwnership::Owned}};
            ValueId::ValueType next = 1;
            const auto fresh = [&] { return ValueId{next++}; };
            const auto output = fresh(), prefix = fresh(), cursor = fresh(), first = fresh(), condition = fresh();
            const auto truth = fresh(), falsity = fresh();
            BasicBlock entry{.id = BlockId{0}, .name = "format.container.entry"};
            entry.instructions = {
                {.opcode = Opcode::LocalPlace,
                 .result = output,
                 .resultType = place,
                 .selector = "$format.output",
                 .resultOwnership = ValueOwnership::Borrowed,
                 .borrowLifetime = BorrowLifetime::Lexical},
                {.opcode = Opcode::Constant,
                 .result = prefix,
                 .resultType = string,
                 .literal = std::string(shape.kind == TypeKind::Array ? "[" : "{"),
                 .resultOwnership = ValueOwnership::Owned},
                {.opcode = Opcode::PlaceInit, .operands = {output, prefix}},
                {.opcode = Opcode::Release, .operands = {prefix}},
                {.opcode = Opcode::IteratorCreate,
                 .result = cursor,
                 .resultType = iterator,
                 .operands = {ValueId{0}},
                 .selector = shape.kind == TypeKind::Array ? "array" : "dictionary",
                 .signatureTypes = {container},
                 .resultOwnership = ValueOwnership::Owned},
                {.opcode = Opcode::Constant, .result = truth, .resultType = boolean, .literal = true},
                {.opcode = Opcode::Constant, .result = falsity, .resultType = boolean, .literal = false},
                {.opcode = Opcode::Branch, .operands = {truth}, .targets = {BlockId{1}}}};
            BasicBlock header{
                .id = BlockId{1}, .name = "format.container.next", .parameters = {{.id = first, .type = boolean}}};
            header.instructions = {
                {.opcode = Opcode::IteratorHasNext, .result = condition, .resultType = boolean, .operands = {cursor}},
                {.opcode = Opcode::CondBranch, .operands = {condition}, .targets = {BlockId{2}, BlockId{5}}}};
            BasicBlock separatorTest{.id = BlockId{2}, .name = "format.container.separator-test"};
            separatorTest.instructions = {
                {.opcode = Opcode::CondBranch, .operands = {first}, .targets = {BlockId{4}, BlockId{3}}}};
            const auto append = [&](BasicBlock& block, ValueId value)
            {
                block.instructions.push_back({.opcode = Opcode::IntrinsicCall,
                                              .resultType = module.types.voidType(),
                                              .operands = {output, value},
                                              .selector = "Append",
                                              .signatureTypes = {place, string},
                                              .intrinsicFamily = IntrinsicFamily::String,
                                              .targetType = string});
            };
            const auto literal = [&](BasicBlock& block, const char* text)
            {
                const auto value = fresh();
                block.instructions.push_back({.opcode = Opcode::Constant,
                                              .result = value,
                                              .resultType = string,
                                              .literal = std::string(text),
                                              .resultOwnership = ValueOwnership::Owned});
                append(block, value);
                block.instructions.push_back({.opcode = Opcode::Release, .operands = {value}});
            };
            BasicBlock separator{.id = BlockId{3}, .name = "format.container.separator"};
            literal(separator, ", ");
            separator.instructions.push_back({.opcode = Opcode::Branch, .targets = {BlockId{4}}});
            BasicBlock body{.id = BlockId{4}, .name = "format.container.item"};
            for (std::size_t index = 0; index < shape.arguments.size(); ++index)
            {
                if (index)
                    literal(body, ": ");
                const auto item = fresh(), rendered = fresh();
                body.instructions.push_back({.opcode = Opcode::IteratorValue,
                                             .result = item,
                                             .resultType = shape.arguments[index],
                                             .operands = {cursor},
                                             .selector = shape.kind == TypeKind::Array ? "__value__"
                                                         : index == 0                  ? "first"
                                                                                       : "second",
                                             .resultOwnership = ValueOwnership::Owned});
                body.instructions.push_back({.opcode = Opcode::Interpolate,
                                             .result = rendered,
                                             .resultType = string,
                                             .operands = {item},
                                             .signatureTypes = {shape.arguments[index]},
                                             .stringSegments = {"", ""},
                                             .intrinsicFamily = IntrinsicFamily::String,
                                             .resultOwnership = ValueOwnership::Owned});
                append(body, rendered);
                body.instructions.push_back({.opcode = Opcode::Release, .operands = {rendered}});
                if (requiresCleanup(module.types.get(shape.arguments[index])))
                    body.instructions.push_back({.opcode = Opcode::Release, .operands = {item}});
            }
            body.instructions.push_back({.opcode = Opcode::IteratorAdvance, .operands = {cursor}});
            body.instructions.push_back({.opcode = Opcode::Branch, .operands = {falsity}, .targets = {BlockId{1}}});
            BasicBlock exit{.id = BlockId{5}, .name = "format.container.exit"};
            literal(exit, shape.kind == TypeKind::Array ? "]" : "}");
            const auto value = fresh();
            exit.instructions.push_back({.opcode = Opcode::Move,
                                         .result = value,
                                         .resultType = string,
                                         .operands = {output},
                                         .resultOwnership = ValueOwnership::Owned});
            exit.instructions.push_back({.opcode = Opcode::Release, .operands = {cursor}});
            exit.instructions.push_back({.opcode = Opcode::Release, .operands = {ValueId{0}}});
            exit.instructions.push_back({.opcode = Opcode::Return, .operands = {value}});
            result.blocks = {std::move(entry),     std::move(header), std::move(separatorTest),
                             std::move(separator), std::move(body),   std::move(exit)};
            return result;
        }

        std::size_t lowerFunction(Module& module, const FunctionIndex& functions, const HelperIndex& helpers,
                                  Function& function)
        {
            if (!function.genericParameters.empty() || hasOpenType(module.types, function.ownerType))
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
                    bool forwarded = false;
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
                        else if (selected.surface == Surface::Container || selected.surface == Surface::Native)
                        {
                            const bool bare = instruction.resultType == string && instruction.operands.size() == 1 &&
                                              instruction.stringSegments[0].empty() &&
                                              instruction.stringSegments[1].empty();
                            // A normal owned call consumes its SSA argument. Give
                            // it a separate claim, never the original expression
                            // which the surrounding function still cleans up.
                            auto source = instruction.operands[index];
                            if (module.types.get(instruction.signatureTypes[index]).kind == TypeKind::Reference)
                            {
                                const auto loaded = fresh();
                                blocks[current].instructions.push_back(
                                    {.opcode = Opcode::Load,
                                     .result = loaded,
                                     .resultType = selected.owner,
                                     .operands = {source},
                                     .resultOwnership = requiresCleanup(module.types.get(selected.owner))
                                                            ? ValueOwnership::Borrowed
                                                            : ValueOwnership::Owned,
                                     .borrowLifetime = requiresCleanup(module.types.get(selected.owner))
                                                           ? BorrowLifetime::Lexical
                                                           : BorrowLifetime::None,
                                     .borrowOrigin =
                                         requiresCleanup(module.types.get(selected.owner)) ? source : ValueId{},
                                     .source = instruction.source});
                                source = loaded;
                            }
                            const auto argument = fresh();
                            blocks[current].instructions.push_back({.opcode = Opcode::Copy,
                                                                    .result = argument,
                                                                    .resultType = selected.owner,
                                                                    .operands = {source},
                                                                    .resultOwnership = ValueOwnership::Owned,
                                                                    .source = instruction.source});
                            blocks[current].instructions.push_back(
                                {.opcode = selected.surface == Surface::Native ? Opcode::NativeCall : Opcode::Call,
                                 .result = bare ? instruction.result : formatted,
                                 .resultType = string,
                                 .operands = {argument},
                                 .callee = helpers.at(selected.owner.value()),
                                 .signatureTypes = {selected.owner},
                                 .resultOwnership = ValueOwnership::Owned,
                                 .source = instruction.source});
                            if (bare)
                            {
                                // The call already owns the complete snapshot.
                                // Do not build/copy another interpolation string.
                                forwarded = true;
                                break;
                            }
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
                    if (forwarded)
                        continue;
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
        // Make a native method's implicit self explicit at the registered ABI
        // boundary. The public method remains virtual WIR dispatch; the VM
        // never tries to enter an external method as if it had a Wio body.
        FunctionId::ValueType nextFunction = 0;
        for (const auto& function : module.functions)
            nextFunction = std::max(nextFunction, function.id.value() + 1);
        std::vector<Function> adapters;
        for (auto& method : module.functions)
        {
            if (!method.isMethod || !method.nativeBinding || method.isAsync || !method.genericParameters.empty() ||
                hasOpenType(module.types, method.ownerType) || method.returnType != module.types.stringType() ||
                method.parameters.size() != 1)
                continue;
            const auto owner = module.types.get(method.ownerType);
            if (owner.nominalKind != NominalKind::Object && owner.nominalKind != NominalKind::Interface)
                continue;
            if (std::ranges::none_of(owner.methods,
                                     [&](const MethodLayout& entry)
                                     {
                                         return entry.function == method.id && entry.name == "ToString" &&
                                                entry.visibility == FieldVisibility::Public;
                                     }))
                continue;
            Function adapter{
                .id = FunctionId{nextFunction++},
                .name = "$format.native-method." + std::to_string(method.id.value()),
                .parameters = {{.id = ValueId{0}, .type = method.ownerType, .ownership = ValueOwnership::Owned}},
                .returnType = method.returnType,
                .isExternal = true};
            adapter.callableType =
                module.types.intern({.kind = TypeKind::Function, .arguments = {method.ownerType, method.returnType}});
            adapter.nativeBinding = *method.nativeBinding;
            auto& binding = *adapter.nativeBinding;
            binding.stableKey = module.contract.stableKey + ":format-native-method:" + binding.stableKey +
                                ":receiver:" + nativeAbiTypeKey(module.types, method.ownerType);
            binding.thunkSymbol = "_wio_format_method_" + std::to_string(stableModuleHash(binding.stableKey));
            binding.thunkKind = NativeThunkKind::Adapter;
            binding.requiresAdapter = true;
            binding.parameters.insert(binding.parameters.begin(), {.type = method.ownerType});
            refreshNativeAbiValue(module.types, binding.parameters.front(), false);
            ValueId::ValueType result = method.parameters[0].id.value() + 1;
            const ValueId borrowed{result++}, owned{result++};
            method.blocks = {
                {.id = BlockId{0},
                 .name = "format.native-method",
                 .instructions = {{.opcode = Opcode::Load,
                                   .result = borrowed,
                                   .resultType = method.ownerType,
                                   .operands = {method.parameters[0].id},
                                   .resultOwnership = ValueOwnership::Borrowed,
                                   .borrowLifetime = BorrowLifetime::Caller,
                                   .borrowOrigin = method.parameters[0].id,
                                   .source = method.source},
                                  {.opcode = Opcode::Copy,
                                   .result = owned,
                                   .resultType = method.ownerType,
                                   .operands = {borrowed},
                                   .resultOwnership = ValueOwnership::Owned,
                                   .source = method.source},
                                  {.opcode = Opcode::NativeCall,
                                   .result = ValueId{result},
                                   .resultType = method.returnType,
                                   .operands = {owned},
                                   .callee = adapter.id,
                                   .signatureTypes = {method.ownerType},
                                   .resultOwnership = ValueOwnership::Owned,
                                   .source = method.source},
                                  {.opcode = Opcode::Return, .operands = {ValueId{result}}, .source = method.source}}}};
            method.isExternal = false;
            method.nativeBinding.reset();
            adapters.push_back(std::move(adapter));
            ++changed;
        }
        for (auto& adapter : adapters)
            module.functions.push_back(std::move(adapter));
        FunctionIndex functions;
        functions.reserve(module.functions.size());
        for (const auto& function : module.functions)
            functions.emplace(function.id.value(), &function);
        HelperIndex helpers;
        std::vector<TypeId> required;
        const auto require = [&](auto&& self, TypeId id) -> void
        {
            const auto selected = formatter(module, functions, id);
            if (selected.surface != Surface::Native && selected.surface != Surface::Container)
                return;
            if (std::ranges::find(required, selected.owner) != required.end())
                return;
            required.push_back(selected.owner);
            if (selected.surface == Surface::Container)
                for (const auto child : module.types.get(selected.owner).arguments)
                    self(self, child);
        };
        for (const auto& function : module.functions)
        {
            if (!function.genericParameters.empty() || hasOpenType(module.types, function.ownerType))
                continue;
            for (const auto& block : function.blocks)
                for (const auto& instruction : block.instructions)
                    if (eligible(module, functions, instruction))
                        for (const auto type : instruction.signatureTypes)
                            require(require, type);
        }
        std::vector<Function> generated;
        for (const auto id : required)
        {
            const auto selected = formatter(module, functions, id);
            const FunctionId functionId{nextFunction++};
            helpers.emplace(id.value(), functionId);
            if (selected.surface == Surface::Container)
                generated.push_back(containerHelper(module, id, functionId));
            else
            {
                Function function{.id = functionId,
                                  .name = "$format.native." + std::to_string(id.value()),
                                  .returnType = module.types.stringType(),
                                  .isExternal = true};
                function.callableType =
                    module.types.intern({.kind = TypeKind::Function, .arguments = {id, module.types.stringType()}});
                function.parameters = {{.id = ValueId{0}, .type = id, .ownership = ValueOwnership::Owned}};
                NativeBinding binding{.symbol = "wio::wir_backend::format_single",
                                      .header = module.types.get(id).nativeBinding->header,
                                      .stableKey = module.contract.stableKey +
                                                   ":format-native:" + nativeAbiTypeKey(module.types, id),
                                      .parameters = {{.type = id}},
                                      .result = {.type = module.types.stringType()}};
                binding.thunkSymbol = "_wio_format_native_" + std::to_string(stableModuleHash(binding.stableKey));
                refreshNativeAbiValue(module.types, binding.parameters.front(), false);
                refreshNativeAbiValue(module.types, binding.result, true);
                function.nativeBinding = std::move(binding);
                generated.push_back(std::move(function));
            }
        }
        for (auto& function : generated)
            module.functions.push_back(std::move(function));
        functions.clear();
        for (const auto& function : module.functions)
            functions.emplace(function.id.value(), &function);
        for (auto& function : module.functions)
            changed += lowerFunction(module, functions, helpers, function);
        return changed;
    }
} // namespace wio::wir
