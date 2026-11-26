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
    constexpr std::uint32_t USize = 3;
    constexpr std::uint32_t Dict = 4;
    constexpr std::uint32_t Tree = 5;
    constexpr std::uint32_t RefDict = 6;
    constexpr std::uint32_t RefI32 = 7;
    constexpr std::uint32_t I32Array = 8;

    constexpr std::uint32_t Ordered = 0;
    constexpr std::uint32_t Unordered = 1;
    constexpr std::uint32_t Count = 2;
    constexpr std::uint32_t ContainsKey = 3;
    constexpr std::uint32_t Set = 4;
    constexpr std::uint32_t GetOrAdd = 5;
    constexpr std::uint32_t Remove = 6;
    constexpr std::uint32_t TryGet = 7;
    constexpr std::uint32_t Keys = 8;
    constexpr std::uint32_t Contains = 9;
    constexpr std::uint32_t FirstKey = 10;
    constexpr std::uint32_t LastKey = 11;
    constexpr std::uint32_t FloorKeyOr = 12;
    constexpr std::uint32_t CeilKeyOr = 13;
    constexpr std::uint32_t Merge = 14;
    constexpr std::uint32_t Clear = 15;
    constexpr std::uint32_t Empty = 16;

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

    bytecode::Instruction constant(const std::uint32_t result, const bytecode::ConstantId value)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Constant);
        output.result = result;
        output.resultType = I32;
        output.constant = value;
        return output;
    }

    bytecode::Instruction dictionary(const std::uint32_t result, const std::uint32_t type,
                                     std::vector<std::uint32_t> operands, const std::uint32_t selector)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::DictionaryCreate);
        output.result = result;
        output.resultType = type;
        output.operands = std::move(operands);
        output.selector = selector;
        return output;
    }

    bytecode::Instruction intrinsic(const std::uint32_t result, const std::uint32_t resultType,
                                    const std::uint32_t selector, std::vector<std::uint32_t> operands,
                                    const std::uint8_t family = 2)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::IntrinsicCall);
        output.result = result;
        output.resultType = resultType;
        output.selector = selector;
        output.operands = std::move(operands);
        output.intrinsicFamily = family;
        return output;
    }

    bytecode::Instruction localPlace(const std::uint32_t result, const std::uint32_t type)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::LocalPlace);
        output.result = result;
        output.resultType = type;
        return output;
    }

    bytecode::Instruction placeInit(const std::uint32_t place, const std::uint32_t value)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::PlaceInit);
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

    bytecode::Instruction binaryAdd(const std::uint32_t result, const std::uint32_t left, const std::uint32_t right)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Binary);
        output.result = result;
        output.resultType = I32;
        output.operands = {left, right};
        output.binaryOperator = 0;
        return output;
    }

    bytecode::Instruction dictionaryGet(const std::uint32_t result, const std::uint32_t dictionaryValue,
                                        const std::uint32_t key)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::DictionaryGet);
        output.result = result;
        output.resultType = I32;
        output.operands = {dictionaryValue, key};
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
        module.strings = {"ordered",    "unordered", "Count", "ContainsKey", "Set",      "GetOrAdd",
                          "Remove",     "TryGet",    "Keys",  "Contains",    "FirstKey", "LastKey",
                          "FloorKeyOr", "CeilKeyOr", "Merge", "Clear",       "Empty"};
        module.constants = {
            bytecode::Constant{},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 1},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 2},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 3},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 10},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 20},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 30},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 40},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 99},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 0},
        };
        module.types = {
            bytecode::Type{.kind = 1},
            bytecode::Type{.kind = 2},
            bytecode::Type{.kind = 5},
            bytecode::Type{.kind = 12},
            bytecode::Type{.kind = 32, .name = Unordered, .arguments = {I32, I32}},
            bytecode::Type{.kind = 32, .name = Ordered, .arguments = {I32, I32}},
            bytecode::Type{.kind = 29, .arguments = {Dict}, .flags = 0x01u},
            bytecode::Type{.kind = 29, .arguments = {I32}, .flags = 0x01u},
            bytecode::Type{.kind = 31, .arguments = {I32}},
        };

        module.functions.push_back(
            function(0, I32,
                     {constant(0, 1), constant(1, 4), constant(2, 2), constant(3, 5), constant(4, 8),
                      dictionary(5, Dict, {0, 1, 2, 3, 0, 4}, Unordered), dictionaryGet(6, 5, 0), returnValue(6)}));

        module.functions.push_back(
            function(1, I32,
                     {constant(0, 1), constant(1, 4), constant(2, 2), constant(3, 5), constant(4, 3), constant(5, 6),
                      constant(6, 7), dictionary(7, Dict, {0, 1, 2, 3}, Unordered), localPlace(8, RefDict),
                      placeInit(8, 7), intrinsic(bytecode::InvalidIndex, Void, Set, {8, 0, 5}),
                      intrinsic(9, I32, GetOrAdd, {8, 4, 6}), intrinsic(10, Bool, Remove, {8, 2}), load(11, Dict, 8),
                      dictionaryGet(12, 11, 0), binaryAdd(13, 9, 12), returnValue(13)}));

        module.functions.push_back(
            function(2, I32,
                     {constant(0, 1), constant(1, 4), constant(2, 9), dictionary(3, Dict, {0, 1}, Unordered),
                      localPlace(4, RefI32), placeInit(4, 2), intrinsic(5, Bool, TryGet, {3, 0, 4}), load(6, I32, 4),
                      returnValue(6)}));

        module.functions.push_back(
            function(3, Bool,
                     {constant(0, 1), constant(1, 4), dictionary(2, Dict, {0, 1}, Unordered),
                      intrinsic(3, I32Array, Keys, {2}), intrinsic(4, Bool, Contains, {3, 0}, 1), returnValue(4)}));

        module.functions.push_back(
            function(4, I32,
                     {constant(0, 1), constant(1, 4), constant(2, 3), constant(3, 5), constant(4, 2), constant(5, 9),
                      dictionary(6, Tree, {2, 3, 0, 1}, Ordered), intrinsic(7, I32, FirstKey, {6}),
                      intrinsic(8, I32, LastKey, {6}), intrinsic(9, I32, FloorKeyOr, {6, 4, 5}),
                      intrinsic(10, I32, CeilKeyOr, {6, 4, 5}), binaryAdd(11, 7, 8), binaryAdd(12, 9, 10),
                      binaryAdd(13, 11, 12), returnValue(13)}));

        bytecode::Instruction copy = instruction(bytecode::Opcode::CopyValue);
        copy.result = 4;
        copy.resultType = Dict;
        copy.operands = {2};
        bytecode::Instruction copiedPlace = instruction(bytecode::Opcode::DictionaryPlace);
        copiedPlace.result = 6;
        copiedPlace.resultType = RefI32;
        copiedPlace.operands = {5, 0};
        bytecode::Instruction store = instruction(bytecode::Opcode::Store);
        store.operands = {6, 3};
        module.functions.push_back(
            function(5, I32,
                     {constant(0, 1), constant(1, 4), dictionary(2, Dict, {0, 1}, Unordered), constant(3, 8), copy,
                      localPlace(5, RefDict), placeInit(5, 4), copiedPlace, store, dictionaryGet(7, 2, 0),
                      load(8, Dict, 5), dictionaryGet(9, 8, 0), binaryAdd(10, 7, 9), returnValue(10)}));

        module.functions.push_back(
            function(6, I32,
                     {constant(0, 1), constant(1, 4), constant(2, 8), dictionary(3, Dict, {0, 1}, Unordered),
                      dictionary(4, Dict, {0, 2}, Unordered), intrinsic(5, Dict, Merge, {3, 4}), dictionaryGet(6, 5, 0),
                      returnValue(6)}));

        module.functions.push_back(
            function(7, Bool,
                     {constant(0, 1), constant(1, 4), dictionary(2, Dict, {0, 1}, Unordered), localPlace(3, RefDict),
                      placeInit(3, 2), intrinsic(bytecode::InvalidIndex, Void, Clear, {3}),
                      intrinsic(4, Bool, Empty, {3}), returnValue(4)}));

        module.functions.push_back(
            function(8, I32,
                     {constant(0, 1), constant(1, 4), constant(2, 2), dictionary(3, Dict, {0, 1}, Unordered),
                      dictionaryGet(4, 3, 2), returnValue(4)}));
        return module;
    }
} // namespace

