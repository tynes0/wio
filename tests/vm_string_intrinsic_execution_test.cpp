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
    constexpr std::uint32_t Char = 5;
    constexpr std::uint32_t String = 6;
    constexpr std::uint32_t RefString = 7;
    constexpr std::uint32_t StringArray = 8;
    constexpr std::uint32_t F64 = 9;

    constexpr std::uint8_t ArrayFamily = 1;
    constexpr std::uint8_t StringFamily = 3;

    enum StringId : std::uint32_t
    {
        Trim,
        Replace,
        ToUpper,
        IndexOf,
        LastIndexOf,
        Split,
        Get,
        ToI32,
        ToF64,
        ToBool,
        TrimInPlace,
        Append,
        Push,
        Insert,
        Erase,
        ToUpperInPlace,
        GetOr,
        Lines,
        Count,
        SourceTransform,
        NeedleAb,
        ReplacementXy,
        SourceBanana,
        NeedleAn,
        SourceCsv,
        Comma,
        SourceInteger,
        SourceBool,
        SourceMutable,
        SuffixC,
        FragmentX,
        SourceFallback,
        SourceLines,
        SourceFloat,
        SourceInvalid
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

    bytecode::Instruction intrinsic(const std::uint32_t result, const std::uint32_t type, const std::uint32_t selector,
                                    std::vector<std::uint32_t> operands, const std::uint8_t family = StringFamily)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::IntrinsicCall);
        output.result = result;
        output.resultType = type;
        output.selector = selector;
        output.operands = std::move(operands);
        output.intrinsicFamily = family;
        return output;
    }

    bytecode::Instruction localPlace(const std::uint32_t result)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::LocalPlace);
        output.result = result;
        output.resultType = RefString;
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
        output.resultType = String;
        output.operands = {place};
        return output;
    }

    bytecode::Instruction add(const std::uint32_t result, const std::uint32_t left, const std::uint32_t right,
                              const std::uint32_t type)
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
        module.strings = {
            "Trim",
            "Replace",
            "ToUpper",
            "IndexOf",
            "LastIndexOf",
            "Split",
            "Get",
            "ToI32",
            "ToF64",
            "ToBool",
            "TrimInPlace",
            "Append",
            "Push",
            "Insert",
            "Erase",
            "ToUpperInPlace",
            "GetOr",
            "Lines",
            "Count",
            "  ab-ab  ",
            "ab",
            "xy",
            "banana",
            "an",
            "a,b,c",
            ",",
            "0x2a",
            " YES ",
            "  ab  ",
            "c",
            "X",
            "a",
            "first\r\nsecond\n",
            "3.25",
            "not-a-number",
        };
        module.constants = {
            bytecode::Constant{},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = SourceTransform},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = NeedleAb},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = ReplacementXy},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = SourceBanana},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = NeedleAn},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = SourceCsv},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = Comma},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = SourceInteger},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 0},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = SourceBool},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = SourceMutable},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = SuffixC},
            bytecode::Constant{.kind = bytecode::ConstantKind::UnsignedInteger, .bits = 'd'},
            bytecode::Constant{.kind = bytecode::ConstantKind::UnsignedInteger, .bits = 1},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = FragmentX},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = SourceFallback},
            bytecode::Constant{.kind = bytecode::ConstantKind::UnsignedInteger, .bits = 99},
            bytecode::Constant{.kind = bytecode::ConstantKind::UnsignedInteger, .bits = 'Z'},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = SourceLines},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = SourceFloat},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = SourceInvalid},
        };
        module.types = {
            bytecode::Type{.kind = 1},
            bytecode::Type{.kind = 2},
            bytecode::Type{.kind = 5},
            bytecode::Type{.kind = 7},
            bytecode::Type{.kind = 12},
            bytecode::Type{.kind = 16},
            bytecode::Type{.kind = 17},
            bytecode::Type{.kind = 29, .arguments = {String}, .flags = 0x01u},
            bytecode::Type{.kind = 31, .arguments = {String}},
            bytecode::Type{.kind = 14},
        };

        module.functions.push_back(function(0, String,
                                            {constant(0, String, 1), constant(1, String, 2), constant(2, String, 3),
                                             intrinsic(3, String, Trim, {0}), intrinsic(4, String, Replace, {3, 1, 2}),
                                             intrinsic(5, String, ToUpper, {4}), returnValue(5)}));

        module.functions.push_back(
            function(1, ISize,
                     {constant(0, String, 4), constant(1, String, 5), intrinsic(2, ISize, IndexOf, {0, 1}),
                      intrinsic(3, ISize, LastIndexOf, {0, 1}), add(4, 2, 3, ISize), returnValue(4)}));

        module.functions.push_back(function(2, String,
                                            {constant(0, String, 6), constant(1, String, 7), constant(2, USize, 14),
                                             intrinsic(3, StringArray, Split, {0, 1}),
                                             intrinsic(4, String, Get, {3, 2}, ArrayFamily), returnValue(4)}));

        module.functions.push_back(function(
            3, I32, {constant(0, String, 8), constant(1, I32, 9), intrinsic(2, I32, ToI32, {0, 1}), returnValue(2)}));

        module.functions.push_back(
            function(4, Bool, {constant(0, String, 10), intrinsic(1, Bool, ToBool, {0}), returnValue(1)}));

        module.functions.push_back(
            function(5, String,
                     {constant(0, String, 11), constant(1, String, 12), constant(2, Char, 13), constant(3, USize, 14),
                      constant(4, String, 15), localPlace(5), placeInit(5, 0),
                      intrinsic(bytecode::InvalidIndex, Void, TrimInPlace, {5}),
                      intrinsic(bytecode::InvalidIndex, Void, Append, {5, 1}),
                      intrinsic(bytecode::InvalidIndex, Void, Push, {5, 2}),
                      intrinsic(bytecode::InvalidIndex, Void, Insert, {5, 3, 4}),
                      intrinsic(bytecode::InvalidIndex, Void, Erase, {5, 3, 3}),
                      intrinsic(bytecode::InvalidIndex, Void, ToUpperInPlace, {5}), load(6, 5), returnValue(6)}));

        module.functions.push_back(function(6, Char,
                                            {constant(0, String, 16), constant(1, USize, 17), constant(2, Char, 18),
                                             intrinsic(3, Char, GetOr, {0, 1, 2}), returnValue(3)}));

        module.functions.push_back(function(7, USize,
                                            {constant(0, String, 19), intrinsic(1, StringArray, Lines, {0}),
                                             intrinsic(2, USize, Count, {1}, ArrayFamily), returnValue(2)}));

        module.functions.push_back(
            function(8, F64, {constant(0, String, 20), intrinsic(1, F64, ToF64, {0}), returnValue(1)}));

        module.functions.push_back(
            function(9, I32, {constant(0, String, 21), intrinsic(1, I32, ToI32, {0}), returnValue(1)}));
        return module;
    }
} // namespace

