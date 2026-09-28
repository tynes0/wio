#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"

#include <bit>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace
{
    namespace bytecode = wio::bytecode;
    namespace vm = wio::vm;

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

    bytecode::Instruction binary(const std::uint32_t result, const std::uint32_t type, const std::uint32_t left,
                                 const std::uint32_t right, const std::uint8_t operation)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Binary);
        output.result = result;
        output.resultType = type;
        output.operands = {left, right};
        output.binaryOperator = operation;
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

    bytecode::Module makeModule()
    {
        constexpr std::uint32_t Bool = 1;
        constexpr std::uint32_t I32 = 2;
        constexpr std::uint32_t RefI32 = 3;

        bytecode::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.constants = {bytecode::Constant{},
                            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 0},
                            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 1}};
        module.types = {
            bytecode::Type{.kind = 1},                                     // void
            bytecode::Type{.kind = 2},                                     // bool
            bytecode::Type{.kind = 5},                                     // i32
            bytecode::Type{.kind = 29, .arguments = {I32}, .flags = 0x01u} // mut ref i32
        };
        module.globals = {bytecode::Global{.id = 0, .type = I32, .flags = 0x01u}};

        bytecode::Function addOne;
        addOne.id = 0;
        addOne.returnType = I32;
        addOne.parameters = {parameter(0, I32)};
        bytecode::Block addEntry;
        addEntry.id = 0;
        addEntry.instructions = {constant(1, I32, 2), binary(2, I32, 0, 1, 0), returnValue(2)};
        addOne.blocks.push_back(std::move(addEntry));
        module.functions.push_back(std::move(addOne));

        bytecode::Function sum;
        sum.id = 1;
        sum.returnType = I32;
        sum.parameters = {parameter(0, I32)};

        bytecode::Block sumEntry;
        sumEntry.id = 0;
        sumEntry.instructions.push_back(constant(1, I32, 1));
        bytecode::Instruction enterLoop = instruction(bytecode::Opcode::Jump);
        enterLoop.targets = {bytecode::BranchTarget{.block = 1, .arguments = {1, 1}}};
        sumEntry.instructions.push_back(std::move(enterLoop));

        bytecode::Block loop;
        loop.id = 1;
        loop.parameters = {parameter(2, I32), parameter(3, I32)};
        loop.instructions.push_back(constant(4, I32, 2));
        loop.instructions.push_back(binary(5, Bool, 3, 0, 7)); // index < count
        bytecode::Instruction condition = instruction(bytecode::Opcode::CondJump);
        condition.operands = {5};
        condition.targets = {bytecode::BranchTarget{.block = 2, .arguments = {2, 3}},
                             bytecode::BranchTarget{.block = 3, .arguments = {2}}};
        loop.instructions.push_back(std::move(condition));

        bytecode::Block body;
        body.id = 2;
        body.parameters = {parameter(6, I32), parameter(7, I32)};
        body.instructions.push_back(binary(8, I32, 6, 7, 0));
        body.instructions.push_back(binary(9, I32, 7, 4, 0));
        bytecode::Instruction repeat = instruction(bytecode::Opcode::Jump);
        repeat.targets = {bytecode::BranchTarget{.block = 1, .arguments = {8, 9}}};
        body.instructions.push_back(std::move(repeat));

        bytecode::Block exit;
        exit.id = 3;
        exit.parameters = {parameter(10, I32)};
        exit.instructions = {returnValue(10)};
        sum.blocks = {std::move(sumEntry), std::move(loop), std::move(body), std::move(exit)};
        module.functions.push_back(std::move(sum));

        bytecode::Function composed;
        composed.id = 2;
        composed.returnType = I32;
        composed.parameters = {parameter(0, I32)};
        bytecode::Block composedEntry;
        composedEntry.id = 0;
        bytecode::Instruction callSum = instruction(bytecode::Opcode::Call);
        callSum.result = 1;
        callSum.resultType = I32;
        callSum.operands = {0};
        callSum.callee = 1;
        bytecode::Instruction callAddOne = instruction(bytecode::Opcode::Call);
        callAddOne.result = 2;
        callAddOne.resultType = I32;
        callAddOne.operands = {1};
        callAddOne.callee = 0;
        composedEntry.instructions = {std::move(callSum), std::move(callAddOne), returnValue(2)};
        composed.blocks.push_back(std::move(composedEntry));
        module.functions.push_back(std::move(composed));

        bytecode::Function divide;
        divide.id = 3;
        divide.returnType = I32;
        divide.parameters = {parameter(0, I32), parameter(1, I32)};
        bytecode::Block divideEntry;
        divideEntry.id = 0;
        divideEntry.instructions = {binary(2, I32, 0, 1, 3), returnValue(2)};
        divide.blocks.push_back(std::move(divideEntry));
        module.functions.push_back(std::move(divide));

        bytecode::Function unsupported;
        unsupported.id = 4;
        unsupported.returnType = I32;
        bytecode::Block unsupportedEntry;
        unsupportedEntry.id = 0;
        bytecode::Instruction array = instruction(bytecode::Opcode::ArrayCreate);
        array.result = 0;
        array.resultType = I32;
        unsupportedEntry.instructions = {std::move(array), returnValue(0)};
        unsupported.blocks.push_back(std::move(unsupportedEntry));
        module.functions.push_back(std::move(unsupported));

        bytecode::Function endless;
        endless.id = 5;
        endless.returnType = 0;
        bytecode::Block endlessEntry;
        endlessEntry.id = 0;
        bytecode::Instruction cycle = instruction(bytecode::Opcode::Jump);
        cycle.targets = {bytecode::BranchTarget{.block = 0}};
        endlessEntry.instructions = {std::move(cycle)};
        endless.blocks.push_back(std::move(endlessEntry));
        module.functions.push_back(std::move(endless));

        bytecode::Function localPlace;
        localPlace.id = 6;
        localPlace.returnType = I32;
        localPlace.parameters = {parameter(0, I32)};
        bytecode::Block localEntry;
        localEntry.id = 0;
        bytecode::Instruction local = instruction(bytecode::Opcode::LocalPlace);
        local.result = 1;
        local.resultType = RefI32;
        bytecode::Instruction initialize = instruction(bytecode::Opcode::PlaceInit);
        initialize.operands = {1, 0};
        bytecode::Instruction load = instruction(bytecode::Opcode::Load);
        load.result = 2;
        load.resultType = I32;
        load.operands = {1};
        localEntry.instructions = {std::move(local), std::move(initialize), std::move(load), constant(3, I32, 2),
                                   binary(4, I32, 2, 3, 0)};
        bytecode::Instruction store = instruction(bytecode::Opcode::Store);
        store.operands = {1, 4};
        localEntry.instructions.push_back(std::move(store));
        bytecode::Instruction finalLoad = instruction(bytecode::Opcode::Load);
        finalLoad.result = 5;
        finalLoad.resultType = I32;
        finalLoad.operands = {1};
        localEntry.instructions.push_back(std::move(finalLoad));
        localEntry.instructions.push_back(returnValue(5));
        localPlace.blocks.push_back(std::move(localEntry));
        module.functions.push_back(std::move(localPlace));

        bytecode::Function movePlace;
        movePlace.id = 7;
        movePlace.returnType = I32;
        movePlace.parameters = {parameter(0, I32)};
        bytecode::Block moveEntry;
        moveEntry.id = 0;
        bytecode::Instruction moveLocal = instruction(bytecode::Opcode::LocalPlace);
        moveLocal.result = 1;
        moveLocal.resultType = RefI32;
        bytecode::Instruction moveInitialize = instruction(bytecode::Opcode::PlaceInit);
        moveInitialize.operands = {1, 0};
        bytecode::Instruction move = instruction(bytecode::Opcode::MoveValue);
        move.result = 2;
        move.resultType = I32;
        move.operands = {1};
        moveEntry.instructions = {std::move(moveLocal), std::move(moveInitialize), std::move(move), returnValue(2)};
        movePlace.blocks.push_back(std::move(moveEntry));
        module.functions.push_back(std::move(movePlace));

        bytecode::Function incrementGlobal;
        incrementGlobal.id = 8;
        incrementGlobal.returnType = I32;
        bytecode::Block globalEntry;
        globalEntry.id = 0;
        bytecode::Instruction global = instruction(bytecode::Opcode::GlobalPlace);
        global.result = 0;
        global.resultType = RefI32;
        global.global = 0;
        bytecode::Instruction globalLoad = instruction(bytecode::Opcode::Load);
        globalLoad.result = 1;
        globalLoad.resultType = I32;
        globalLoad.operands = {0};
        globalEntry.instructions = {std::move(global), std::move(globalLoad), constant(2, I32, 2),
                                    binary(3, I32, 1, 2, 0)};
        bytecode::Instruction globalStore = instruction(bytecode::Opcode::Store);
        globalStore.operands = {0, 3};
        globalEntry.instructions.push_back(std::move(globalStore));
        bytecode::Instruction reloaded = instruction(bytecode::Opcode::Load);
        reloaded.result = 4;
        reloaded.resultType = I32;
        reloaded.operands = {0};
        globalEntry.instructions.push_back(std::move(reloaded));
        globalEntry.instructions.push_back(returnValue(4));
        incrementGlobal.blocks.push_back(std::move(globalEntry));
        module.functions.push_back(std::move(incrementGlobal));
        return module;
    }
} // namespace

