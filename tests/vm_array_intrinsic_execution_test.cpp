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
    constexpr std::uint32_t Bool = 1;
    constexpr std::uint32_t I32 = 2;
    constexpr std::uint32_t ISize = 3;
    constexpr std::uint32_t USize = 4;
    constexpr std::uint32_t String = 5;
    constexpr std::uint32_t I32Array = 6;
    constexpr std::uint32_t RefI32Array = 7;
    constexpr std::uint32_t StringArray = 8;

    enum Selector : std::uint32_t
    {
        Sorted,
        First,
        Last,
        IndexOf,
        LastIndexOf,
        Slice,
        Take,
        Skip,
        Concat,
        Push,
        PushFront,
        Pop,
        PopFront,
        Insert,
        RemoveAt,
        Remove,
        Extend,
        Reserve,
        ShrinkToFit,
        Reverse,
        Sort,
        Get,
        Join,
        GetOr,
        LiteralA,
        LiteralB,
        LiteralDash
    };

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

    bytecode::Instruction constant(const std::uint32_t result, const std::uint32_t type, const bytecode::ConstantId id)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Constant);
        output.result = result;
        output.resultType = type;
        output.constant = id;
        return output;
    }

    bytecode::Instruction array(const std::uint32_t result, const std::uint32_t type,
                                std::vector<std::uint32_t> operands)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::ArrayCreate);
        output.result = result;
        output.resultType = type;
        output.operands = std::move(operands);
        return output;
    }

    bytecode::Instruction intrinsic(const std::uint32_t result, const std::uint32_t type, const Selector selector,
                                    std::vector<std::uint32_t> operands)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::IntrinsicCall);
        output.result = result;
        output.resultType = type;
        output.selector = selector;
        output.operands = std::move(operands);
        output.intrinsicFamily = 1;
        return output;
    }

    bytecode::Instruction localPlace(const std::uint32_t result)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::LocalPlace);
        output.result = result;
        output.resultType = RefI32Array;
        return output;
    }

    bytecode::Instruction placeInit(const std::uint32_t place, const std::uint32_t value)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::PlaceInit);
        output.operands = {place, value};
        return output;
    }

    bytecode::Instruction load(const std::uint32_t result, const std::uint32_t place)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Load);
        output.result = result;
        output.resultType = I32Array;
        output.operands = {place};
        return output;
    }

    bytecode::Instruction add(const std::uint32_t result, const std::uint32_t left, const std::uint32_t right,
                              const std::uint32_t type = I32)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Binary);
        output.result = result;
        output.resultType = type;
        output.operands = {left, right};
        output.binaryOperator = 0;
        return output;
    }

    bytecode::Instruction returnValue(const std::uint32_t value)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Return);
        output.operands = {value};
        return output;
    }

    bytecode::Function function(const std::uint32_t id, const std::uint32_t returnType,
                                std::vector<bytecode::Instruction> instructions)
    {
        bytecode::Function output;
        output.id = id;
        output.returnType = returnType;
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
        module.strings = {"Sorted",   "First",  "Last",   "IndexOf",   "LastIndexOf", "Slice",    "Take",
                          "Skip",     "Concat", "Push",   "PushFront", "Pop",         "PopFront", "Insert",
                          "RemoveAt", "Remove", "Extend", "Reserve",   "ShrinkToFit", "Reverse",  "Sort",
                          "Get",      "Join",   "GetOr",  "a",         "b",           "-"};
        module.constants = {
            bytecode::Constant{},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 0},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 1},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 2},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 3},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 4},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 5},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 9},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 99},
            bytecode::Constant{.kind = bytecode::ConstantKind::UnsignedInteger, .bits = 0},
            bytecode::Constant{.kind = bytecode::ConstantKind::UnsignedInteger, .bits = 1},
            bytecode::Constant{.kind = bytecode::ConstantKind::UnsignedInteger, .bits = 2},
            bytecode::Constant{.kind = bytecode::ConstantKind::UnsignedInteger, .bits = 16},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = LiteralA},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = LiteralB},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = LiteralDash},
        };
        module.types = {
            bytecode::Type{.kind = 1},
            bytecode::Type{.kind = 2},
            bytecode::Type{.kind = 5},
            bytecode::Type{.kind = 7},
            bytecode::Type{.kind = 12},
            bytecode::Type{.kind = 17},
            bytecode::Type{.kind = 31, .arguments = {I32}},
            bytecode::Type{.kind = 29, .arguments = {I32Array}, .flags = 0x01u},
            bytecode::Type{.kind = 31, .arguments = {String}},
        };

        module.functions.push_back(
            function(0, I32,
                     {constant(0, I32, 4), constant(1, I32, 2), constant(2, I32, 3), array(3, I32Array, {0, 1, 2}),
                      intrinsic(4, I32Array, Sorted, {3}), intrinsic(5, I32, First, {4}), intrinsic(6, I32, Last, {4}),
                      add(7, 5, 6), returnValue(7)}));

        module.functions.push_back(
            function(1, ISize,
                     {constant(0, I32, 6), constant(1, I32, 3), constant(2, I32, 7), array(3, I32Array, {0, 1, 2, 1}),
                      intrinsic(4, ISize, IndexOf, {3, 1}), intrinsic(5, ISize, LastIndexOf, {3, 1}),
                      add(6, 4, 5, ISize), returnValue(6)}));

        module.functions.push_back(function(
            2, I32,
            {constant(0, I32, 2), constant(1, I32, 3), constant(2, I32, 4), constant(3, I32, 5), constant(4, USize, 10),
             constant(5, USize, 11), constant(6, USize, 9), array(7, I32Array, {0, 1, 2, 3}),
             intrinsic(8, I32Array, Slice, {7, 4, 5}), intrinsic(9, I32Array, Take, {7, 5}),
             intrinsic(10, I32Array, Skip, {7, 5}), intrinsic(11, I32Array, Concat, {8, 9}),
             intrinsic(12, I32, Get, {11, 6}), intrinsic(13, I32, Get, {10, 6}), add(14, 12, 13), returnValue(14)}));

        module.functions.push_back(function(3, I32,
                                            {constant(0, I32, 4),
                                             constant(1, I32, 2),
                                             constant(2, I32, 3),
                                             constant(3, I32, 5),
                                             constant(4, I32, 1),
                                             constant(5, I32, 7),
                                             constant(6, I32, 6),
                                             constant(7, USize, 10),
                                             constant(8, USize, 9),
                                             constant(9, USize, 12),
                                             array(10, I32Array, {0, 1, 2}),
                                             localPlace(11),
                                             placeInit(11, 10),
                                             intrinsic(bytecode::InvalidIndex, Void, Push, {11, 3}),
                                             intrinsic(bytecode::InvalidIndex, Void, PushFront, {11, 4}),
                                             intrinsic(12, I32, Pop, {11}),
                                             intrinsic(13, I32, PopFront, {11}),
                                             intrinsic(bytecode::InvalidIndex, Void, Insert, {11, 7, 5}),
                                             intrinsic(bytecode::InvalidIndex, Void, RemoveAt, {11, 8}),
                                             intrinsic(14, Bool, Remove, {11, 1}),
                                             array(15, I32Array, {6, 3}),
                                             intrinsic(bytecode::InvalidIndex, Void, Extend, {11, 15}),
                                             intrinsic(bytecode::InvalidIndex, Void, Reserve, {11, 9}),
                                             intrinsic(bytecode::InvalidIndex, Void, Reverse, {11}),
                                             intrinsic(bytecode::InvalidIndex, Void, Sort, {11}),
                                             intrinsic(bytecode::InvalidIndex, Void, ShrinkToFit, {11}),
                                             load(16, 11),
                                             intrinsic(17, I32, Last, {16}),
                                             returnValue(17)}));

        module.functions.push_back(
            function(4, String,
                     {constant(0, String, 13), constant(1, String, 14), constant(2, String, 15),
                      array(3, StringArray, {0, 1}), intrinsic(4, String, Join, {3, 2}), returnValue(4)}));

        module.functions.push_back(
            function(5, I32,
                     {constant(0, I32, 2), constant(1, I32, 8), constant(2, USize, 12), array(3, I32Array, {0}),
                      intrinsic(4, I32, GetOr, {3, 2, 1}), returnValue(4)}));

        module.functions.push_back(function(
            6, I32,
            {array(0, I32Array, {}), localPlace(1), placeInit(1, 0), intrinsic(2, I32, Pop, {1}), returnValue(2)}));
        return module;
    }
} // namespace