int main()
{
    bool ok = true;
    const bytecode::DecodeResult decoded = bytecode::decode(bytecode::encode(makeModule()));
    ok &= expect(decoded.succeeded(), "String intrinsic fixture must survive the real .wiob codec");
    if (!decoded.succeeded())
        return 1;

    vm::Machine machine{decoded.module};
    const vm::ExecutionResult transformed = machine.invoke(0);
    ok &= expect(transformed.succeeded() && transformed.value().asString() == "XY-XY",
                 "String copy transformations must compose");
    const vm::ExecutionResult searched = machine.invoke(1);
    ok &= expect(searched.succeeded() && searched.value().asSignedInteger() == 4,
                 "String index queries must return signed byte positions");
    const vm::ExecutionResult split = machine.invoke(2);
    ok &= expect(split.succeeded() && split.value().asString() == "b", "Split must produce an indexable string array");
    const vm::ExecutionResult integer = machine.invoke(3);
    ok &= expect(integer.succeeded() && integer.value().asSignedInteger() == 42,
                 "Base-aware integer parsing must execute in the VM");
    const vm::ExecutionResult boolean = machine.invoke(4);
    ok &= expect(boolean.succeeded() && boolean.value().asBoolean(), "Boolean parsing must trim and ignore case");
    const vm::ExecutionResult mutated = machine.invoke(5);
    ok &= expect(mutated.succeeded() && mutated.value().asString() == "ABCD",
                 "String mutation must require and update a mutable place");
    const vm::ExecutionResult fallback = machine.invoke(6);
    ok &= expect(fallback.succeeded() && fallback.value().asUnsignedInteger() == 'Z',
                 "GetOr must return its fallback outside string byte bounds");
    const vm::ExecutionResult lines = machine.invoke(7);
    ok &= expect(lines.succeeded() && lines.value().asUnsignedInteger() == 3,
                 "Lines must preserve the trailing empty line like the native runtime");
    const vm::ExecutionResult floating = machine.invoke(8);
    ok &= expect(floating.succeeded() && floating.value().asFloat64() == 3.25,
                 "Floating-point parsing must execute in the VM");
    const vm::ExecutionResult invalid = machine.invoke(9);
    ok &= expect(!invalid.succeeded() && invalid.error().code == "WVM1128",
                 "Invalid integer conversion must report stable WVM1128");
    return ok ? 0 : 1;
}
