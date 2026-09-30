#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include "wio/vm/unicode.h"

#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    namespace bytecode = wio::bytecode;
    namespace vm = wio::vm;

    constexpr std::uint32_t Bool = 1;
    constexpr std::uint32_t U32 = 2;
    constexpr std::uint32_t USize = 3;
    constexpr std::uint32_t String = 4;
    constexpr std::uint32_t Text = 5;
    constexpr std::uint32_t U32Array = 6;
    constexpr std::uint32_t TextArray = 7;

    constexpr std::uint8_t ArrayFamily = 1;
    constexpr std::uint8_t TextFamily = 4;

    enum StringId : std::uint32_t
    {
        Contains,
        GraphemeCount,
        SliceGraphemes,
        DisplayWidth,
        CaseFold,
        CodePoints,
        Graphemes,
        Get,
        ToString,
        Slice,
        ByteCount,
        Empty,
        Count,
        StartsWith,
        EndsWith,
        ComplexText,
        EmojiNeedle,
        FoldText,
        ScalarText,
        EmptyText,
        PrefixA,
        SuffixB
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
                                    std::vector<std::uint32_t> operands, const std::uint8_t family = TextFamily)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::IntrinsicCall);
        output.result = result;
        output.resultType = type;
        output.selector = selector;
        output.operands = std::move(operands);
        output.intrinsicFamily = family;
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
            "Contains",
            "GraphemeCount",
            "SliceGraphemes",
            "DisplayWidth",
            "CaseFold",
            "CodePoints",
            "Graphemes",
            "Get",
            "ToString",
            "Slice",
            "ByteCount",
            "Empty",
            "Count",
            "StartsWith",
            "EndsWith",
            "A👩‍💻é🇹🇷你",
            "👩‍💻",
            "Straße",
            "A🌍B",
            "",
            "A",
            "B",
        };
        module.constants = {
            bytecode::Constant{},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = ComplexText},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = EmojiNeedle},
            bytecode::Constant{.kind = bytecode::ConstantKind::UnsignedInteger, .bits = 1},
            bytecode::Constant{.kind = bytecode::ConstantKind::UnsignedInteger, .bits = 3},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = FoldText},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = ScalarText},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = EmptyText},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = PrefixA},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = SuffixB},
        };
        module.types = {
            bytecode::Type{.kind = 1},
            bytecode::Type{.kind = 2},
            bytecode::Type{.kind = 10},
            bytecode::Type{.kind = 12},
            bytecode::Type{.kind = 17},
            bytecode::Type{.kind = 18},
            bytecode::Type{.kind = 31, .arguments = {U32}},
            bytecode::Type{.kind = 31, .arguments = {Text}},
        };

        module.functions.push_back(function(
            0, Bool,
            {constant(0, Text, 1), constant(1, Text, 2), intrinsic(2, Bool, Contains, {0, 1}), returnValue(2)}));

        module.functions.push_back(
            function(1, USize, {constant(0, Text, 1), intrinsic(1, USize, GraphemeCount, {0}), returnValue(1)}));

        module.functions.push_back(function(2, Text,
                                            {constant(0, Text, 1), constant(1, USize, 3), constant(2, USize, 4),
                                             intrinsic(3, Text, SliceGraphemes, {0, 1, 2}), returnValue(3)}));

        module.functions.push_back(
            function(3, USize, {constant(0, Text, 1), intrinsic(1, USize, DisplayWidth, {0}), returnValue(1)}));

        module.functions.push_back(
            function(4, Text, {constant(0, Text, 5), intrinsic(1, Text, CaseFold, {0}), returnValue(1)}));

        module.functions.push_back(
            function(5, U32,
                     {constant(0, Text, 6), constant(1, USize, 3), intrinsic(2, U32Array, CodePoints, {0}),
                      intrinsic(3, U32, Get, {2, 1}, ArrayFamily), returnValue(3)}));

        module.functions.push_back(
            function(6, Text,
                     {constant(0, Text, 1), constant(1, USize, 3), intrinsic(2, TextArray, Graphemes, {0}),
                      intrinsic(3, Text, Get, {2, 1}, ArrayFamily), returnValue(3)}));

        module.functions.push_back(
            function(7, String, {constant(0, Text, 1), intrinsic(1, String, ToString, {0}), returnValue(1)}));

        module.functions.push_back(function(
            8, Text, {constant(0, Text, 6), constant(1, USize, 3), intrinsic(2, Text, Slice, {0, 1}), returnValue(2)}));

        module.functions.push_back(
            function(9, USize, {constant(0, Text, 7), intrinsic(1, USize, GraphemeCount, {0}), returnValue(1)}));

        module.functions.push_back(
            function(10, USize, {constant(0, Text, 6), intrinsic(1, USize, ByteCount, {0}), returnValue(1)}));

        module.functions.push_back(
            function(11, Bool, {constant(0, Text, 7), intrinsic(1, Bool, Empty, {0}), returnValue(1)}));

        module.functions.push_back(
            function(12, USize, {constant(0, Text, 6), intrinsic(1, USize, Count, {0}), returnValue(1)}));

        module.functions.push_back(function(
            13, Bool,
            {constant(0, Text, 6), constant(1, Text, 8), intrinsic(2, Bool, StartsWith, {0, 1}), returnValue(2)}));

        module.functions.push_back(function(
            14, Bool,
            {constant(0, Text, 6), constant(1, Text, 9), intrinsic(2, Bool, EndsWith, {0, 1}), returnValue(2)}));
        return module;
    }
} // namespace