int main()
{
    bool ok = true;
    const bytecode::DecodeResult decoded = bytecode::decode(bytecode::encode(makeModule()));
    ok &= expect(decoded.succeeded(), "Array intrinsic fixture must survive the real .wiob codec");
    if (!decoded.succeeded())
        return 1;

    vm::Machine machine{decoded.module};
    const vm::ExecutionResult sorted = machine.invoke(0);
    ok &= expect(sorted.succeeded() && sorted.value().asSignedInteger() == 4,
                 "Sorted, First, and Last must execute without mutating the source array");
    const vm::ExecutionResult searched = machine.invoke(1);
    ok &= expect(searched.succeeded() && searched.value().asSignedInteger() == 4,
                 "IndexOf and LastIndexOf must return signed indices");
    const vm::ExecutionResult sliced = machine.invoke(2);
    ok &= expect(sliced.succeeded() && sliced.value().asSignedInteger() == 5,
                 "Slice, Take, Skip, Concat, and indexed Get must compose");
    const vm::ExecutionResult mutated = machine.invoke(3);
    ok &= expect(mutated.succeeded() && mutated.value().asSignedInteger() == 9,
                 "Resizable array mutation and in-place sorting must execute through a mutable place");
    const vm::ExecutionResult joined = machine.invoke(4);
    ok &= expect(joined.succeeded() && joined.value().asString() == "a-b", "String arrays must support Join");
    const vm::ExecutionResult fallback = machine.invoke(5);
    ok &= expect(fallback.succeeded() && fallback.value().asSignedInteger() == 99,
                 "GetOr must return its fallback outside array bounds");
    const vm::ExecutionResult emptyPop = machine.invoke(6);
    ok &= expect(!emptyPop.succeeded() && emptyPop.error().code == "WVM1104", "Empty Pop must report stable WVM1104");
    return ok ? 0 : 1;
}