int main()
{
    bool ok = true;
    const bytecode::Module source = makeModule();
    const bytecode::DecodeResult decoded = bytecode::decode(bytecode::encode(source));
    ok &= expect(decoded.succeeded(), "VM fixture must survive the real .wiob codec");
    if (!decoded.succeeded())
        return 1;

    vm::Machine machine{decoded.module};
    const vm::Value five = vm::Value::signedInteger(5);
    const vm::ExecutionResult composed = machine.invoke(2, std::span{&five, 1});
    ok &= expect(composed.succeeded(), "Nested calls and loop CFG must execute");
    ok &= expect(composed.succeeded() && composed.value().kind() == vm::Value::Kind::SignedInteger &&
                     composed.value().asSignedInteger() == 11,
                 "sum(0..4) followed by add-one must return 11");

    const vm::Value zero = vm::Value::signedInteger(0);
    const vm::Value divideArguments[] = {five, zero};
    const vm::ExecutionResult divide = machine.invoke(3, divideArguments);
    ok &= expect(!divide.succeeded() && divide.error().code == "WVM1014",
                 "Integer division by zero must return a stable VM diagnostic");

    const vm::ExecutionResult unsupported = machine.invoke(4);
    ok &= expect(!unsupported.succeeded() && unsupported.error().code == "WVM1036",
                 "Unsupported opcodes must fail explicitly instead of silently misexecuting");

    vm::Machine bounded{decoded.module, vm::MachineOptions{.instructionLimit = 8}};
    const vm::ExecutionResult endless = bounded.invoke(5);
    ok &= expect(!endless.succeeded() && endless.error().code == "WVM1008",
                 "Instruction budget must stop non-terminating bytecode");

    const vm::ExecutionResult wrongArity = machine.invoke(0);
    ok &=
        expect(!wrongArity.succeeded() && wrongArity.error().code == "WVM1004", "Call boundaries must validate arity");

    const vm::ExecutionResult local = machine.invoke(6, std::span{&five, 1});
    ok &= expect(local.succeeded() && local.value().asSignedInteger() == 6,
                 "Local place init/load/store must preserve mutable scalar storage");
    const vm::ExecutionResult moved = machine.invoke(7, std::span{&five, 1});
    ok &= expect(moved.succeeded() && moved.value().asSignedInteger() == 5,
                 "Move-value must transfer an initialized place into an SSA result");
    const vm::ExecutionResult firstGlobal = machine.invoke(8);
    const vm::ExecutionResult secondGlobal = machine.invoke(8);
    ok &= expect(firstGlobal.succeeded() && secondGlobal.succeeded() && firstGlobal.value().asSignedInteger() == 1 &&
                     secondGlobal.value().asSignedInteger() == 2,
                 "Global places must persist across invocations on one machine");
    return ok ? 0 : 1;
}