int main()
{
    bool ok = true;
    const bytecode::DecodeResult decoded = bytecode::decode(bytecode::encode(makeModule()));
    ok &= expect(decoded.succeeded(), "Dictionary VM fixture must survive the real .wiob codec and verifier");
    if (!decoded.succeeded())
        return 1;

    vm::Machine machine{decoded.module};
    const vm::ExecutionResult literal = machine.invoke(0);
    ok &= expect(literal.succeeded() && literal.value().asSignedInteger() == 10,
                 "Dictionary literals must keep the first duplicate key");

    const vm::ExecutionResult mutation = machine.invoke(1);
    ok &= expect(mutation.succeeded() && mutation.value().asSignedInteger() == 70,
                 "Set, GetOrAdd, Remove, and mutable dictionary places must execute");

    const vm::ExecutionResult tryGet = machine.invoke(2);
    ok &= expect(tryGet.succeeded() && tryGet.value().asSignedInteger() == 10,
                 "TryGet must write through its output place");

    const vm::ExecutionResult keys = machine.invoke(3);
    ok &= expect(keys.succeeded() && keys.value().asBoolean(), "Keys and array Contains must compose in the VM");

    const vm::ExecutionResult ordered = machine.invoke(4);
    ok &= expect(ordered.succeeded() && ordered.value().asSignedInteger() == 8,
                 "Ordered dictionary endpoints and floor/ceil lookups must use key order");

    const vm::ExecutionResult copied = machine.invoke(5);
    ok &= expect(copied.succeeded() && copied.value().asSignedInteger() == 109,
                 "CopyValue must isolate dictionary storage before mutation");

    const vm::ExecutionResult merged = machine.invoke(6);
    ok &= expect(merged.succeeded() && merged.value().asSignedInteger() == 99,
                 "Merge must overwrite left-hand values with right-hand values");

    const vm::ExecutionResult cleared = machine.invoke(7);
    ok &= expect(cleared.succeeded() && cleared.value().asBoolean(),
                 "Void Clear intrinsic must verify and leave the dictionary empty");

    const vm::ExecutionResult missing = machine.invoke(8);
    ok &= expect(!missing.succeeded() && missing.error().code == "WVM1080",
                 "Missing strict dictionary lookup must report stable WVM1080");
    return ok ? 0 : 1;
}
