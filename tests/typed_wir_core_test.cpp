#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"
#include "wio/wir/typed_ir_builder.h"
#include "wio/wir/typed_ir_printer.h"
#include "wio/wir/typed_ir_verifier.h"

#include <iostream>
#include <string>

namespace
{
    bool expect(const bool condition, const char* message)
    {
        if (condition)
            return true;
        std::cerr << message << '\n';
        return false;
    }

    bool hasCode(
        const wio::wir::typed::VerificationResult& result,
        const std::string_view code)
    {
        for (const auto& diagnostic : result.diagnostics())
        {
            if (diagnostic.code == code)
                return true;
        }
        return false;
    }

    bool hasBuildCode(
        const wio::wir::typed::BuildResult& result,
        const std::string_view code)
    {
        for (const auto& diagnostic : result.diagnostics())
        {
            if (diagnostic.code == code)
                return true;
        }
        return false;
    }

    wio::wir::typed::Module makeAddModule()
    {
        using namespace wio::wir;
        using namespace wio::wir::typed;

        Module module;
        module.name = "typed-wir-test";

        Function add;
        add.id = FunctionId{0};
        add.name = "Add";
        add.returnType = module.types.i32Type();
        add.parameters = {
            Parameter{.id = ValueId{0}, .name = "left", .type = module.types.i32Type()},
            Parameter{.id = ValueId{1}, .name = "right", .type = module.types.i32Type()}
        };

        BasicBlock entry;
        entry.id = BlockId{0};
        entry.name = "entry";
        entry.instructions = {
            Instruction{
                .opcode = Opcode::Binary,
                .result = ValueId{2},
                .resultType = module.types.i32Type(),
                .operands = {ValueId{0}, ValueId{1}},
                .binaryOperator = BinaryOperator::Add
            },
            Instruction{
                .opcode = Opcode::Return,
                .operands = {ValueId{2}}
            }
        };
        add.blocks.push_back(std::move(entry));
        module.functions.push_back(std::move(add));
        return module;
    }
}

int main()
{
    using namespace wio;
    using namespace wio::wir;
    using namespace wio::wir::typed;

    bool ok = true;
    Module module = makeAddModule();

    const TypeId firstNamed = module.types.intern(Type{
        .kind = TypeKind::Named,
        .name = "game::Player"
    });
    const TypeId secondNamed = module.types.intern(Type{
        .kind = TypeKind::Named,
        .name = "game::Player"
    });
    ok &= expect(firstNamed == secondNamed, "Typed WIR type table must intern structurally equal types");

    const VerificationResult validResult = Verifier{}.verify(module);
    ok &= expect(validResult.succeeded(), "Well-formed Typed WIR module must pass verification");

    const std::string printed = Printer{}.print(module);
    const std::string expected =
        "typed-wir module \"typed-wir-test\" {\n"
        "  !t0 = void\n"
        "  !t1 = bool\n"
        "  !t2 = i32\n"
        "  !t3 = string\n"
        "  !t4 = named \"game::Player\"\n"
        "\n"
        "  func @f0 \"Add\"(%v0 \"left\": !t2, %v1 \"right\": !t2) -> !t2 {\n"
        "  ^b0 \"entry\":\n"
        "    %v2: !t2 = add %v0, %v1\n"
        "    return %v2\n"
        "  }\n"
        "}\n";
    ok &= expect(printed == expected, "Typed WIR printer output must be deterministic");

    Module invalidModule = makeAddModule();
    invalidModule.functions.front().blocks.front().instructions.back().operands = {ValueId{99}};
    const VerificationResult invalidResult = Verifier{}.verify(invalidModule);
    ok &= expect(!invalidResult.succeeded(), "Unknown Typed WIR values must fail verification");
    ok &= expect(hasCode(invalidResult, "WIR1400"), "Unknown Typed WIR value must have a stable diagnostic code");

    invalidModule = makeAddModule();
    invalidModule.functions.front().blocks.front().instructions.insert(
        invalidModule.functions.front().blocks.front().instructions.begin(),
        Instruction{.opcode = Opcode::Return, .operands = {ValueId{0}}});
    const VerificationResult earlyTerminatorResult = Verifier{}.verify(invalidModule);
    ok &= expect(hasCode(earlyTerminatorResult, "WIR1303"), "Early terminators must fail Typed WIR verification");

    {
        Lexer lexer(
            "fn Add(left: i32, right: i32) -> i32 { return left + right; } "
            "fn Entry() -> i32 { return Add(20, 22); }",
            "typed_wir_builder_test.wio");
        Parser parser(lexer.lex());
        const Ref<Program> program = parser.parseProgram();
        sema::SemanticAnalyzer analyzer;
        analyzer.analyze(program);

        BuildResult buildResult = Builder{}.build(program);
        ok &= expect(buildResult.succeeded(), "Analyzed Wio function must lower into the initial Typed WIR slice");
        const VerificationResult builtVerification = Verifier{}.verify(buildResult.module());
        ok &= expect(builtVerification.succeeded(), "Typed WIR produced from analyzed Wio must pass verification");

        const std::string builtText = Printer{}.print(buildResult.module());
        ok &= expect(
            builtText.find("func @f0 \"Add\"") != std::string::npos &&
                builtText.find("add %v0, %v1") != std::string::npos,
            "Typed WIR builder must retain resolved function and arithmetic semantics");
    }

    {
        Lexer lexer(
            "fn Pick() -> i32 { return 7; } "
            "fn Entry() -> i32 { return true ? Pick() : 0; }",
            "typed_wir_effect_boundary_test.wio");
        Parser parser(lexer.lex());
        const Ref<Program> program = parser.parseProgram();
        sema::SemanticAnalyzer analyzer;
        analyzer.analyze(program);

        const BuildResult buildResult = Builder{}.build(program);
        ok &= expect(!buildResult.succeeded(), "Initial Typed WIR builder must not eagerly lower side-effecting conditional arms");
        ok &= expect(hasBuildCode(buildResult, "WIR2306"), "Side-effecting conditional rejection must have a stable diagnostic code");
    }

    return ok ? 0 : 1;
}
