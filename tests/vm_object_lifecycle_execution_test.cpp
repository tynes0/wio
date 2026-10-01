#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"

#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    namespace bytecode = wio::bytecode;
    namespace vm = wio::vm;

    constexpr std::uint32_t Void = 0;
    constexpr std::uint32_t I32 = 1;
    constexpr std::uint32_t USize = 2;
    constexpr std::uint32_t RefI32 = 3;
    constexpr std::uint32_t Stats = 4;
    constexpr std::uint32_t RefStats = 5;
    constexpr std::uint32_t Enemy = 6;
    constexpr std::uint32_t RefEnemy = 7;
    constexpr std::uint32_t I32Array = 8;
    constexpr std::uint32_t RefI32Array = 9;

    bool expect(const bool condition, const std::string_view message)
    {
        if (condition)
            return true;
        std::cerr << message << '\n';
        return false;
    }

    bytecode::Instruction instruction(const bytecode::Opcode opcode)
    {
        bytecode::Instruction result;
        result.opcode = opcode;
        result.constant = 0;
        return result;
    }

    bytecode::Instruction constant(const std::uint32_t result, const std::uint32_t type,
                                   const bytecode::ConstantId value)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Constant);
        output.result = result;
        output.resultType = type;
        output.constant = value;
        return output;
    }

    bytecode::Instruction fieldPlace(const std::uint32_t result, const std::uint32_t resultType,
                                     const std::uint32_t base, const std::uint32_t owner, const std::uint32_t field)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::FieldPlace);
        output.result = result;
        output.resultType = resultType;
        output.operands = {base};
        output.targetType = owner;
        output.projectionIndex = field;
        return output;
    }

    bytecode::Instruction store(const std::uint32_t place, const std::uint32_t value)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Store);
        output.operands = {place, value};
        return output;
    }

    bytecode::Instruction load(const std::uint32_t result, const std::uint32_t type, const std::uint32_t place)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Load);
        output.result = result;
        output.resultType = type;
        output.operands = {place};
        return output;
    }

    bytecode::Instruction returnValue(const std::uint32_t value)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Return);
        output.operands = {value};
        return output;
    }

    bytecode::Parameter parameter(const std::uint32_t value, const std::uint32_t type)
    {
        return bytecode::Parameter{.value = value, .type = type};
    }

    bytecode::Function function(const std::uint32_t id, const std::uint32_t returnType,
                                std::vector<bytecode::Parameter> parameters,
                                std::vector<bytecode::Instruction> instructions)
    {
        bytecode::Function output;
        output.id = id;
        output.returnType = returnType;
        output.parameters = std::move(parameters);
        bytecode::Block entry;
        entry.id = 0;
        entry.instructions = std::move(instructions);
        output.blocks.push_back(std::move(entry));
        return output;
    }

    bytecode::Module makeModule()
    {
        bytecode::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.constants = {
            bytecode::Constant{},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 7},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 1},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 9},
            bytecode::Constant{.kind = bytecode::ConstantKind::UnsignedInteger, .bits = 0},
        };
        module.types = {
            bytecode::Type{.kind = 1},
            bytecode::Type{.kind = 5},
            bytecode::Type{.kind = 12},
            bytecode::Type{.kind = 29, .arguments = {I32}, .flags = 0x01u},
            bytecode::Type{
                .kind = 28, .nominalKind = 1, .ownership = 1, .fields = {bytecode::Type::Field{.type = I32}}},
            bytecode::Type{.kind = 29, .arguments = {Stats}, .flags = 0x01u},
            bytecode::Type{
                .kind = 28,
                .nominalKind = 2,
                .ownership = 2,
                .cleanup = 2,
                .fields = {bytecode::Type::Field{.type = Stats}},
                .methods = {bytecode::Type::Method{.parameterTypes = {}, .returnType = I32, .function = 2, .slot = 0}},
                .castTypes = {Enemy},
                .dispatchEntries = {bytecode::Type::DispatchEntry{
                    .contractType = Enemy, .slot = 0, .implementation = 2}},
                .destructor = 3},
            bytecode::Type{.kind = 29, .arguments = {Enemy}, .flags = 0x01u},
            bytecode::Type{.kind = 31, .arguments = {I32}},
            bytecode::Type{.kind = 29, .arguments = {I32Array}, .flags = 0x01u},
        };
        module.globals = {bytecode::Global{.id = 0, .type = I32, .flags = 0x01u}};

        bytecode::Instruction statsStore = store(2, 1);
        module.functions.push_back(
            function(0, Void, {parameter(0, RefStats), parameter(1, I32)},
                     {fieldPlace(2, RefI32, 0, Stats, 0), statsStore, instruction(bytecode::Opcode::Return)}));

        bytecode::Instruction constructStats = instruction(bytecode::Opcode::ConstructComponent);
        constructStats.result = 2;
        constructStats.resultType = Stats;
        constructStats.operands = {1};
        constructStats.callee = 0;
        module.functions.push_back(function(1, Void, {parameter(0, RefEnemy), parameter(1, I32)},
                                            {constructStats, fieldPlace(3, RefStats, 0, Enemy, 0), store(3, 2),
                                             instruction(bytecode::Opcode::Return)}));

        module.functions.push_back(function(2, I32, {parameter(0, RefEnemy)},
                                            {fieldPlace(1, RefStats, 0, Enemy, 0), fieldPlace(2, RefI32, 1, Stats, 0),
                                             load(3, I32, 2), returnValue(3)}));
        module.functions.back().ownerType = Enemy;
        module.functions.back().methodSlot = 0;

        bytecode::Instruction global = instruction(bytecode::Opcode::GlobalPlace);
        global.result = 1;
        global.resultType = RefI32;
        global.global = 0;
        bytecode::Instruction increment = instruction(bytecode::Opcode::Binary);
        increment.result = 4;
        increment.resultType = I32;
        increment.operands = {2, 3};
        increment.binaryOperator = 0;
        module.functions.push_back(function(3, Void, {parameter(0, RefEnemy)},
                                            {global, load(2, I32, 1), constant(3, I32, 2), increment, store(1, 4),
                                             instruction(bytecode::Opcode::Return)}));

        bytecode::Instruction constructEnemy = instruction(bytecode::Opcode::ConstructObject);
        constructEnemy.result = 1;
        constructEnemy.resultType = Enemy;
        constructEnemy.operands = {0};
        constructEnemy.callee = 1;
        bytecode::Instruction retain = instruction(bytecode::Opcode::Retain);
        retain.result = 2;
        retain.resultType = Enemy;
        retain.operands = {1};
        bytecode::Instruction getter = instruction(bytecode::Opcode::VirtualCall);
        getter.result = 3;
        getter.resultType = I32;
        getter.operands = {2};
        getter.callee = 2;
        getter.targetType = Enemy;
        getter.projectionIndex = 0;
        getter.signatureTypes = {RefEnemy};
        bytecode::Instruction releaseRetained = instruction(bytecode::Opcode::Release);
        releaseRetained.operands = {2};
        bytecode::Instruction releaseOriginal = instruction(bytecode::Opcode::Release);
        releaseOriginal.operands = {1};
        bytecode::Instruction readDestroyCount = instruction(bytecode::Opcode::GlobalPlace);
        readDestroyCount.result = 4;
        readDestroyCount.resultType = RefI32;
        readDestroyCount.global = 0;
        bytecode::Instruction combine = instruction(bytecode::Opcode::Binary);
        combine.result = 6;
        combine.resultType = I32;
        combine.operands = {3, 5};
        combine.binaryOperator = 0;
        module.functions.push_back(
            function(4, I32, {},
                     {constant(0, I32, 1), constructEnemy, retain, getter, releaseRetained, releaseOriginal,
                      readDestroyCount, load(5, I32, 4), combine, returnValue(6)}));

        bytecode::Instruction constructValue = instruction(bytecode::Opcode::ConstructComponent);
        constructValue.result = 1;
        constructValue.resultType = Stats;
        constructValue.operands = {0};
        constructValue.callee = 0;
        bytecode::Instruction copy = instruction(bytecode::Opcode::CopyValue);
        copy.result = 2;
        copy.resultType = Stats;
        copy.operands = {1};
        module.functions.push_back(function(5, I32, {},
                                            {constant(0, I32, 1), constructValue, copy, constant(3, I32, 3),
                                             fieldPlace(4, RefI32, 1, Stats, 0), store(4, 3),
                                             fieldPlace(5, RefI32, 2, Stats, 0), load(6, I32, 5), returnValue(6)}));

        bytecode::Instruction array = instruction(bytecode::Opcode::ArrayCreate);
        array.result = 2;
        array.resultType = I32Array;
        array.operands = {0};
        bytecode::Instruction local = instruction(bytecode::Opcode::LocalPlace);
        local.result = 3;
        local.resultType = RefI32Array;
        bytecode::Instruction initialize = instruction(bytecode::Opcode::PlaceInit);
        initialize.operands = {3, 2};
        bytecode::Instruction arrayPlace = instruction(bytecode::Opcode::ArrayPlace);
        arrayPlace.result = 5;
        arrayPlace.resultType = RefI32;
        arrayPlace.operands = {3, 4};
        module.functions.push_back(function(6, I32, {},
                                            {constant(0, I32, 1), constant(1, I32, 3), array, local, initialize,
                                             constant(4, USize, 4), arrayPlace, store(5, 1), load(6, I32Array, 3),
                                             [&]
                                             {
                                                 bytecode::Instruction get = instruction(bytecode::Opcode::ArrayGet);
                                                 get.result = 7;
                                                 get.resultType = I32;
                                                 get.operands = {6, 4};
                                                 return get;
                                             }(),
                                             returnValue(7)}));
        return module;
    }
} // namespace

int main()
{
    bool ok = true;
    const bytecode::DecodeResult decoded = bytecode::decode(bytecode::encode(makeModule()));
    ok &= expect(decoded.succeeded(), "Object lifecycle VM fixture must survive the real .wiob codec");
    if (!decoded.succeeded())
        return 1;

    vm::Machine machine{decoded.module};
    const vm::ExecutionResult first = machine.invoke(4);
    const vm::ExecutionResult second = machine.invoke(4);
    ok &= expect(first.succeeded() && first.value().asSignedInteger() == 8,
                 "Constructors, nested field places, virtual calls, and first destructor must execute");
    ok &= expect(second.succeeded() && second.value().asSignedInteger() == 9,
                 "Exactly one destructor must run for each final intrusive release");

    const vm::ExecutionResult copied = machine.invoke(5);
    ok &= expect(copied.succeeded() && copied.value().asSignedInteger() == 7,
                 "CopyValue must deep-copy component storage before either value is mutated");

    const vm::ExecutionResult arrayPlace = machine.invoke(6);
    ok &= expect(arrayPlace.succeeded() && arrayPlace.value().asSignedInteger() == 9,
                 "ArrayPlace must alias the selected element and preserve the mutation through its parent place");
    return ok ? 0 : 1;
}
