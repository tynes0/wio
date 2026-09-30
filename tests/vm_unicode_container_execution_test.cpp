#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include "wio/vm/unicode.h"

#include <bit>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    namespace bytecode = wio::bytecode;
    namespace vm = wio::vm;

    constexpr std::uint32_t I32 = 1;
    constexpr std::uint32_t USize = 2;
    constexpr std::uint32_t String = 3;
    constexpr std::uint32_t Text = 4;
    constexpr std::uint32_t I32Array = 5;

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
        module.strings = {"A🌍é", "Count", "ByteCount", "Slice", "prefix ", " / ", "", "世界", "\xc0\xaf"};
        module.constants = {
            bytecode::Constant{},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = 0},
            bytecode::Constant{.kind = bytecode::ConstantKind::UnsignedInteger, .bits = 1},
            bytecode::Constant{.kind = bytecode::ConstantKind::UnsignedInteger, .bits = 2},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 7},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 11},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = 7},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = 8},
        };
        module.types = {
            bytecode::Type{.kind = 1},  bytecode::Type{.kind = 5},  bytecode::Type{.kind = 12},
            bytecode::Type{.kind = 17}, bytecode::Type{.kind = 18}, bytecode::Type{.kind = 31, .arguments = {I32}},
        };

        auto count = instruction(bytecode::Opcode::IntrinsicCall);
        count.result = 1;
        count.resultType = USize;
        count.operands = {0};
        count.selector = 1;
        count.intrinsicFamily = 4;
        module.functions.push_back(function(0, USize, {constant(0, Text, 1), count, returnValue(1)}));

        auto byteCount = instruction(bytecode::Opcode::IntrinsicCall);
        byteCount.result = 1;
        byteCount.resultType = USize;
        byteCount.operands = {0};
        byteCount.selector = 2;
        byteCount.intrinsicFamily = 4;
        module.functions.push_back(function(1, USize, {constant(0, Text, 1), byteCount, returnValue(1)}));

        auto slice = instruction(bytecode::Opcode::IntrinsicCall);
        slice.result = 3;
        slice.resultType = Text;
        slice.operands = {0, 1, 2};
        slice.selector = 3;
        slice.intrinsicFamily = 4;
        module.functions.push_back(function(
            2, Text, {constant(0, Text, 1), constant(1, USize, 2), constant(2, USize, 3), slice, returnValue(3)}));

        auto interpolation = instruction(bytecode::Opcode::Interpolate);
        interpolation.result = 2;
        interpolation.resultType = Text;
        interpolation.operands = {0, 1};
        interpolation.stringSegments = {4, 5, 6};
        module.functions.push_back(
            function(3, Text, {constant(0, Text, 6), constant(1, I32, 4), interpolation, returnValue(2)}));

        auto array = instruction(bytecode::Opcode::ArrayCreate);
        array.result = 2;
        array.resultType = I32Array;
        array.operands = {0, 1};
        auto get = instruction(bytecode::Opcode::ArrayGet);
        get.result = 4;
        get.resultType = I32;
        get.operands = {2, 3};
        module.functions.push_back(function(
            4, I32, {constant(0, I32, 4), constant(1, I32, 5), array, constant(3, USize, 2), get, returnValue(4)}));

        auto arrayLength = instruction(bytecode::Opcode::ArrayLength);
        arrayLength.result = 3;
        arrayLength.resultType = USize;
        arrayLength.operands = {2};
        module.functions.push_back(
            function(5, USize, {constant(0, I32, 4), constant(1, I32, 5), array, arrayLength, returnValue(3)}));

        module.functions.push_back(function(6, Text, {constant(0, Text, 7), returnValue(0)}));
        return module;
    }
} // namespace

int main()
{
    bool ok = true;
    const bytecode::DecodeResult decoded = bytecode::decode(bytecode::encode(makeModule()));
    ok &= expect(decoded.succeeded(), "Unicode/container VM fixture must survive the real .wiob codec");
    if (!decoded.succeeded())
        return 1;

    vm::Machine machine{decoded.module};
    const vm::ExecutionResult count = machine.invoke(0);
    ok &= expect(count.succeeded() && count.value().asUnsignedInteger() == 4,
                 "text.Count must count Unicode scalars rather than UTF-8 bytes");
    const vm::ExecutionResult byteCount = machine.invoke(1);
    ok &= expect(byteCount.succeeded() && byteCount.value().asUnsignedInteger() == 8,
                 "text.ByteCount must report encoded UTF-8 bytes");
    const vm::ExecutionResult slice = machine.invoke(2);
    ok &= expect(slice.succeeded() && vm::encodeUtf8(slice.value().asText()) == "🌍e",
                 "text.Slice must use Unicode scalar boundaries");
    const vm::ExecutionResult interpolation = machine.invoke(3);
    ok &= expect(interpolation.succeeded() && vm::encodeUtf8(interpolation.value().asText()) == "prefix 世界 / 7",
                 "Unicode interpolation must preserve text operands and scalar formatting");
    const vm::ExecutionResult arrayGet = machine.invoke(4);
    ok &= expect(arrayGet.succeeded() && arrayGet.value().asSignedInteger() == 11,
                 "VM arrays must preserve ordered typed values");
    const vm::ExecutionResult arrayLength = machine.invoke(5);
    ok &= expect(arrayLength.succeeded() && arrayLength.value().asUnsignedInteger() == 2,
                 "VM array length must use aggregate storage");
    const vm::ExecutionResult invalidText = machine.invoke(6);
    ok &= expect(!invalidText.succeeded() && invalidText.error().code == "WVM1049",
                 "Invalid UTF-8 text constants must fail at the VM boundary");

    const vm::Utf8DecodeResult overlong = vm::decodeUtf8("\xc0\xaf");
    ok &= expect(!overlong.succeeded() && overlong.error->offset == 0,
                 "The VM UTF-8 decoder must reject overlong encodings deterministically");
    return ok ? 0 : 1;
}
