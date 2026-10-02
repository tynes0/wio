#include "wio/bytecode/codec.h"
#include "wio/bytecode/verifier.h"
#include "wio/vm/machine.h"

#include <bit>
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
    constexpr std::uint32_t U32 = 3;
    constexpr std::uint32_t USize = 4;
    constexpr std::uint32_t String = 5;
    constexpr std::uint32_t Any = 6;
    constexpr std::uint32_t NullableI32 = 7;
    constexpr std::uint32_t I32Array = 8;
    constexpr std::uint32_t RangeIterator = 9;
    constexpr std::uint32_t ArrayIterator = 10;
    constexpr std::uint32_t Mode = 11;
    constexpr std::uint32_t Permission = 12;
    constexpr std::uint32_t OptionI32 = 13;
    constexpr std::uint32_t ResultError = 14;
    constexpr std::uint32_t ResultI32 = 15;
    constexpr std::uint32_t ResultString = 16;
    constexpr std::uint32_t StringI32Dictionary = 17;
    constexpr std::uint32_t DictionaryIterator = 18;

    enum StringId : std::uint32_t
    {
        Ready,
        Read,
        Write,
        Name,
        Value,
        IsValid,
        Has,
        With,
        RangeInclusive,
        Array,
        Dictionary,
        IteratorValue,
        IteratorIndex,
        First,
        Second,
        Some,
        None,
        Ok,
        Err,
        Key,
        Empty,
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
                                std::vector<bytecode::Instruction> instructions,
                                std::vector<bytecode::Parameter> parameters = {})
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

    bytecode::Instruction enumConstant(const std::uint32_t result, const std::uint32_t type, const StringId selector)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::EnumConstant);
        output.result = result;
        output.resultType = type;
        output.targetType = type;
        output.selector = selector;
        return output;
    }

    bytecode::Instruction intrinsic(const std::uint32_t result, const std::uint32_t resultType,
                                    const std::uint32_t targetType, const std::uint8_t family, const StringId selector,
                                    std::vector<std::uint32_t> operands)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::IntrinsicCall);
        output.result = result;
        output.resultType = resultType;
        output.targetType = targetType;
        output.intrinsicFamily = family;
        output.selector = selector;
        output.operands = std::move(operands);
        return output;
    }

    bytecode::Module makeModule()
    {
        bytecode::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.strings = {
            "Ready", "Read",       "Write",     "Name",      "Value", "IsValid", "Has",  "With", "range.inclusive",
            "array", "dictionary", "__value__", "__index__", "first", "second",  "Some", "None", "Ok",
            "Err",   "key",        ""};
        module.constants = {
            bytecode::Constant{},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 1},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 2},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 3},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 5},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 10},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 20},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 42},
            bytecode::Constant{.kind = bytecode::ConstantKind::UnsignedInteger, .bits = 1},
            bytecode::Constant{.kind = bytecode::ConstantKind::UnsignedInteger, .bits = 2},
            bytecode::Constant{.kind = bytecode::ConstantKind::String, .string = Key},
        };
        module.types = {
            bytecode::Type{.kind = 1},
            bytecode::Type{.kind = 2},
            bytecode::Type{.kind = 5},
            bytecode::Type{.kind = 10},
            bytecode::Type{.kind = 12},
            bytecode::Type{.kind = 17},
            bytecode::Type{.kind = 19},
            bytecode::Type{.kind = 30, .arguments = {I32}},
            bytecode::Type{.kind = 31, .arguments = {I32}},
            bytecode::Type{.kind = 35, .arguments = {I32}},
            bytecode::Type{.kind = 35, .arguments = {I32Array}},
            bytecode::Type{
                .kind = 28, .nominalKind = 4, .enumUnderlyingType = I32, .enumCases = {{.name = Ready, .rawValue = 1}}},
            bytecode::Type{.kind = 28,
                           .nominalKind = 5,
                           .enumUnderlyingType = U32,
                           .enumCases = {{.name = Read, .rawValue = 1}, {.name = Write, .rawValue = 2}}},
            bytecode::Type{.kind = 28,
                           .arguments = {I32},
                           .nominalKind = 2,
                           .nominalValueModel = 3,
                           .fields = {{.type = Bool}, {.type = I32}}},
            bytecode::Type{.kind = 28, .nominalKind = 1, .fields = {{.type = I32}}},
            bytecode::Type{.kind = 28,
                           .arguments = {I32},
                           .nominalKind = 2,
                           .nominalValueModel = 4,
                           .fields = {{.type = Bool}, {.type = I32}, {.type = ResultError}}},
            bytecode::Type{.kind = 28,
                           .arguments = {String},
                           .nominalKind = 2,
                           .nominalValueModel = 4,
                           .fields = {{.type = Bool}, {.type = String}, {.type = ResultError}}},
            bytecode::Type{.kind = 32, .arguments = {String, I32}},
            bytecode::Type{.kind = 35, .arguments = {StringI32Dictionary}},
        };

        module.functions.push_back(function(0, Mode, {enumConstant(0, Mode, Ready), returnValue(0)}));
        module.functions.push_back(function(
            1, String, {enumConstant(0, Mode, Ready), intrinsic(1, String, Mode, 5, Name, {0}), returnValue(1)}));
        module.functions.push_back(function(2, Bool,
                                            {enumConstant(0, Permission, Read), enumConstant(1, Permission, Write),
                                             intrinsic(2, Permission, Permission, 6, With, {0, 1}),
                                             intrinsic(3, Bool, Permission, 6, Has, {2, 1}), returnValue(3)}));

        bytecode::Instruction box = instruction(bytecode::Opcode::AnyBox);
        box.result = 1;
        box.resultType = Any;
        box.targetType = I32;
        box.operands = {0};
        box.signatureTypes = {I32};
        bytecode::Instruction test = instruction(bytecode::Opcode::AnyTypeTest);
        test.result = 2;
        test.resultType = Bool;
        test.targetType = I32;
        test.operands = {1};
        bytecode::Instruction cast = instruction(bytecode::Opcode::AnyCheckedCast);
        cast.result = 3;
        cast.resultType = I32;
        cast.targetType = I32;
        cast.operands = {1};
        module.functions.push_back(function(3, I32, {constant(0, I32, 7), box, test, cast, returnValue(3)}));

        bytecode::Instruction wrap = instruction(bytecode::Opcode::NullableWrap);
        wrap.result = 1;
        wrap.resultType = NullableI32;
        wrap.targetType = NullableI32;
        wrap.operands = {0};
        bytecode::Instruction unwrap = instruction(bytecode::Opcode::NullableUnwrap);
        unwrap.result = 2;
        unwrap.resultType = I32;
        unwrap.targetType = I32;
        unwrap.operands = {1};
        module.functions.push_back(function(4, I32, {constant(0, I32, 6), wrap, unwrap, returnValue(2)}));

        bytecode::Instruction variantTest = instruction(bytecode::Opcode::VariantTest);
        variantTest.result = 1;
        variantTest.resultType = Bool;
        variantTest.operands = {0};
        variantTest.selector = Some;
        module.functions.push_back(function(5, Bool, {variantTest, returnValue(1)}, {{.value = 0, .type = OptionI32}}));
        bytecode::Instruction variantPayload = instruction(bytecode::Opcode::VariantPayload);
        variantPayload.result = 1;
        variantPayload.resultType = I32;
        variantPayload.operands = {0};
        variantPayload.selector = Some;
        module.functions.push_back(
            function(6, I32, {variantPayload, returnValue(1)}, {{.value = 0, .type = OptionI32}}));

        bytecode::Instruction isError = instruction(bytecode::Opcode::ResultIsError);
        isError.result = 1;
        isError.resultType = Bool;
        isError.operands = {0};
        module.functions.push_back(function(7, Bool, {isError, returnValue(1)}, {{.value = 0, .type = ResultI32}}));
        bytecode::Instruction resultValue = instruction(bytecode::Opcode::ResultValue);
        resultValue.result = 1;
        resultValue.resultType = I32;
        resultValue.operands = {0};
        module.functions.push_back(function(8, I32, {resultValue, returnValue(1)}, {{.value = 0, .type = ResultI32}}));
        bytecode::Instruction resultUnwrap = instruction(bytecode::Opcode::ResultUnwrap);
        resultUnwrap.result = 1;
        resultUnwrap.resultType = I32;
        resultUnwrap.operands = {0};
        module.functions.push_back(function(9, I32, {resultUnwrap, returnValue(1)}, {{.value = 0, .type = ResultI32}}));

        bytecode::Instruction range = instruction(bytecode::Opcode::IteratorCreate);
        range.result = 3;
        range.resultType = RangeIterator;
        range.operands = {0, 1, 2};
        range.signatureTypes = {I32, I32, I32};
        range.selector = RangeInclusive;
        bytecode::Instruction rangeAdvance = instruction(bytecode::Opcode::IteratorAdvance);
        rangeAdvance.operands = {3};
        bytecode::Instruction rangeHasNext = instruction(bytecode::Opcode::IteratorHasNext);
        rangeHasNext.result = 4;
        rangeHasNext.resultType = Bool;
        rangeHasNext.operands = {3};
        bytecode::Instruction rangeValue = instruction(bytecode::Opcode::IteratorValue);
        rangeValue.result = 5;
        rangeValue.resultType = I32;
        rangeValue.operands = {3};
        rangeValue.selector = IteratorValue;
        module.functions.push_back(function(10, I32,
                                            {constant(0, I32, 1), constant(1, I32, 4), constant(2, I32, 2), range,
                                             rangeHasNext, rangeAdvance, rangeValue, returnValue(5)}));

        bytecode::Instruction array = instruction(bytecode::Opcode::ArrayCreate);
        array.result = 2;
        array.resultType = I32Array;
        array.operands = {0, 1};
        bytecode::Instruction arrayIterator = instruction(bytecode::Opcode::IteratorCreate);
        arrayIterator.result = 3;
        arrayIterator.resultType = ArrayIterator;
        arrayIterator.operands = {2};
        arrayIterator.signatureTypes = {I32Array};
        arrayIterator.selector = Array;
        bytecode::Instruction arrayAdvance = instruction(bytecode::Opcode::IteratorAdvance);
        arrayAdvance.operands = {3};
        bytecode::Instruction arrayHasNext = instruction(bytecode::Opcode::IteratorHasNext);
        arrayHasNext.result = 4;
        arrayHasNext.resultType = Bool;
        arrayHasNext.operands = {3};
        bytecode::Instruction arrayValue = instruction(bytecode::Opcode::IteratorValue);
        arrayValue.result = 5;
        arrayValue.resultType = I32;
        arrayValue.operands = {3};
        arrayValue.selector = IteratorValue;
        bytecode::Instruction arrayIndex = instruction(bytecode::Opcode::IteratorValue);
        arrayIndex.result = 6;
        arrayIndex.resultType = USize;
        arrayIndex.operands = {3};
        arrayIndex.selector = IteratorIndex;
        module.functions.push_back(function(11, I32,
                                            {constant(0, I32, 5), constant(1, I32, 6), array, arrayIterator,
                                             arrayHasNext, arrayAdvance, arrayValue, arrayIndex, returnValue(5)}));

        bytecode::Instruction dictionary = instruction(bytecode::Opcode::DictionaryCreate);
        dictionary.result = 2;
        dictionary.resultType = StringI32Dictionary;
        dictionary.operands = {0, 1};
        dictionary.signatureTypes = {String, I32};
        dictionary.selector = Dictionary;
        bytecode::Instruction dictionaryIterator = instruction(bytecode::Opcode::IteratorCreate);
        dictionaryIterator.result = 3;
        dictionaryIterator.resultType = DictionaryIterator;
        dictionaryIterator.operands = {2};
        dictionaryIterator.signatureTypes = {StringI32Dictionary};
        dictionaryIterator.selector = Dictionary;
        bytecode::Instruction dictionaryHasNext = instruction(bytecode::Opcode::IteratorHasNext);
        dictionaryHasNext.result = 4;
        dictionaryHasNext.resultType = Bool;
        dictionaryHasNext.operands = {3};
        bytecode::Instruction dictionaryValue = instruction(bytecode::Opcode::IteratorValue);
        dictionaryValue.result = 5;
        dictionaryValue.resultType = I32;
        dictionaryValue.operands = {3};
        dictionaryValue.selector = Second;
        module.functions.push_back(function(12, I32,
                                            {constant(0, String, 10), constant(1, I32, 7), dictionary,
                                             dictionaryIterator, dictionaryHasNext, dictionaryValue, returnValue(5)}));

        bytecode::Instruction propagate = instruction(bytecode::Opcode::ResultPropagate);
        propagate.operands = {0};
        propagate.targetType = ResultString;
        module.functions.push_back(function(13, ResultString, {propagate}, {{.value = 0, .type = ResultI32}}));

        bytecode::Instruction absentUnwrap = instruction(bytecode::Opcode::NullableUnwrap);
        absentUnwrap.result = 1;
        absentUnwrap.resultType = I32;
        absentUnwrap.targetType = I32;
        absentUnwrap.operands = {0};
        module.functions.push_back(
            function(14, I32, {absentUnwrap, returnValue(1)}, {{.value = 0, .type = NullableI32}}));
        return module;
    }
} // namespace