int main()
{
    bool ok = true;
    const bytecode::DecodeResult decoded = bytecode::decode(bytecode::encode(makeModule()));
    ok &= expect(decoded.succeeded(), "Text intrinsic fixture must survive the real .wiob codec");
    if (!decoded.succeeded())
        return 1;

    vm::Machine machine{decoded.module};
    const vm::ExecutionResult contains = machine.invoke(0);
    ok &= expect(contains.succeeded() && contains.value().asBoolean(), "Text Contains must compare Unicode scalars");
    const vm::ExecutionResult graphemeCount = machine.invoke(1);
    ok &= expect(graphemeCount.succeeded() && graphemeCount.value().asUnsignedInteger() == 5,
                 "GraphemeCount must preserve combining, ZWJ, and regional-indicator clusters");
    const vm::ExecutionResult sliced = machine.invoke(2);
    ok &= expect(sliced.succeeded() && vm::encodeUtf8(sliced.value().asText()) == "👩‍💻é🇹🇷",
                 "SliceGraphemes must slice complete Unicode clusters");
    const vm::ExecutionResult width = machine.invoke(3);
    ok &= expect(width.succeeded() && width.value().asUnsignedInteger() == 10,
                 "DisplayWidth must apply the runtime Unicode width model");
    const vm::ExecutionResult folded = machine.invoke(4);
    ok &= expect(folded.succeeded() && vm::encodeUtf8(folded.value().asText()) == "strasse",
                 "CaseFold must support expanding folds");
    const vm::ExecutionResult codePoint = machine.invoke(5);
    ok &= expect(codePoint.succeeded() && codePoint.value().asUnsignedInteger() == 0x1f30d,
                 "CodePoints must expose Unicode scalars as u32 values");
    const vm::ExecutionResult grapheme = machine.invoke(6);
    ok &= expect(grapheme.succeeded() && vm::encodeUtf8(grapheme.value().asText()) == "👩‍💻",
                 "Graphemes must produce an indexable text array");
    const vm::ExecutionResult utf8 = machine.invoke(7);
    ok &= expect(utf8.succeeded() && utf8.value().asString() == "A👩‍💻é🇹🇷你",
                 "ToString must encode text as UTF-8");
    const vm::ExecutionResult scalarSlice = machine.invoke(8);
    ok &= expect(scalarSlice.succeeded() && vm::encodeUtf8(scalarSlice.value().asText()) == "🌍B",
                 "The one-argument Slice overload must use scalar boundaries");
    const vm::ExecutionResult empty = machine.invoke(9);
    ok &= expect(empty.succeeded() && empty.value().asUnsignedInteger() == 0,
                 "Empty text must contain zero grapheme clusters");
    const vm::ExecutionResult byteCount = machine.invoke(10);
    ok &= expect(byteCount.succeeded() && byteCount.value().asUnsignedInteger() == 6,
                 "ByteCount must report encoded UTF-8 size");
    const vm::ExecutionResult isEmpty = machine.invoke(11);
    ok &= expect(isEmpty.succeeded() && isEmpty.value().asBoolean(), "Empty must recognize empty text");
    const vm::ExecutionResult scalarCount = machine.invoke(12);
    ok &= expect(scalarCount.succeeded() && scalarCount.value().asUnsignedInteger() == 3,
                 "Count must report Unicode scalar count");
    const vm::ExecutionResult startsWith = machine.invoke(13);
    ok &= expect(startsWith.succeeded() && startsWith.value().asBoolean(),
                 "StartsWith must compare Unicode scalar prefixes");
    const vm::ExecutionResult endsWith = machine.invoke(14);
    ok &= expect(endsWith.succeeded() && endsWith.value().asBoolean(), "EndsWith must compare Unicode scalar suffixes");
    return ok ? 0 : 1;
}
