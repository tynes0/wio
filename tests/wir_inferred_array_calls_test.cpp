#include "wio/bytecode/compiler.h"
#include "wio/bytecode/verifier.h"
#include "wio/common/logger.h"
#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"
#include "wio/wir/lowered_ir_verifier.h"
#include "wio/wir/lowering_pipeline.h"
#include "wio/wir/typed_ir_builder.h"
#include "wio/wir/typed_ir_verifier.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <unordered_map>

namespace
{
    bool expect(bool value, const char* message)
    {
        if (!value)
            std::cerr << message << '\n';
        return value;
    }

    template <class Result> bool hasCode(const Result& result, const char* code)
    {
        return std::ranges::any_of(result.diagnostics(),
                                   [&](const auto& diagnostic) { return diagnostic.code == code; });
    }

    template <class Module, class Opcode> auto* materializingCopy(Module& module, const Opcode opcode)
    {
        using namespace wio::wir;
        for (auto& function : module.functions)
        {
            if (function.name != "Entry")
                continue;
            std::unordered_map<std::uint32_t, TypeId> values;
            for (auto& block : function.blocks)
                for (auto& instruction : block.instructions)
                {
                    if (instruction.result)
                        values[instruction.result.value()] = instruction.resultType;
                    if (instruction.opcode == opcode && instruction.operands.size() == 1 &&
                        values.contains(instruction.operands.front().value()) &&
                        module.types.get(values.at(instruction.operands.front().value())).name == "literal" &&
                        module.types.get(instruction.resultType).name.empty())
                        return &instruction;
                }
        }
        using Instruction = typename decltype(module.functions.front().blocks.front().instructions)::value_type;
        return static_cast<Instruction*>(nullptr);
    }
} // namespace

int main()
{
    using namespace wio;
    using namespace wio::wir;
    std::ifstream stream(std::string(WIO_TEST_SOURCE_DIR) + "/tests/wir_inferred_array_calls.wio");
    std::string source{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    Lexer lexer(source, "wir_inferred_array_calls.wio");
    Parser parser(lexer.lex());
    const auto program = parser.parseProgram();
    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);
    if (Logger::get().getErrorCount())
        return 1;
    const auto built = typed::Builder{}.build(program);
    if (!expect(built.succeeded(), "inferred argument fixture must build"))
        return 1;
    auto module = built.module();
    auto* copy = materializingCopy(module, typed::Opcode::Copy);
    if (!expect(copy != nullptr, "inferred array must materialize as Copy, not numeric Convert"))
        return 1;
    Type wrong = module.types.get(copy->resultType);
    wrong.arguments = {module.types.boolType()};
    const auto wrongType = module.types.intern(wrong);
    auto invalid = module;
    materializingCopy(invalid, typed::Opcode::Copy)->resultType = wrongType;
    bool ok = expect(hasCode(typed::Verifier{}.verify(invalid), "WIR1466"), "wrong element copy must reject");
    invalid = module;
    materializingCopy(invalid, typed::Opcode::Copy)->opcode = typed::Opcode::Convert;
    ok &= expect(hasCode(typed::Verifier{}.verify(invalid), "WIR1422"), "nonnumeric Convert must still reject");
    const auto lowering = LoweringPipeline{}.lower(module);
    if (!expect(lowering.succeeded(), "inferred/nested generic argument copies must lower"))
        return 1;
    auto badLowered = lowering.module();
    auto* loweredCopy = materializingCopy(badLowered, lowered::Opcode::CopyValue);
    if (!expect(loweredCopy != nullptr, "copy must survive canonical lowering"))
        return 1;
    loweredCopy->resultType = wrongType;
    ok &= expect(hasCode(lowered::Verifier{}.verify(badLowered), "LIR1446"), "lowered wrong-element copy must reject");
    const auto compiled = bytecode::Compiler{}.compile(lowering.module());
    if (!expect(compiled.succeeded(), "materialized argument bytecode must compile"))
        return 1;
    auto badBytecode = compiled.module();
    bool changed = false;
    for (auto& function : badBytecode.functions)
        if (badBytecode.string(function.name) == "Entry")
            for (auto& block : function.blocks)
                for (auto& instruction : block.instructions)
                    if (!changed && instruction.opcode == bytecode::Opcode::CopyValue)
                    {
                        instruction.resultType = wrongType.value();
                        changed = true;
                    }
    ok &= expect(changed && hasCode(bytecode::Verifier{}.verify(badBytecode), "WBC1119"),
                 "bytecode must reject a forged copy identity");
    return ok ? 0 : 1;
}