int main()
{
    bool ok = true;
    const bytecode::DecodeResult decoded = bytecode::decode(bytecode::encode(makeModule()));
    ok &= expect(decoded.succeeded(), "Value-model fixture must survive the real .wiob codec");
    if (!decoded.succeeded())
    {
        std::cerr << decoded.error << '\n';
        return 1;
    }
    const bytecode::VerificationResult verification = bytecode::Verifier{}.verify(decoded.module);
    if (!verification.succeeded())
        for (const bytecode::VerificationDiagnostic& diagnostic : verification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    ok &= expect(verification.succeeded(), "Value-model fixture must pass strict bytecode verification");
    if (!verification.succeeded())
        return 1;

    vm::Machine machine{decoded.module};
    const vm::ExecutionResult enumValue = machine.invoke(0);
    ok &= expect(enumValue.succeeded() && enumValue.value().asSignedInteger() == 1,
                 "Enum constants must preserve their underlying value");
    const vm::ExecutionResult enumName = machine.invoke(1);
    ok &= expect(enumName.succeeded() && enumName.value().asString() == "Ready",
                 "Enum Name must resolve canonical case metadata");
    const vm::ExecutionResult flags = machine.invoke(2);
    ok &= expect(flags.succeeded() && flags.value().asBoolean(),
                 "Flagset With/Has must operate on the underlying bit pattern");
    const vm::ExecutionResult any = machine.invoke(3);
    ok &= expect(any.succeeded() && any.value().asSignedInteger() == 42,
                 "Any box, type-test, and checked-cast must preserve the payload");
    const vm::ExecutionResult nullable = machine.invoke(4);
    ok &= expect(nullable.succeeded() && nullable.value().asSignedInteger() == 20,
                 "Nullable wrap/unwrap must preserve a present payload");
    const vm::Value absent = vm::Value::null();
    const vm::ExecutionResult absentNullable = machine.invoke(14, std::span{&absent, 1});
    ok &= expect(!absentNullable.succeeded() && absentNullable.error().code == "WVM1187",
                 "Nullable unwrap must reject an absent payload deterministically");

    const vm::Value option = vm::Value::object(OptionI32, {vm::Value::boolean(true), vm::Value::signedInteger(42)});
    const vm::ExecutionResult optionTest = machine.invoke(5, std::span{&option, 1});
    const vm::ExecutionResult optionPayload = machine.invoke(6, std::span{&option, 1});
    ok &= expect(optionTest.succeeded() && optionTest.value().asBoolean() && optionPayload.succeeded() &&
                     optionPayload.value().asSignedInteger() == 42,
                 "Option variant tests and payload projections must use the canonical nominal layout");

    const vm::Value error = vm::Value::component(ResultError, {vm::Value::signedInteger(7)});
    const vm::Value success =
        vm::Value::object(ResultI32, {vm::Value::boolean(true), vm::Value::signedInteger(42), error});
    const vm::Value failure =
        vm::Value::object(ResultI32, {vm::Value::boolean(false), vm::Value::signedInteger(0), error});
    const vm::ExecutionResult successTest = machine.invoke(7, std::span{&success, 1});
    const vm::ExecutionResult successValue = machine.invoke(8, std::span{&success, 1});
    const vm::ExecutionResult failedUnwrap = machine.invoke(9, std::span{&failure, 1});
    ok &= expect(successTest.succeeded() && !successTest.value().asBoolean() && successValue.succeeded() &&
                     successValue.value().asSignedInteger() == 42,
                 "Result tests and success projections must preserve the canonical result layout");
    ok &= expect(!failedUnwrap.succeeded() && failedUnwrap.error().code == "WVM1189",
                 "Checked Result unwrap must reject an error value deterministically");

    const vm::ExecutionResult range = machine.invoke(10);
    const vm::ExecutionResult array = machine.invoke(11);
    const vm::ExecutionResult dictionary = machine.invoke(12);
    ok &= expect(range.succeeded() && range.value().asSignedInteger() == 3,
                 "Range iterators must advance by their typed step");
    ok &= expect(array.succeeded() && array.value().asSignedInteger() == 20,
                 "Array iterators must retain and advance over their source");
    ok &= expect(dictionary.succeeded() && dictionary.value().asSignedInteger() == 42,
                 "Dictionary iterators must project mapped values");

    const vm::ExecutionResult propagated = machine.invoke(13, std::span{&failure, 1});
    const vm::Value* propagatedPresent = propagated.succeeded() ? propagated.value().field(0) : nullptr;
    const vm::Value* propagatedError = propagated.succeeded() ? propagated.value().field(2) : nullptr;
    ok &= expect(propagated.succeeded() && propagatedPresent && !propagatedPresent->asBoolean() && propagatedError &&
                     propagatedError->field(0) && propagatedError->field(0)->asSignedInteger() == 7,
                 "Result propagation must rebuild the caller's result type with the same error payload");

    bytecode::Module unresolved = makeModule();
    unresolved.functions[0].blocks[0].instructions[0].opcode = bytecode::Opcode::GenericConstant;
    const bytecode::VerificationResult unresolvedVerification = bytecode::Verifier{}.verify(unresolved);
    ok &= expect(!unresolvedVerification.succeeded(),
                 "Executable bytecode must reject unresolved generic constants before VM execution");
    return ok ? 0 : 1;
}
