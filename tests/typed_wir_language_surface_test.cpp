#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"
#include "wio/wir/lowered_ir_printer.h"
#include "wio/wir/lowering_pipeline.h"
#include "wio/wir/typed_ir_builder.h"
#include "wio/wir/typed_ir_printer.h"
#include "wio/wir/typed_ir_verifier.h"

#include <iostream>
#include <string>

namespace
{
    bool expect(const bool condition, const char* message)
    {
        if (condition) return true;
        std::cerr << message << '\n';
        return false;
    }
}

int main()
{
    using namespace wio;
    using namespace wio::wir;
    namespace typed = wio::wir::typed;
    namespace lowered = wio::wir::lowered;

    Lexer lexer(
        "mut total: i32 = 2; "
        "const title: string = \"surface\"; "
        "realm std { "
        "  component ResultError { public code: i32; } "
        "  object Result<T> { "
        "    private value: T; "
        "    OnConstruct(value: T) { self.value = value; } "
        "  } "
        "} "
        "component Number { "
        "  public value: i32; "
        "  fn operator +(rhs: Number) -> Number { return Number(self.value + rhs.value); } "
        "} "
        "fn MakeNumber() -> std::Result<i32>; "
        "fn MakeText() -> std::Result<string>; "
        "fn Propagate() -> std::Result<string> { "
        "  let number = MakeNumber?(); "
        "  return MakeText(); "
        "} "
        "fn Unwrap() -> i32 { return MakeNumber!(); } "
        "fn Take<Args...>(args: Args...) -> i32; "
        "fn Forward<Args...>(args: Args...) -> i32 { return Take(args...); } "
        "fn Surface() -> i32 { "
        "  for value in 1...5 step 2 { if (value == 3) { continue; } total += value; } "
        "  let values = [10, 20, 30]; "
        "  for index | value in values step 2 { total += (index fit i32) + value; } "
        "  mut scores: Dict<string, i32> = { \"hp\": 3, \"mp\": 4 }; "
        "  for key | value in scores { if (value == 4) { break; } total += value; } "
        "  let delay = 250ms; "
        "  let within = 3 in (1...5); "
        "  let combined = Number(1) + Number(2); "
        "  total += combined.value; "
        "  if (delay > 0.0f32) { total += title.Count() fit i32; } "
        "  return total; "
        "} "
        "fn Entry() -> i32 { return Surface(); }",
        "typed_wir_language_surface_test.wio");
    Parser parser(lexer.lex());
    const Ref<Program> program = parser.parseProgram();
    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);

    typed::BuildResult build = typed::Builder{}.build(program);
    if (!build.succeeded())
        for (const auto& diagnostic : build.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    bool ok = expect(build.succeeded(), "Language surface source must build into Typed WIR");

    const auto verification = typed::Verifier{}.verify(build.module());
    if (!verification.succeeded())
    {
        for (const auto& diagnostic : verification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(verification.succeeded(), "Language surface Typed WIR must verify");

    std::size_t iteratorCreates = 0;
    bool sawRange = false;
    bool sawArray = false;
    bool sawDictionary = false;
    bool sawDuration = false;
    bool sawGlobalPlace = false;
    bool sawResultUnwrap = false;
    bool sawResultPropagate = false;
    bool sawPackExpansion = false;
    bool sawRangeContains = false;
    bool sawOperatorDispatch = false;
    for (const typed::Function& function : build.module().functions)
        for (const typed::BasicBlock& block : function.blocks)
            for (const typed::Instruction& instruction : block.instructions)
            {
                if (instruction.opcode == typed::Opcode::IteratorCreate)
                {
                    ++iteratorCreates;
                    sawRange |= instruction.selector == "range.inclusive";
                    sawArray |= instruction.selector == "array";
                    sawDictionary |= instruction.selector == "dictionary";
                }
                sawDuration |= instruction.opcode == typed::Opcode::Constant &&
                    std::holds_alternative<double>(instruction.literal) &&
                    std::get<double>(instruction.literal) == 0.25;
                sawGlobalPlace |= instruction.opcode == typed::Opcode::GlobalPlace;
                sawResultUnwrap |= instruction.opcode == typed::Opcode::ResultUnwrap;
                sawResultPropagate |= instruction.opcode == typed::Opcode::ResultPropagate;
                if (!instruction.expandedOperands.empty())
                    for (const bool expanded : instruction.expandedOperands)
                        sawPackExpansion |= expanded;
                sawRangeContains |= instruction.opcode == typed::Opcode::RangeContains;
                sawOperatorDispatch |= instruction.opcode == typed::Opcode::MethodCall &&
                    instruction.selector.starts_with("__op_");
            }

    ok &= expect(build.module().globals.size() == 2, "Module globals must have stable WIR declarations");
    ok &= expect(iteratorCreates == 3 && sawRange && sawArray && sawDictionary,
        "Range, array, and dictionary for-in loops must use canonical iterator operations");
    ok &= expect(sawDuration, "Duration literals must lower to seconds in Typed WIR");
    ok &= expect(sawGlobalPlace, "Global reads and writes must use global-place");
    ok &= expect(sawResultUnwrap && sawResultPropagate,
        "Result unwrap and propagation must be explicit WIR operations");
    ok &= expect(sawPackExpansion, "Parameter-pack expansion must survive in call metadata");
    ok &= expect(sawRangeContains, "Range containment must be an explicit WIR operation");
    ok &= expect(sawOperatorDispatch, "Resolved operator overloads must lower to pinned callable dispatch");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    if (!lowering.succeeded())
        for (const auto& diagnostic : lowering.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    ok &= expect(lowering.succeeded(), "Language surface Typed WIR must lower and verify");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(lowering.module().globals.size() == 2 &&
        loweredText.find("iterator-create") != std::string::npos &&
        loweredText.find("result-propagate") != std::string::npos &&
        loweredText.find("global-place") != std::string::npos,
        "Lowered WIR must preserve language-surface contracts");

    return ok ? 0 : 1;
}
