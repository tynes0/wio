#include "wio/wir/lowered_ir_verifier.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace wio::wir::lowered
{
    namespace
    {
        using FunctionMap = std::unordered_map<FunctionId::ValueType, const Function*>;
        using BlockMap = std::unordered_map<BlockId::ValueType, const BasicBlock*>;
        using ValueTypeMap = std::unordered_map<ValueId::ValueType, TypeId>;

        bool isComparison(const typed::BinaryOperator op)
        {
            return op == typed::BinaryOperator::Equal || op == typed::BinaryOperator::NotEqual ||
                   op == typed::BinaryOperator::Less || op == typed::BinaryOperator::LessEqual ||
                   op == typed::BinaryOperator::Greater || op == typed::BinaryOperator::GreaterEqual;
        }

        bool isNumeric(const TypeKind kind)
        {
            return kind == TypeKind::I8 || kind == TypeKind::I16 || kind == TypeKind::I32 ||
                   kind == TypeKind::I64 || kind == TypeKind::ISize ||
                   kind == TypeKind::U8 || kind == TypeKind::U16 || kind == TypeKind::U32 ||
                   kind == TypeKind::U64 || kind == TypeKind::USize ||
                   kind == TypeKind::F32 || kind == TypeKind::F64;
        }

        bool isInteger(const TypeKind kind)
        {
            return isNumeric(kind) && kind != TypeKind::F32 && kind != TypeKind::F64;
        }

        const FieldLayout* findFieldLayout(
            const TypeTable& types,
            const Type& owner,
            const std::string_view name,
            std::unordered_set<TypeId::ValueType>& visited)
        {
            const auto field = std::ranges::find_if(
                owner.fields,
                [&](const FieldLayout& layout) { return layout.name == name; });
            if (field != owner.fields.end())
                return &*field;
            for (const TypeId baseTypeId : owner.baseTypes)
            {
                if (!baseTypeId || !visited.insert(baseTypeId.value()).second)
                    continue;
                const Type* baseType = types.tryGet(baseTypeId);
                if (baseType)
                {
                    if (const FieldLayout* inherited = findFieldLayout(types, *baseType, name, visited))
                        return inherited;
                }
            }
            return nullptr;
        }
    }

    VerificationResult Verifier::verify(const Module& module) const
    {
        VerificationResult result;
        auto report = [&](std::string code,
                          std::string message,
                          const SourceSpan& source = {},
                          const FunctionId function = {},
                          const BlockId block = {})
        {
            result.diagnostics_.push_back(VerificationDiagnostic{
                .code = std::move(code),
                .message = std::move(message),
                .source = source,
                .function = function,
                .block = block
            });
        };

        if (module.name.empty())
            report("LIR1000", "Lowered WIR module name cannot be empty.");
        for (const Type& type : module.types.types())
        {
            if (type.kind == TypeKind::Invalid)
                report("LIR1001", "Lowered WIR type table contains an invalid type.");
            for (const TypeId argument : type.arguments)
            {
                if (!module.types.tryGet(argument))
                    report("LIR1002", "Lowered WIR type references an unknown type id.");
            }
            if (type.kind != TypeKind::Named && type.nominalKind != NominalKind::None)
                report("LIR1003", "Only named Lowered WIR types may carry a nominal kind.");
            if (type.nominalRepresentation == NominalRepresentation::NativePod &&
                (type.kind != TypeKind::Named || type.nominalKind != NominalKind::Component))
            {
                report("LIR1004", "Native POD representation requires a named component type.");
            }
            if (type.kind != TypeKind::Named &&
                (!type.baseTypes.empty() || !type.fields.empty() || type.hasConstructor || type.hasDestructor))
            {
                report("LIR1005", "Only named Lowered WIR types may carry layout or lifecycle metadata.");
            }
            std::unordered_set<std::string> fieldNames;
            for (const FieldLayout& field : type.fields)
            {
                if (field.name.empty() || !module.types.tryGet(field.type) || !fieldNames.insert(field.name).second)
                    report("LIR1006", "Lowered WIR field layouts require unique names and valid field types.");
            }
            for (const TypeId baseTypeId : type.baseTypes)
            {
                const Type* baseType = module.types.tryGet(baseTypeId);
                if (!baseType || baseType->kind != TypeKind::Named)
                    report("LIR1007", "Lowered WIR nominal base layouts must reference named types.");
            }
        }

        FunctionMap functions;
        for (const Function& function : module.functions)
        {
            if (!function.id)
                report("LIR1100", "Lowered WIR function has an invalid id.", function.source);
            else if (!functions.emplace(function.id.value(), &function).second)
                report("LIR1101", "Lowered WIR function id is duplicated.", function.source, function.id);
            if (function.name.empty())
                report("LIR1102", "Lowered WIR function name cannot be empty.", function.source, function.id);
            if (!module.types.tryGet(function.returnType))
                report("LIR1103", "Lowered WIR function return type is invalid.", function.source, function.id);
        }

        for (const Function& function : module.functions)
        {
            ValueTypeMap values;
            std::unordered_map<ValueId::ValueType, Opcode> producerOpcodes;
            auto defineValue = [&](const Parameter& parameter, const BlockId block)
            {
                if (!parameter.id)
                {
                    report("LIR1200", "Lowered WIR value has an invalid id.", parameter.source, function.id, block);
                    return;
                }
                if (!module.types.tryGet(parameter.type))
                    report("LIR1201", "Lowered WIR value has an invalid type.", parameter.source, function.id, block);
                if (!values.emplace(parameter.id.value(), parameter.type).second)
                    report("LIR1202", "Lowered WIR value id is defined more than once.", parameter.source, function.id, block);
            };
            for (const Parameter& parameter : function.parameters)
                defineValue(parameter, {});

            if (function.isExternal)
            {
                if (!function.blocks.empty())
                    report("LIR1104", "External Lowered WIR functions cannot contain blocks.", function.source, function.id);
                continue;
            }
            if (function.blocks.empty())
            {
                report("LIR1105", "Defined Lowered WIR functions require at least one block.", function.source, function.id);
                continue;
            }

            BlockMap blocks;
            for (const BasicBlock& block : function.blocks)
            {
                if (!block.id)
                    report("LIR1300", "Lowered WIR block has an invalid id.", block.source, function.id);
                else if (!blocks.emplace(block.id.value(), &block).second)
                    report("LIR1301", "Lowered WIR block id is duplicated.", block.source, function.id, block.id);
                for (const Parameter& parameter : block.parameters)
                    defineValue(parameter, block.id);
                for (const Instruction& instruction : block.instructions)
                {
                    if (instruction.result)
                    {
                        defineValue(Parameter{
                            .id = instruction.result,
                            .type = instruction.resultType,
                            .source = instruction.source
                        }, block.id);
                        producerOpcodes.emplace(instruction.result.value(), instruction.opcode);
                    }
                }
            }

            auto valueType = [&](const ValueId value) -> TypeId
            {
                if (!value)
                    return {};
                const auto found = values.find(value.value());
                return found == values.end() ? TypeId{} : found->second;
            };

            std::unordered_set<ValueId::ValueType> initializedPlaces;
            for (const BasicBlock& block : function.blocks)
            {
                if (block.instructions.empty())
                {
                    report("LIR1302", "Lowered WIR block requires a terminator.", block.source, function.id, block.id);
                    continue;
                }

                for (std::size_t index = 0; index < block.instructions.size(); ++index)
                {
                    const Instruction& instruction = block.instructions[index];
                    const bool terminator = isTerminator(instruction.opcode);
                    if (terminator != (index + 1 == block.instructions.size()))
                    {
                        report(
                            terminator ? "LIR1303" : "LIR1304",
                            terminator ? "Lowered WIR terminator must be the final instruction in its block."
                                       : "Lowered WIR block must end with a terminator.",
                            instruction.source,
                            function.id,
                            block.id);
                    }

                    for (const ValueId operand : instruction.operands)
                    {
                        if (!valueType(operand))
                            report("LIR1400", "Lowered WIR instruction references an unknown value.", instruction.source, function.id, block.id);
                    }

                    bool callReturnsVoid = false;
                    if (instruction.opcode == Opcode::Call && instruction.callee)
                    {
                        const auto calleeIt = functions.find(instruction.callee.value());
                        if (calleeIt != functions.end())
                        {
                            const Type* returnType = module.types.tryGet(calleeIt->second->returnType);
                            callReturnsVoid = returnType && returnType->kind == TypeKind::Void;
                        }
                    }
                    const bool shouldHaveResult = producesValue(instruction.opcode) && !callReturnsVoid;
                    if (shouldHaveResult && (!instruction.result || !module.types.tryGet(instruction.resultType)))
                        report("LIR1401", "Value-producing Lowered WIR instruction requires a typed result.", instruction.source, function.id, block.id);
                    if (!shouldHaveResult && instruction.opcode != Opcode::Call && instruction.result)
                        report("LIR1402", "Lowered WIR terminator cannot produce a value.", instruction.source, function.id, block.id);

                    if (instruction.opcode == Opcode::Unary)
                    {
                        if (instruction.operands.size() != 1 || valueType(instruction.operands.front()) != instruction.resultType)
                            report("LIR1403", "Lowered WIR unary operand must match its result type.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::Binary)
                    {
                        if (instruction.operands.size() != 2 || valueType(instruction.operands[0]) != valueType(instruction.operands[1]))
                            report("LIR1404", "Lowered WIR binary operands must have the same type.", instruction.source, function.id, block.id);
                        else if (isComparison(instruction.binaryOperator) && instruction.resultType != module.types.boolType())
                            report("LIR1405", "Lowered WIR comparison result must be bool.", instruction.source, function.id, block.id);
                        else if (!isComparison(instruction.binaryOperator) && instruction.resultType != valueType(instruction.operands[0]))
                            report("LIR1406", "Lowered WIR binary result must match its operand type.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::Convert)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* destinationType = module.types.tryGet(instruction.resultType);
                        if (!sourceType || !destinationType ||
                            !isNumeric(sourceType->kind) || !isNumeric(destinationType->kind))
                        {
                            report("LIR1417", "Lowered WIR numeric conversion requires one numeric operand and a numeric result.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::VariantTest)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        if (!sourceType || sourceType->kind != TypeKind::Named ||
                            instruction.resultType != module.types.boolType() || instruction.selector.empty())
                        {
                            report("LIR1418", "Lowered WIR variant test requires one named value, a variant selector, and a bool result.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::VariantPayload)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        if (!sourceType || sourceType->kind != TypeKind::Named || instruction.selector.empty())
                        {
                            report("LIR1419", "Lowered WIR variant payload requires one named value and a variant selector.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::ArrayLength)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        if (!sourceType || sourceType->kind != TypeKind::Array ||
                            !resultType || resultType->kind != TypeKind::USize)
                        {
                            report("LIR1420", "Lowered WIR array length requires one array value and a usize result.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::ArrayElement)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        if (!sourceType || sourceType->kind != TypeKind::Array || sourceType->arguments.size() != 1 ||
                            sourceType->arguments.front() != instruction.resultType)
                        {
                            report("LIR1421", "Lowered WIR array element projection must return its array element type.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::ArrayCreate)
                    {
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        bool valid = resultType && resultType->kind == TypeKind::Array &&
                            resultType->arguments.size() == 1;
                        if (valid)
                        {
                            valid = std::ranges::all_of(
                                instruction.operands,
                                [&](const ValueId operand)
                                {
                                    return valueType(operand) == resultType->arguments.front();
                                });
                            valid = valid && (!resultType->staticExtent.has_value() ||
                                *resultType->staticExtent == instruction.operands.size());
                        }
                        if (!valid)
                            report("LIR1422", "Lowered WIR array creation requires element operands matching its array type and extent.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::ArrayGet)
                    {
                        const Type* arrayType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[0]))
                            : nullptr;
                        const Type* indexType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[1]))
                            : nullptr;
                        if (!arrayType || arrayType->kind != TypeKind::Array || arrayType->arguments.size() != 1 ||
                            !indexType || !isInteger(indexType->kind) ||
                            arrayType->arguments.front() != instruction.resultType)
                        {
                            report("LIR1423", "Lowered WIR array get requires an array, an integer index, and its element result type.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::LocalPlace)
                    {
                        const Type* placeType = module.types.tryGet(instruction.resultType);
                        if (!instruction.operands.empty() || !placeType || placeType->kind != TypeKind::Reference ||
                            placeType->arguments.size() != 1 || instruction.selector.empty())
                        {
                            report("LIR1424", "Lowered WIR local place requires a named reference result and no operands.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::PlaceInit || instruction.opcode == Opcode::Store)
                    {
                        const Type* placeType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[0]))
                            : nullptr;
                        const bool mutabilityValid = instruction.opcode == Opcode::PlaceInit ||
                            (placeType && placeType->isMutable);
                        if (!placeType || placeType->kind != TypeKind::Reference || placeType->arguments.size() != 1 ||
                            valueType(instruction.operands[1]) != placeType->arguments.front() || !mutabilityValid)
                        {
                            report(
                                instruction.opcode == Opcode::PlaceInit ? "LIR1425" : "LIR1426",
                                instruction.opcode == Opcode::PlaceInit
                                    ? "Lowered WIR place initialization requires a reference place and a matching value."
                                    : "Lowered WIR store requires a mutable reference place and a matching value.",
                                instruction.source, function.id, block.id);
                        }
                        if (instruction.opcode == Opcode::PlaceInit && instruction.operands.size() == 2)
                        {
                            const ValueId place = instruction.operands.front();
                            const auto producer = producerOpcodes.find(place.value());
                            if (producer == producerOpcodes.end() || producer->second != Opcode::LocalPlace ||
                                !initializedPlaces.insert(place.value()).second)
                            {
                                report("LIR1431", "Lowered WIR place initialization must target an uninitialized local place exactly once.", instruction.source, function.id, block.id);
                            }
                        }
                    }
                    else if (instruction.opcode == Opcode::Load)
                    {
                        const Type* placeType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        if (!placeType || placeType->kind != TypeKind::Reference || placeType->arguments.size() != 1 ||
                            instruction.resultType != placeType->arguments.front())
                        {
                            report("LIR1427", "Lowered WIR load requires a reference place and its referred result type.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::FieldPlace)
                    {
                        const Type* baseType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* placeType = module.types.tryGet(instruction.resultType);
                        const Type* baseValueType = baseType;
                        if (baseType && baseType->kind == TypeKind::Reference && baseType->arguments.size() == 1)
                            baseValueType = module.types.tryGet(baseType->arguments.front());
                        const bool strengthensMutability = baseType && baseType->kind == TypeKind::Reference &&
                            placeType && placeType->isMutable && !baseType->isMutable;
                        if (!baseValueType || baseValueType->kind != TypeKind::Named || !placeType ||
                            placeType->kind != TypeKind::Reference || placeType->arguments.size() != 1 ||
                            instruction.selector.empty() || strengthensMutability)
                        {
                            report("LIR1428", "Lowered WIR field place requires a named base, field selector, and non-strengthening reference result.", instruction.source, function.id, block.id);
                        }
                        else
                        {
                            std::unordered_set<TypeId::ValueType> visited;
                            const FieldLayout* field = findFieldLayout(module.types, *baseValueType, instruction.selector, visited);
                            if (!field || field->type != placeType->arguments.front() ||
                                (placeType->isMutable && !field->isMutable))
                            {
                                report("LIR1432", "Lowered WIR field place must match the nominal field layout and field mutability.", instruction.source, function.id, block.id);
                            }
                        }
                    }
                    else if (instruction.opcode == Opcode::ArrayPlace)
                    {
                        const Type* baseType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[0]))
                            : nullptr;
                        const Type* indexType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[1]))
                            : nullptr;
                        const Type* placeType = module.types.tryGet(instruction.resultType);
                        const Type* arrayType = baseType && baseType->kind == TypeKind::Reference && baseType->arguments.size() == 1
                            ? module.types.tryGet(baseType->arguments.front())
                            : nullptr;
                        if (!baseType || !arrayType || arrayType->kind != TypeKind::Array || arrayType->arguments.size() != 1 ||
                            !indexType || !isInteger(indexType->kind) || !placeType || placeType->kind != TypeKind::Reference ||
                            placeType->arguments.size() != 1 || placeType->arguments.front() != arrayType->arguments.front() ||
                            (placeType->isMutable && !baseType->isMutable))
                        {
                            report("LIR1429", "Lowered WIR array place requires an array reference, integer index, and non-strengthening element reference.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Borrow)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        if (!sourceType || sourceType->kind != TypeKind::Reference || sourceType->arguments.size() != 1 ||
                            !resultType || resultType->kind != TypeKind::Reference || resultType->arguments.size() != 1 ||
                            sourceType->arguments.front() != resultType->arguments.front() ||
                            (resultType->isMutable && !sourceType->isMutable))
                        {
                            report("LIR1430", "Lowered WIR borrow cannot change the referred type or strengthen mutability.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::ConstructComponent || instruction.opcode == Opcode::ConstructObject)
                    {
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        const NominalKind expectedKind = instruction.opcode == Opcode::ConstructObject
                            ? NominalKind::Object
                            : NominalKind::Component;
                        if (!resultType || resultType->kind != TypeKind::Named || resultType->nominalKind != expectedKind ||
                            !resultType->hasConstructor || instruction.selector.empty() ||
                            instruction.signatureTypes.size() != instruction.operands.size() ||
                            !std::ranges::all_of(instruction.signatureTypes, [&](const TypeId type) { return module.types.tryGet(type) != nullptr; }) ||
                            !std::ranges::equal(
                                instruction.signatureTypes,
                                instruction.operands,
                                {},
                                [](const TypeId type) { return type; },
                                [&](const ValueId value) { return valueType(value); }))
                        {
                            report("LIR1433", "Lowered WIR construction requires a matching constructible component/object result and typed constructor signature.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Drop)
                    {
                        const Type* placeType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* valueTypeInfo = placeType && placeType->kind == TypeKind::Reference && placeType->arguments.size() == 1
                            ? module.types.tryGet(placeType->arguments.front())
                            : nullptr;
                        if (!valueTypeInfo || valueTypeInfo->kind != TypeKind::Named ||
                            (valueTypeInfo->nominalKind != NominalKind::Component && valueTypeInfo->nominalKind != NominalKind::Object))
                        {
                            report("LIR1434", "Lowered WIR drop requires a place containing a component or object value.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Call)
                    {
                        const auto calleeIt = instruction.callee ? functions.find(instruction.callee.value()) : functions.end();
                        if (calleeIt == functions.end())
                        {
                            report("LIR1407", "Lowered WIR call references an unknown function.", instruction.source, function.id, block.id);
                        }
                        else
                        {
                            const Function& callee = *calleeIt->second;
                            if (callee.parameters.size() != instruction.operands.size())
                                report("LIR1408", "Lowered WIR call argument count does not match its callee.", instruction.source, function.id, block.id);
                            else
                            {
                                for (std::size_t argumentIndex = 0; argumentIndex < instruction.operands.size(); ++argumentIndex)
                                {
                                    if (valueType(instruction.operands[argumentIndex]) != callee.parameters[argumentIndex].type)
                                        report("LIR1409", "Lowered WIR call argument type does not match its parameter.", instruction.source, function.id, block.id);
                                }
                            }
                            const Type* returnType = module.types.tryGet(callee.returnType);
                            const bool returnsVoid = returnType && returnType->kind == TypeKind::Void;
                            if (!returnType || (returnsVoid ? instruction.result.isValid() : instruction.resultType != callee.returnType))
                                report("LIR1410", "Lowered WIR call result does not match its callee.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Return)
                    {
                        const Type* returnType = module.types.tryGet(function.returnType);
                        const bool returnsVoid = returnType && returnType->kind == TypeKind::Void;
                        if (!returnType ||
                            (returnsVoid ? !instruction.operands.empty()
                                         : instruction.operands.size() != 1 || valueType(instruction.operands.front()) != function.returnType))
                        {
                            report("LIR1411", "Lowered WIR return value does not match its function.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Jump || instruction.opcode == Opcode::CondJump)
                    {
                        const std::size_t expectedTargets = instruction.opcode == Opcode::Jump ? 1 : 2;
                        const std::size_t expectedOperands = instruction.opcode == Opcode::Jump ? 0 : 1;
                        if (instruction.targets.size() != expectedTargets || instruction.operands.size() != expectedOperands)
                        {
                            report("LIR1412", "Lowered WIR jump shape is invalid.", instruction.source, function.id, block.id);
                        }
                        if (instruction.opcode == Opcode::CondJump && !instruction.operands.empty() &&
                            valueType(instruction.operands.front()) != module.types.boolType())
                        {
                            report("LIR1413", "Lowered WIR conditional jump condition must be bool.", instruction.source, function.id, block.id);
                        }
                    }

                    for (const BranchTarget& target : instruction.targets)
                    {
                        const auto targetIt = target.block ? blocks.find(target.block.value()) : blocks.end();
                        if (targetIt == blocks.end())
                        {
                            report("LIR1414", "Lowered WIR jump references an unknown block.", instruction.source, function.id, block.id);
                            continue;
                        }
                        const BasicBlock& targetBlock = *targetIt->second;
                        if (target.arguments.size() != targetBlock.parameters.size())
                        {
                            report("LIR1415", "Lowered WIR jump argument count does not match target block parameters.", instruction.source, function.id, block.id);
                            continue;
                        }
                        for (std::size_t argumentIndex = 0; argumentIndex < target.arguments.size(); ++argumentIndex)
                        {
                            if (valueType(target.arguments[argumentIndex]) != targetBlock.parameters[argumentIndex].type)
                                report("LIR1416", "Lowered WIR jump argument type does not match target block parameter.", instruction.source, function.id, block.id);
                        }
                    }
                }
            }
        }
        return result;
    }
}
