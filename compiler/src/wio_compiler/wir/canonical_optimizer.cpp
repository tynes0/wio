#include "wio/wir/canonical_optimizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace wio::wir
{
    namespace
    {
        using LiteralMap = std::unordered_map<ValueId::ValueType, typed::Literal>;
        using TypeMap = std::unordered_map<ValueId::ValueType, TypeId>;
        using ReplacementMap = std::unordered_map<ValueId::ValueType, ValueId>;

        bool isSignedInteger(const TypeKind kind)
        {
            return kind == TypeKind::I8 || kind == TypeKind::I16 || kind == TypeKind::I32 ||
                kind == TypeKind::I64 || kind == TypeKind::ISize;
        }

        bool isUnsignedInteger(const TypeKind kind)
        {
            return kind == TypeKind::U8 || kind == TypeKind::U16 || kind == TypeKind::U32 ||
                kind == TypeKind::U64 || kind == TypeKind::USize || kind == TypeKind::Byte;
        }

        bool isFloatingPoint(const TypeKind kind)
        {
            return kind == TypeKind::F32 || kind == TypeKind::F64;
        }

        std::pair<std::int64_t, std::int64_t> signedRange(const TypeKind kind)
        {
            switch (kind)
            {
            case TypeKind::I8: return {INT8_MIN, INT8_MAX};
            case TypeKind::I16: return {INT16_MIN, INT16_MAX};
            case TypeKind::I32: return {INT32_MIN, INT32_MAX};
            default: return {(std::numeric_limits<std::int64_t>::min)(),
                             (std::numeric_limits<std::int64_t>::max)()};
            }
        }

        std::uint64_t unsignedMaximum(const TypeKind kind)
        {
            switch (kind)
            {
            case TypeKind::U8:
            case TypeKind::Byte: return UINT8_MAX;
            case TypeKind::U16: return UINT16_MAX;
            case TypeKind::U32: return UINT32_MAX;
            default: return (std::numeric_limits<std::uint64_t>::max)();
            }
        }

        std::optional<std::int64_t> checkedAdd(const std::int64_t left, const std::int64_t right)
        {
            if ((right > 0 && left > (std::numeric_limits<std::int64_t>::max)() - right) ||
                (right < 0 && left < (std::numeric_limits<std::int64_t>::min)() - right))
                return std::nullopt;
            return left + right;
        }

        std::optional<std::int64_t> checkedSubtract(const std::int64_t left, const std::int64_t right)
        {
            if ((right < 0 && left > (std::numeric_limits<std::int64_t>::max)() + right) ||
                (right > 0 && left < (std::numeric_limits<std::int64_t>::min)() + right))
                return std::nullopt;
            return left - right;
        }

        std::optional<std::int64_t> checkedMultiply(const std::int64_t left, const std::int64_t right)
        {
            if (left == 0 || right == 0) return std::int64_t{0};
            if ((left == -1 && right == (std::numeric_limits<std::int64_t>::min)()) ||
                (right == -1 && left == (std::numeric_limits<std::int64_t>::min)()))
                return std::nullopt;
            if (left > 0)
            {
                if (right > 0 && left > (std::numeric_limits<std::int64_t>::max)() / right) return std::nullopt;
                if (right < 0 && right < (std::numeric_limits<std::int64_t>::min)() / left) return std::nullopt;
            }
            else
            {
                if (right > 0 && left < (std::numeric_limits<std::int64_t>::min)() / right) return std::nullopt;
                if (right < 0 && left < (std::numeric_limits<std::int64_t>::max)() / right) return std::nullopt;
            }
            return left * right;
        }

        template<typename Value>
        std::optional<typed::Literal> foldComparison(
            const typed::BinaryOperator operation,
            const Value& left,
            const Value& right)
        {
            switch (operation)
            {
            case typed::BinaryOperator::Equal: return typed::Literal{left == right};
            case typed::BinaryOperator::NotEqual: return typed::Literal{left != right};
            case typed::BinaryOperator::Less: return typed::Literal{left < right};
            case typed::BinaryOperator::LessEqual: return typed::Literal{left <= right};
            case typed::BinaryOperator::Greater: return typed::Literal{left > right};
            case typed::BinaryOperator::GreaterEqual: return typed::Literal{left >= right};
            default: return std::nullopt;
            }
        }

        std::optional<typed::Literal> foldSigned(
            const typed::BinaryOperator operation,
            const std::int64_t left,
            const std::int64_t right)
        {
            if (const auto comparison = foldComparison(operation, left, right)) return comparison;
            switch (operation)
            {
            case typed::BinaryOperator::Add:
                if (const auto value = checkedAdd(left, right)) return typed::Literal{*value};
                break;
            case typed::BinaryOperator::Subtract:
                if (const auto value = checkedSubtract(left, right)) return typed::Literal{*value};
                break;
            case typed::BinaryOperator::Multiply:
                if (const auto value = checkedMultiply(left, right)) return typed::Literal{*value};
                break;
            case typed::BinaryOperator::Divide:
                if (right != 0 && !(left == (std::numeric_limits<std::int64_t>::min)() && right == -1))
                    return typed::Literal{left / right};
                break;
            case typed::BinaryOperator::Remainder:
                if (right != 0 && !(left == (std::numeric_limits<std::int64_t>::min)() && right == -1))
                    return typed::Literal{left % right};
                break;
            case typed::BinaryOperator::BitwiseAnd: return typed::Literal{left & right};
            case typed::BinaryOperator::BitwiseOr: return typed::Literal{left | right};
            case typed::BinaryOperator::BitwiseXor: return typed::Literal{left ^ right};
            default: break;
            }
            return std::nullopt;
        }

        std::optional<typed::Literal> foldUnsigned(
            const typed::BinaryOperator operation,
            const std::uint64_t left,
            const std::uint64_t right)
        {
            if (const auto comparison = foldComparison(operation, left, right)) return comparison;
            switch (operation)
            {
            case typed::BinaryOperator::Add:
                if (left <= (std::numeric_limits<std::uint64_t>::max)() - right)
                    return typed::Literal{left + right};
                break;
            case typed::BinaryOperator::Subtract:
                if (left >= right) return typed::Literal{left - right};
                break;
            case typed::BinaryOperator::Multiply:
                if (right == 0 || left <= (std::numeric_limits<std::uint64_t>::max)() / right)
                    return typed::Literal{left * right};
                break;
            case typed::BinaryOperator::Divide:
                if (right != 0) return typed::Literal{left / right};
                break;
            case typed::BinaryOperator::Remainder:
                if (right != 0) return typed::Literal{left % right};
                break;
            case typed::BinaryOperator::BitwiseAnd: return typed::Literal{left & right};
            case typed::BinaryOperator::BitwiseOr: return typed::Literal{left | right};
            case typed::BinaryOperator::BitwiseXor: return typed::Literal{left ^ right};
            case typed::BinaryOperator::ShiftLeft:
                if (right < 64 && left <= (std::numeric_limits<std::uint64_t>::max)() >> right)
                    return typed::Literal{left << right};
                break;
            case typed::BinaryOperator::ShiftRight:
                if (right < 64) return typed::Literal{left >> right};
                break;
            default: break;
            }
            return std::nullopt;
        }

        std::optional<typed::Literal> foldFloating(
            const typed::BinaryOperator operation,
            const double left,
            const double right)
        {
            if (const auto comparison = foldComparison(operation, left, right)) return comparison;
            switch (operation)
            {
            case typed::BinaryOperator::Add: return typed::Literal{left + right};
            case typed::BinaryOperator::Subtract: return typed::Literal{left - right};
            case typed::BinaryOperator::Multiply: return typed::Literal{left * right};
            case typed::BinaryOperator::Divide:
                if (right != 0.0) return typed::Literal{left / right};
                break;
            case typed::BinaryOperator::Remainder:
                if (right != 0.0) return typed::Literal{std::fmod(left, right)};
                break;
            default: return std::nullopt;
            }
            return std::nullopt;
        }

        std::optional<typed::Literal> foldBinary(
            const typed::BinaryOperator operation,
            const typed::Literal& left,
            const typed::Literal& right)
        {
            if (const auto* signedLeft = std::get_if<std::int64_t>(&left))
                if (const auto* signedRight = std::get_if<std::int64_t>(&right))
                    return foldSigned(operation, *signedLeft, *signedRight);
            if (const auto* unsignedLeft = std::get_if<std::uint64_t>(&left))
                if (const auto* unsignedRight = std::get_if<std::uint64_t>(&right))
                    return foldUnsigned(operation, *unsignedLeft, *unsignedRight);
            if (const auto* floatingLeft = std::get_if<double>(&left))
                if (const auto* floatingRight = std::get_if<double>(&right))
                    return foldFloating(operation, *floatingLeft, *floatingRight);
            if (const auto* boolLeft = std::get_if<bool>(&left))
                if (const auto* boolRight = std::get_if<bool>(&right))
                    return foldComparison(operation, *boolLeft, *boolRight);
            if (const auto* stringLeft = std::get_if<std::string>(&left))
                if (const auto* stringRight = std::get_if<std::string>(&right))
                    return foldComparison(operation, *stringLeft, *stringRight);
            return std::nullopt;
        }

        std::optional<typed::Literal> foldUnary(
            const typed::UnaryOperator operation,
            const typed::Literal& operand)
        {
            if (const auto* value = std::get_if<bool>(&operand))
                return operation == typed::UnaryOperator::LogicalNot
                    ? std::optional<typed::Literal>{typed::Literal{!*value}}
                    : std::nullopt;
            if (const auto* value = std::get_if<std::int64_t>(&operand))
            {
                if (operation == typed::UnaryOperator::Negate &&
                    *value != (std::numeric_limits<std::int64_t>::min)())
                    return typed::Literal{-*value};
                if (operation == typed::UnaryOperator::BitwiseNot)
                    return typed::Literal{~*value};
            }
            if (const auto* value = std::get_if<std::uint64_t>(&operand))
                if (operation == typed::UnaryOperator::BitwiseNot)
                    return typed::Literal{~*value};
            if (const auto* value = std::get_if<double>(&operand))
                if (operation == typed::UnaryOperator::Negate)
                    return typed::Literal{-*value};
            return std::nullopt;
        }

        std::optional<typed::Literal> foldConversion(const typed::Literal& value, const Type& destination)
        {
            // Pointer-sized integer width is a backend target property. Keep
            // those conversions explicit until a concrete target is selected.
            if (destination.kind == TypeKind::ISize || destination.kind == TypeKind::USize)
                return std::nullopt;
            if (isFloatingPoint(destination.kind))
            {
                double converted = 0.0;
                if (const auto* signedValue = std::get_if<std::int64_t>(&value)) converted = static_cast<double>(*signedValue);
                else if (const auto* unsignedValue = std::get_if<std::uint64_t>(&value)) converted = static_cast<double>(*unsignedValue);
                else if (const auto* floatingValue = std::get_if<double>(&value)) converted = *floatingValue;
                else return std::nullopt;
                if (destination.kind == TypeKind::F32)
                    converted = static_cast<double>(static_cast<float>(converted));
                return typed::Literal{converted};
            }
            if (isSignedInteger(destination.kind))
            {
                const auto [minimum, maximum] = signedRange(destination.kind);
                std::int64_t converted = 0;
                if (const auto* signedValue = std::get_if<std::int64_t>(&value)) converted = *signedValue;
                else if (const auto* unsignedValue = std::get_if<std::uint64_t>(&value))
                {
                    if (*unsignedValue > static_cast<std::uint64_t>(maximum)) return std::nullopt;
                    converted = static_cast<std::int64_t>(*unsignedValue);
                }
                else if (const auto* floatingValue = std::get_if<double>(&value))
                {
                    if (!std::isfinite(*floatingValue) || std::trunc(*floatingValue) != *floatingValue ||
                        *floatingValue < static_cast<double>(minimum) || *floatingValue > static_cast<double>(maximum))
                        return std::nullopt;
                    converted = static_cast<std::int64_t>(*floatingValue);
                }
                else return std::nullopt;
                if (converted < minimum || converted > maximum) return std::nullopt;
                return typed::Literal{converted};
            }
            if (isUnsignedInteger(destination.kind))
            {
                const std::uint64_t maximum = unsignedMaximum(destination.kind);
                std::uint64_t converted = 0;
                if (const auto* unsignedValue = std::get_if<std::uint64_t>(&value)) converted = *unsignedValue;
                else if (const auto* signedValue = std::get_if<std::int64_t>(&value))
                {
                    if (*signedValue < 0) return std::nullopt;
                    converted = static_cast<std::uint64_t>(*signedValue);
                }
                else if (const auto* floatingValue = std::get_if<double>(&value))
                {
                    if (!std::isfinite(*floatingValue) || std::trunc(*floatingValue) != *floatingValue ||
                        *floatingValue < 0.0 || *floatingValue > static_cast<double>(maximum))
                        return std::nullopt;
                    converted = static_cast<std::uint64_t>(*floatingValue);
                }
                else return std::nullopt;
                if (converted > maximum) return std::nullopt;
                return typed::Literal{converted};
            }
            return std::nullopt;
        }

        template<typename Value>
        std::optional<typed::Literal> foldRange(
            const std::string& selector,
            const Value& value,
            const Value& start,
            const Value& end)
        {
            return typed::Literal{value >= start &&
                (selector == "inclusive" ? value <= end : value < end)};
        }

        std::optional<typed::Literal> foldRangeContains(
            const std::string& selector,
            const typed::Literal& value,
            const typed::Literal& start,
            const typed::Literal& end)
        {
            if (const auto* item = std::get_if<std::int64_t>(&value))
                if (const auto* first = std::get_if<std::int64_t>(&start))
                    if (const auto* last = std::get_if<std::int64_t>(&end))
                        return foldRange(selector, *item, *first, *last);
            if (const auto* item = std::get_if<std::uint64_t>(&value))
                if (const auto* first = std::get_if<std::uint64_t>(&start))
                    if (const auto* last = std::get_if<std::uint64_t>(&end))
                        return foldRange(selector, *item, *first, *last);
            if (const auto* item = std::get_if<double>(&value))
                if (const auto* first = std::get_if<double>(&start))
                    if (const auto* last = std::get_if<double>(&end))
                        return foldRange(selector, *item, *first, *last);
            return std::nullopt;
        }

        std::optional<typed::Literal> normalizeFoldedLiteral(
            typed::Literal literal,
            const Type& type)
        {
            if (type.kind == TypeKind::ISize || type.kind == TypeKind::USize)
                return std::nullopt;
            if (type.kind == TypeKind::Bool)
                return std::holds_alternative<bool>(literal)
                    ? std::optional<typed::Literal>{std::move(literal)} : std::nullopt;
            if (isSignedInteger(type.kind))
            {
                const auto* value = std::get_if<std::int64_t>(&literal);
                if (!value) return std::nullopt;
                const auto [minimum, maximum] = signedRange(type.kind);
                return *value >= minimum && *value <= maximum
                    ? std::optional<typed::Literal>{std::move(literal)} : std::nullopt;
            }
            if (isUnsignedInteger(type.kind))
            {
                const auto* value = std::get_if<std::uint64_t>(&literal);
                return value && *value <= unsignedMaximum(type.kind)
                    ? std::optional<typed::Literal>{std::move(literal)} : std::nullopt;
            }
            if (isFloatingPoint(type.kind))
            {
                const auto* value = std::get_if<double>(&literal);
                if (!value) return std::nullopt;
                if (type.kind == TypeKind::F32)
                    literal = static_cast<double>(static_cast<float>(*value));
                return literal;
            }
            return literal;
        }

        LiteralMap collectConstants(const lowered::Function& function)
        {
            LiteralMap constants;
            for (const lowered::BasicBlock& block : function.blocks)
                for (const lowered::Instruction& instruction : block.instructions)
                    if (instruction.opcode == lowered::Opcode::Constant && instruction.result)
                        constants[instruction.result.value()] = instruction.literal;
            return constants;
        }

        TypeMap collectTypes(const lowered::Function& function)
        {
            TypeMap types;
            for (const lowered::Parameter& parameter : function.parameters)
                types[parameter.id.value()] = parameter.type;
            for (const lowered::BasicBlock& block : function.blocks)
            {
                for (const lowered::Parameter& parameter : block.parameters)
                    types[parameter.id.value()] = parameter.type;
                for (const lowered::Instruction& instruction : block.instructions)
                    if (instruction.result)
                        types[instruction.result.value()] = instruction.resultType;
            }
            return types;
        }

        std::size_t foldConstants(lowered::Module& module)
        {
            std::size_t folded = 0;
            for (lowered::Function& function : module.functions)
            {
                if (function.isExternal) continue;
                LiteralMap constants = collectConstants(function);
                for (lowered::BasicBlock& block : function.blocks)
                {
                    for (lowered::Instruction& instruction : block.instructions)
                    {
                        std::optional<typed::Literal> result;
                        if (instruction.opcode == lowered::Opcode::Unary && instruction.operands.size() == 1)
                        {
                            const auto operand = constants.find(instruction.operands.front().value());
                            if (operand != constants.end()) result = foldUnary(instruction.unaryOperator, operand->second);
                        }
                        else if (instruction.opcode == lowered::Opcode::Binary && instruction.operands.size() == 2)
                        {
                            const auto left = constants.find(instruction.operands[0].value());
                            const auto right = constants.find(instruction.operands[1].value());
                            if (left != constants.end() && right != constants.end())
                                result = foldBinary(instruction.binaryOperator, left->second, right->second);
                        }
                        else if (instruction.opcode == lowered::Opcode::Convert && instruction.operands.size() == 1)
                        {
                            const auto operand = constants.find(instruction.operands.front().value());
                            const Type* destination = module.types.tryGet(instruction.resultType);
                            if (operand != constants.end() && destination)
                                result = foldConversion(operand->second, *destination);
                        }
                        else if (instruction.opcode == lowered::Opcode::RangeContains && instruction.operands.size() == 3)
                        {
                            const auto value = constants.find(instruction.operands[0].value());
                            const auto start = constants.find(instruction.operands[1].value());
                            const auto end = constants.find(instruction.operands[2].value());
                            if (value != constants.end() && start != constants.end() && end != constants.end())
                                result = foldRangeContains(instruction.selector, value->second, start->second, end->second);
                        }
                        if (!result || !instruction.result) continue;
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        if (!resultType) continue;
                        result = normalizeFoldedLiteral(std::move(*result), *resultType);
                        if (!result) continue;
                        instruction.opcode = lowered::Opcode::Constant;
                        instruction.operands.clear();
                        instruction.targets.clear();
                        instruction.literal = std::move(*result);
                        instruction.selector.clear();
                        instruction.targetType = {};
                        constants[instruction.result.value()] = instruction.literal;
                        ++folded;
                    }
                }
            }
            return folded;
        }

        ValueId resolveReplacement(ValueId value, const ReplacementMap& replacements)
        {
            std::unordered_set<ValueId::ValueType> visited;
            while (value && visited.insert(value.value()).second)
            {
                const auto replacement = replacements.find(value.value());
                if (replacement == replacements.end()) break;
                value = replacement->second;
            }
            return value;
        }

        void applyReplacements(lowered::Function& function, const ReplacementMap& replacements)
        {
            if (replacements.empty()) return;
            for (lowered::BasicBlock& block : function.blocks)
                for (lowered::Instruction& instruction : block.instructions)
                {
                    for (ValueId& operand : instruction.operands)
                        operand = resolveReplacement(operand, replacements);
                    for (lowered::BranchTarget& target : instruction.targets)
                        for (ValueId& argument : target.arguments)
                            argument = resolveReplacement(argument, replacements);
                    if (instruction.borrowOrigin)
                        instruction.borrowOrigin = resolveReplacement(instruction.borrowOrigin, replacements);
                }
            if (!function.coroutine) return;
            for (CoroutineFrameSlot& slot : function.coroutine->frameSlots)
                slot.value = resolveReplacement(slot.value, replacements);
            for (CoroutineState& state : function.coroutine->states)
            {
                if (state.awaitedTask) state.awaitedTask = resolveReplacement(state.awaitedTask, replacements);
                if (state.resumedValue) state.resumedValue = resolveReplacement(state.resumedValue, replacements);
            }
        }

        void normalizeCoroutineSlots(lowered::Function& function)
        {
            if (!function.coroutine) return;
            std::unordered_set<ValueId::ValueType> values;
            std::erase_if(function.coroutine->frameSlots, [&](const CoroutineFrameSlot& slot)
            {
                return !values.insert(slot.value.value()).second;
            });
            for (std::size_t index = 0; index < function.coroutine->frameSlots.size(); ++index)
                function.coroutine->frameSlots[index].slot = static_cast<std::uint32_t>(index);
        }

        std::size_t propagateTrivialValues(lowered::Module& module)
        {
            std::size_t propagated = 0;
            for (lowered::Function& function : module.functions)
            {
                if (function.isExternal) continue;
                const TypeMap valueTypes = collectTypes(function);
                ReplacementMap replacements;
                std::unordered_set<ValueId::ValueType> removeResults;

                for (lowered::BasicBlock& block : function.blocks)
                    for (const lowered::Instruction& instruction : block.instructions)
                        if (instruction.opcode == lowered::Opcode::Convert && instruction.result &&
                            instruction.operands.size() == 1)
                        {
                            const auto operandType = valueTypes.find(instruction.operands.front().value());
                            if (operandType != valueTypes.end() && operandType->second == instruction.resultType)
                            {
                                replacements[instruction.result.value()] = instruction.operands.front();
                                removeResults.insert(instruction.result.value());
                            }
                        }

                for (lowered::BasicBlock& block : function.blocks)
                {
                    if (block.parameters.empty()) continue;
                    std::vector<lowered::BranchTarget*> incoming;
                    for (lowered::BasicBlock& predecessor : function.blocks)
                        for (lowered::Instruction& instruction : predecessor.instructions)
                            for (lowered::BranchTarget& target : instruction.targets)
                                if (target.block == block.id)
                                    incoming.push_back(&target);
                    if (incoming.empty()) continue;
                    for (std::size_t parameterIndex = block.parameters.size(); parameterIndex > 0; --parameterIndex)
                    {
                        const std::size_t index = parameterIndex - 1;
                        const Type* parameterType = module.types.tryGet(block.parameters[index].type);
                        if (!parameterType || parameterType->ownership != OwnershipModel::Trivial) continue;
                        if (std::ranges::any_of(incoming, [&](const lowered::BranchTarget* target)
                            { return target->arguments.size() <= index; }))
                            continue;
                        const ValueId candidate = incoming.front()->arguments[index];
                        if (!std::ranges::all_of(incoming, [&](const lowered::BranchTarget* target)
                            { return target->arguments[index] == candidate; }))
                            continue;
                        replacements[block.parameters[index].id.value()] = candidate;
                        block.parameters.erase(block.parameters.begin() + static_cast<std::ptrdiff_t>(index));
                        for (lowered::BranchTarget* target : incoming)
                            target->arguments.erase(target->arguments.begin() + static_cast<std::ptrdiff_t>(index));
                        ++propagated;
                    }
                }

                applyReplacements(function, replacements);
                for (lowered::BasicBlock& block : function.blocks)
                    std::erase_if(block.instructions, [&](const lowered::Instruction& instruction)
                    {
                        if (!instruction.result || !removeResults.contains(instruction.result.value())) return false;
                        ++propagated;
                        return true;
                    });
                normalizeCoroutineSlots(function);
            }
            return propagated;
        }

        std::size_t simplifyBranches(lowered::Module& module)
        {
            std::size_t simplified = 0;
            for (lowered::Function& function : module.functions)
            {
                LiteralMap constants = collectConstants(function);
                for (lowered::BasicBlock& block : function.blocks)
                {
                    if (block.instructions.empty()) continue;
                    lowered::Instruction& terminator = block.instructions.back();
                    if (terminator.opcode != lowered::Opcode::CondJump || terminator.operands.size() != 1 ||
                        terminator.targets.size() != 2)
                        continue;
                    const auto condition = constants.find(terminator.operands.front().value());
                    if (condition == constants.end()) continue;
                    const auto* value = std::get_if<bool>(&condition->second);
                    if (!value) continue;
                    const lowered::BranchTarget target = terminator.targets[*value ? 0 : 1];
                    terminator = lowered::Instruction{
                        .opcode = lowered::Opcode::Jump,
                        .targets = {target},
                        .source = terminator.source
                    };
                    ++simplified;
                }

                // Thread jumps through parameterless forwarding blocks. The
                // loop is bounded by the number of blocks to remain robust in
                // malformed cyclic input (the verifier runs again afterwards).
                std::unordered_map<BlockId::ValueType, lowered::BasicBlock*> blocks;
                for (lowered::BasicBlock& block : function.blocks) blocks[block.id.value()] = &block;
                for (lowered::BasicBlock& block : function.blocks)
                    for (lowered::Instruction& instruction : block.instructions)
                        for (lowered::BranchTarget& target : instruction.targets)
                        {
                            std::unordered_set<BlockId::ValueType> visited;
                            while (target.block && visited.insert(target.block.value()).second)
                            {
                                const auto next = blocks.find(target.block.value());
                                if (next == blocks.end() || !next->second->parameters.empty() ||
                                    next->second->instructions.size() != 1 ||
                                    next->second->instructions.front().opcode != lowered::Opcode::Jump ||
                                    !target.arguments.empty())
                                    break;
                                target = next->second->instructions.front().targets.front();
                                ++simplified;
                            }
                        }
            }
            return simplified;
        }

        std::size_t removeUnreachableBlocks(lowered::Module& module)
        {
            std::size_t removed = 0;
            for (lowered::Function& function : module.functions)
            {
                if (function.blocks.empty()) continue;
                std::unordered_map<BlockId::ValueType, const lowered::BasicBlock*> blocks;
                for (const lowered::BasicBlock& block : function.blocks) blocks[block.id.value()] = &block;
                std::unordered_set<BlockId::ValueType> reachable;
                std::vector<BlockId> pending{function.blocks.front().id};
                while (!pending.empty())
                {
                    const BlockId id = pending.back();
                    pending.pop_back();
                    if (!reachable.insert(id.value()).second) continue;
                    const auto block = blocks.find(id.value());
                    if (block == blocks.end()) continue;
                    for (const lowered::Instruction& instruction : block->second->instructions)
                        for (const lowered::BranchTarget& target : instruction.targets)
                            pending.push_back(target.block);
                }
                const std::size_t before = function.blocks.size();
                std::erase_if(function.blocks, [&](const lowered::BasicBlock& block)
                    { return !reachable.contains(block.id.value()); });
                removed += before - function.blocks.size();
                if (function.coroutine)
                {
                    std::unordered_map<std::uint32_t, std::uint32_t> stateRemap;
                    std::vector<CoroutineState> retainedStates;
                    retainedStates.reserve(function.coroutine->states.size());
                    for (CoroutineState state : function.coroutine->states)
                    {
                        if (!reachable.contains(state.suspendBlock.value()) ||
                            !reachable.contains(state.resumeBlock.value()))
                            continue;
                        const std::uint32_t newIndex = static_cast<std::uint32_t>(retainedStates.size());
                        stateRemap[state.index] = newIndex;
                        state.index = newIndex;
                        retainedStates.push_back(std::move(state));
                    }
                    function.coroutine->states = std::move(retainedStates);
                    for (lowered::BasicBlock& block : function.blocks)
                        for (lowered::Instruction& instruction : block.instructions)
                            if (instruction.opcode == lowered::Opcode::CancellationCheck ||
                                instruction.opcode == lowered::Opcode::CoroutineSuspend ||
                                instruction.opcode == lowered::Opcode::CoroutineResume)
                                if (const auto state = stateRemap.find(instruction.projectionIndex);
                                    state != stateRemap.end())
                                    instruction.projectionIndex = state->second;

                    std::unordered_set<ValueId::ValueType> definedValues;
                    for (const lowered::Parameter& parameter : function.parameters)
                        definedValues.insert(parameter.id.value());
                    for (const lowered::BasicBlock& block : function.blocks)
                    {
                        for (const lowered::Parameter& parameter : block.parameters)
                            definedValues.insert(parameter.id.value());
                        for (const lowered::Instruction& instruction : block.instructions)
                            if (instruction.result) definedValues.insert(instruction.result.value());
                    }
                    std::erase_if(function.coroutine->frameSlots, [&](const CoroutineFrameSlot& slot)
                        { return !definedValues.contains(slot.value.value()); });
                    normalizeCoroutineSlots(function);
                }
            }
            return removed;
        }

        bool isPureDiscardable(const lowered::Opcode opcode)
        {
            return opcode == lowered::Opcode::Constant || opcode == lowered::Opcode::Unary ||
                opcode == lowered::Opcode::Binary || opcode == lowered::Opcode::RangeContains ||
                opcode == lowered::Opcode::Convert || opcode == lowered::Opcode::FunctionReference ||
                opcode == lowered::Opcode::Upcast || opcode == lowered::Opcode::TypeTest ||
                opcode == lowered::Opcode::IdentityEqual || opcode == lowered::Opcode::VariantTest ||
                opcode == lowered::Opcode::ArrayLength || opcode == lowered::Opcode::EnumConstant ||
                opcode == lowered::Opcode::AnyTypeTest;
        }

        std::size_t eliminateDeadValues(lowered::Module& module)
        {
            std::size_t removed = 0;
            for (lowered::Function& function : module.functions)
            {
                bool changed = true;
                while (changed)
                {
                    changed = false;
                    std::unordered_set<ValueId::ValueType> used;
                    for (const lowered::BasicBlock& block : function.blocks)
                        for (const lowered::Instruction& instruction : block.instructions)
                        {
                            for (const ValueId operand : instruction.operands) used.insert(operand.value());
                            for (const lowered::BranchTarget& target : instruction.targets)
                                for (const ValueId argument : target.arguments) used.insert(argument.value());
                            if (instruction.borrowOrigin) used.insert(instruction.borrowOrigin.value());
                        }
                    if (function.coroutine)
                    {
                        for (const CoroutineFrameSlot& slot : function.coroutine->frameSlots) used.insert(slot.value.value());
                        for (const CoroutineState& state : function.coroutine->states)
                        {
                            if (state.awaitedTask) used.insert(state.awaitedTask.value());
                            if (state.resumedValue) used.insert(state.resumedValue.value());
                        }
                    }
                    for (lowered::BasicBlock& block : function.blocks)
                        std::erase_if(block.instructions, [&](const lowered::Instruction& instruction)
                        {
                            if (!instruction.result || used.contains(instruction.result.value()) ||
                                !isPureDiscardable(instruction.opcode))
                                return false;
                            const Type* type = module.types.tryGet(instruction.resultType);
                            if (!type || requiresCleanup(*type)) return false;
                            ++removed;
                            changed = true;
                            return true;
                        });
                }
            }
            return removed;
        }

        lowered::EscapeClass mergeEscape(
            const lowered::EscapeClass current,
            const lowered::EscapeClass candidate)
        {
            return static_cast<std::uint8_t>(candidate) > static_cast<std::uint8_t>(current)
                ? candidate : current;
        }

        bool isCall(const lowered::Opcode opcode)
        {
            return opcode == lowered::Opcode::Call || opcode == lowered::Opcode::NativeInvoke ||
                opcode == lowered::Opcode::IndirectCall || opcode == lowered::Opcode::ExtensionCall ||
                opcode == lowered::Opcode::MethodCall || opcode == lowered::Opcode::VirtualCall ||
                opcode == lowered::Opcode::InterfaceCall || opcode == lowered::Opcode::IntrinsicCall;
        }

        bool isAllocationCandidate(const lowered::Opcode opcode)
        {
            return opcode == lowered::Opcode::LocalPlace || opcode == lowered::Opcode::ConstructComponent ||
                opcode == lowered::Opcode::ConstructObject || opcode == lowered::Opcode::ClosureCreate ||
                opcode == lowered::Opcode::ArrayCreate || opcode == lowered::Opcode::DictionaryCreate ||
                opcode == lowered::Opcode::Interpolate || opcode == lowered::Opcode::AnyBox ||
                opcode == lowered::Opcode::NullableWrap || opcode == lowered::Opcode::IteratorCreate;
        }

        void classifyStorage(lowered::Module& module, OptimizationStatistics& statistics)
        {
            for (lowered::Function& function : module.functions)
            {
                std::unordered_map<ValueId::ValueType, lowered::EscapeClass> escapes;
                for (const lowered::BasicBlock& block : function.blocks)
                    for (const lowered::Instruction& instruction : block.instructions)
                    {
                        lowered::EscapeClass use = lowered::EscapeClass::Local;
                        if (instruction.opcode == lowered::Opcode::Return ||
                            instruction.opcode == lowered::Opcode::CoroutineComplete ||
                            instruction.opcode == lowered::Opcode::ResultPropagate)
                            use = lowered::EscapeClass::Return;
                        else if (instruction.opcode == lowered::Opcode::Store ||
                                 instruction.opcode == lowered::Opcode::PlaceInit ||
                                 instruction.opcode == lowered::Opcode::Replace ||
                                 instruction.opcode == lowered::Opcode::ClosureCreate ||
                                 instruction.opcode == lowered::Opcode::ArrayCreate ||
                                 instruction.opcode == lowered::Opcode::DictionaryCreate ||
                                 instruction.opcode == lowered::Opcode::ConstructComponent ||
                                 instruction.opcode == lowered::Opcode::ConstructObject)
                            use = lowered::EscapeClass::Store;
                        else if (isCall(instruction.opcode))
                            use = lowered::EscapeClass::Call;
                        else if (instruction.opcode == lowered::Opcode::CoroutineSuspend)
                            use = lowered::EscapeClass::Coroutine;
                        for (const ValueId operand : instruction.operands)
                            escapes[operand.value()] = mergeEscape(escapes[operand.value()], use);
                    }
                if (function.coroutine)
                    for (const CoroutineFrameSlot& slot : function.coroutine->frameSlots)
                        escapes[slot.value.value()] = lowered::EscapeClass::Coroutine;

                // Carry escape information backwards through identity-like
                // value wrappers. This keeps an escaping closure/object heap
                // decision intact even when retain/upcast/boxing sits between
                // construction and the observable use.
                bool changed = true;
                while (changed)
                {
                    changed = false;
                    for (const lowered::BasicBlock& block : function.blocks)
                        for (const lowered::Instruction& instruction : block.instructions)
                        {
                            const bool forwardsEscape = instruction.opcode == lowered::Opcode::Retain ||
                                instruction.opcode == lowered::Opcode::CopyValue ||
                                instruction.opcode == lowered::Opcode::Upcast ||
                                instruction.opcode == lowered::Opcode::Borrow ||
                                instruction.opcode == lowered::Opcode::AnyBox ||
                                instruction.opcode == lowered::Opcode::NullableWrap;
                            if (!forwardsEscape || !instruction.result) continue;
                            const lowered::EscapeClass resultEscape = escapes.contains(instruction.result.value())
                                ? escapes.at(instruction.result.value()) : lowered::EscapeClass::Local;
                            for (const ValueId operand : instruction.operands)
                            {
                                const lowered::EscapeClass previous = escapes[operand.value()];
                                const lowered::EscapeClass merged = mergeEscape(previous, resultEscape);
                                if (merged != previous)
                                {
                                    escapes[operand.value()] = merged;
                                    changed = true;
                                }
                            }
                        }
                }

                for (lowered::BasicBlock& block : function.blocks)
                    for (lowered::Instruction& instruction : block.instructions)
                    {
                        if (!instruction.result || !isAllocationCandidate(instruction.opcode)) continue;
                        instruction.escapeClass = escapes.contains(instruction.result.value())
                            ? escapes.at(instruction.result.value()) : lowered::EscapeClass::Local;
                        const Type* type = module.types.tryGet(instruction.resultType);
                        if (instruction.opcode == lowered::Opcode::ConstructObject ||
                                 instruction.opcode == lowered::Opcode::ArrayCreate ||
                                 instruction.opcode == lowered::Opcode::DictionaryCreate ||
                                 instruction.opcode == lowered::Opcode::Interpolate ||
                                 instruction.opcode == lowered::Opcode::AnyBox ||
                                 instruction.opcode == lowered::Opcode::IteratorCreate ||
                                 (type && type->ownership == OwnershipModel::ReferenceCounted) ||
                                 (instruction.opcode == lowered::Opcode::ClosureCreate &&
                                  instruction.escapeClass != lowered::EscapeClass::Local))
                        {
                            instruction.storageClass = lowered::StorageClass::Heap;
                            ++statistics.heapAllocations;
                        }
                        else if (instruction.escapeClass == lowered::EscapeClass::Coroutine)
                        {
                            instruction.storageClass = lowered::StorageClass::CoroutineFrame;
                            ++statistics.coroutineFrameAllocations;
                        }
                        else
                        {
                            instruction.storageClass = lowered::StorageClass::Stack;
                            ++statistics.stackAllocations;
                        }
                    }
            }
        }

        std::optional<std::uint64_t> nonNegativeIndex(const typed::Literal& literal)
        {
            if (const auto* value = std::get_if<std::uint64_t>(&literal)) return *value;
            if (const auto* value = std::get_if<std::int64_t>(&literal))
                if (*value >= 0) return static_cast<std::uint64_t>(*value);
            return std::nullopt;
        }

        std::size_t eliminateBoundsChecks(lowered::Module& module)
        {
            std::size_t eliminated = 0;
            for (lowered::Function& function : module.functions)
            {
                const LiteralMap constants = collectConstants(function);
                const TypeMap valueTypes = collectTypes(function);
                std::unordered_map<ValueId::ValueType, const lowered::Instruction*> producers;
                for (const lowered::BasicBlock& block : function.blocks)
                    for (const lowered::Instruction& instruction : block.instructions)
                        if (instruction.result) producers[instruction.result.value()] = &instruction;

                for (lowered::BasicBlock& block : function.blocks)
                    for (lowered::Instruction& instruction : block.instructions)
                    {
                        if ((instruction.opcode != lowered::Opcode::ArrayGet &&
                             instruction.opcode != lowered::Opcode::ArrayPlace) ||
                            instruction.operands.size() != 2)
                            continue;
                        instruction.boundsCheck = lowered::BoundsCheckMode::Required;
                        const auto indexLiteral = constants.find(instruction.operands[1].value());
                        if (indexLiteral == constants.end()) continue;
                        const auto index = nonNegativeIndex(indexLiteral->second);
                        if (!index) continue;

                        const auto baseTypeId = valueTypes.find(instruction.operands[0].value());
                        const Type* baseType = baseTypeId == valueTypes.end()
                            ? nullptr : module.types.tryGet(baseTypeId->second);
                        const Type* arrayType = baseType;
                        if (baseType && baseType->kind == TypeKind::Reference && baseType->arguments.size() == 1)
                            arrayType = module.types.tryGet(baseType->arguments.front());
                        if (arrayType && arrayType->kind == TypeKind::Array && arrayType->staticExtent &&
                            *index < *arrayType->staticExtent)
                        {
                            instruction.boundsCheck = lowered::BoundsCheckMode::EliminatedStatic;
                            ++eliminated;
                            continue;
                        }
                        const auto producer = producers.find(instruction.operands[0].value());
                        if (producer != producers.end() && producer->second->opcode == lowered::Opcode::ArrayCreate &&
                            *index < producer->second->operands.size())
                        {
                            instruction.boundsCheck = lowered::BoundsCheckMode::EliminatedProven;
                            ++eliminated;
                        }
                    }
            }
            return eliminated;
        }
    }

    OptimizationStatistics CanonicalOptimizer::optimize(lowered::Module& module) const
    {
        OptimizationStatistics statistics;
        statistics.constantsFolded = foldConstants(module);
        statistics.branchesSimplified = simplifyBranches(module);
        statistics.blocksRemoved = removeUnreachableBlocks(module);
        statistics.valuesPropagated = propagateTrivialValues(module);
        statistics.branchesSimplified += simplifyBranches(module);
        statistics.blocksRemoved += removeUnreachableBlocks(module);
        statistics.instructionsRemoved = eliminateDeadValues(module);
        classifyStorage(module, statistics);
        statistics.boundsChecksEliminated = eliminateBoundsChecks(module);
        return statistics;
    }
}
