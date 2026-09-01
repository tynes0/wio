#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"
#include "wio/wir/lowered_ir_printer.h"
#include "wio/wir/lowered_ir_verifier.h"
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
        if (condition)
            return true;
        std::cerr << message << '\n';
        return false;
    }
}

int main()
{
    using namespace wio;
    using namespace wio::wir;
    namespace lowered = wio::wir::lowered;
    namespace typed = wio::wir::typed;

    bool ok = true;
    Lexer lexer(
        "object Owner { "
        "  public id: i32; "
        "  OnConstruct(id: i32) { self.id = id; } "
        "  OnDestruct() {} "
        "} "
        "component Payload { "
        "  public label: string; "
        "  OnConstruct(label: string) { self.label = label; } "
        "  OnDestruct() {} "
        "} "
        "fn Exercise(flag: bool) -> Owner { "
        "  let first = Owner(1); "
        "  let shared = first; "
        "  mut slot = shared; "
        "  slot = first; "
        "  { "
        "    let text: string = \"hello\"; "
        "    let payload = Payload(text); "
        "    if flag { return slot; } "
        "  } "
        "  Owner(99); "
        "  return slot; "
        "} "
        "fn Entry() -> i32 { let owner = Exercise(false); return owner.id; }",
        "typed_wir_ownership_cleanup_test.wio");
    Parser parser(lexer.lex());
    const Ref<Program> program = parser.parseProgram();
    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);

    typed::BuildResult build = typed::Builder{}.build(program);
    if (!build.succeeded())
        for (const auto& diagnostic : build.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    ok &= expect(build.succeeded(), "Ownership source must build into Typed WIR");

    const typed::VerificationResult verification = typed::Verifier{}.verify(build.module());
    if (!verification.succeeded())
    {
        for (const auto& diagnostic : verification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(verification.succeeded(), "Ownership and cleanup Typed WIR must verify");

    bool ownerContract = false;
    bool payloadContract = false;
    for (const Type& type : build.module().types.types())
    {
        if (type.name == "Owner")
            ownerContract = type.ownership == OwnershipModel::ReferenceCounted &&
                type.cleanup == CleanupKind::ReleaseReference;
        if (type.name == "Payload")
            payloadContract = type.ownership == OwnershipModel::OwnedValue &&
                type.cleanup == CleanupKind::DestroyValue;
    }
    ok &= expect(ownerContract && payloadContract,
        "Object and component types must publish distinct ownership/cleanup contracts");

    std::size_t copies = 0;
    std::size_t moves = 0;
    std::size_t replaces = 0;
    std::size_t releases = 0;
    std::size_t drops = 0;
    for (const typed::Function& function : build.module().functions)
    {
        for (const typed::BasicBlock& block : function.blocks)
        {
            for (const typed::Instruction& instruction : block.instructions)
            {
                copies += instruction.opcode == typed::Opcode::Copy;
                moves += instruction.opcode == typed::Opcode::Move;
                replaces += instruction.opcode == typed::Opcode::Replace;
                releases += instruction.opcode == typed::Opcode::Release;
                drops += instruction.opcode == typed::Opcode::Drop;
            }
        }
    }
    ok &= expect(copies >= 4 && moves >= 2 && replaces >= 2 && releases >= 1 && drops >= 8,
        "Typed WIR must make copies, moves, replacements, discarded temporaries, and lexical drops explicit");

    const std::string typedText = typed::Printer{}.print(build.module());
    ok &= expect(
        typedText.find("ownership=reference-counted cleanup=release-reference") != std::string::npos &&
        typedText.find("ownership=owned-value cleanup=destroy-value") != std::string::npos &&
        typedText.find("[borrowed] lifetime=lexical") != std::string::npos &&
        typedText.find("[owned] = copy") != std::string::npos &&
        typedText.find("replace") != std::string::npos,
        "Typed WIR printer must expose ownership and cleanup decisions");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    if (!lowering.succeeded())
        for (const auto& diagnostic : lowering.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    ok &= expect(lowering.succeeded(), "Ownership Typed WIR must lower successfully");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(
        loweredText.find("retain") != std::string::npos &&
        loweredText.find("release ") != std::string::npos &&
        loweredText.find("release-place") != std::string::npos &&
        loweredText.find("drop-place") != std::string::npos &&
        loweredText.find("move-value") != std::string::npos,
        "Lowered WIR must split intrusive reference operations from owned-value cleanup glue");

    typed::Module doubleDrop = build.module();
    bool duplicated = false;
    for (typed::Function& function : doubleDrop.functions)
    {
        for (typed::BasicBlock& block : function.blocks)
        {
            for (std::size_t index = 0; index < block.instructions.size(); ++index)
            {
                if (!duplicated && block.instructions[index].opcode == typed::Opcode::Drop)
                {
                    block.instructions.insert(block.instructions.begin() + index, block.instructions[index]);
                    duplicated = true;
                    break;
                }
            }
            if (duplicated) break;
        }
        if (duplicated) break;
    }
    ok &= expect(duplicated && !typed::Verifier{}.verify(doubleDrop).succeeded(),
        "Typed WIR verifier must reject duplicate cleanup on one control-flow path");

    typed::Module invalidOwnership = build.module();
    bool damagedCopy = false;
    for (typed::Function& function : invalidOwnership.functions)
    {
        for (typed::BasicBlock& block : function.blocks)
        {
            for (typed::Instruction& instruction : block.instructions)
            {
                if (instruction.opcode == typed::Opcode::Copy)
                {
                    instruction.resultOwnership = typed::ValueOwnership::Borrowed;
                    damagedCopy = true;
                    break;
                }
            }
            if (damagedCopy) break;
        }
        if (damagedCopy) break;
    }
    ok &= expect(damagedCopy && !typed::Verifier{}.verify(invalidOwnership).succeeded(),
        "Typed WIR verifier must reject a copy that fails to create an owned claim");

    typed::Module invalidBorrow = build.module();
    bool damagedBorrow = false;
    for (typed::Function& function : invalidBorrow.functions)
    {
        for (typed::BasicBlock& block : function.blocks)
        {
            for (typed::Instruction& instruction : block.instructions)
            {
                if (instruction.resultOwnership == typed::ValueOwnership::Borrowed)
                {
                    instruction.borrowLifetime = typed::BorrowLifetime::None;
                    damagedBorrow = true;
                    break;
                }
            }
            if (damagedBorrow) break;
        }
        if (damagedBorrow) break;
    }
    ok &= expect(damagedBorrow && !typed::Verifier{}.verify(invalidBorrow).succeeded(),
        "Typed WIR verifier must reject a borrow without lifetime metadata");

    lowered::Module invalidLowered = lowering.module();
    bool damagedLowered = false;
    for (lowered::Function& function : invalidLowered.functions)
    {
        for (lowered::BasicBlock& block : function.blocks)
        {
            for (lowered::Instruction& instruction : block.instructions)
            {
                if (instruction.opcode == lowered::Opcode::Retain)
                {
                    instruction.opcode = lowered::Opcode::CopyValue;
                    damagedLowered = true;
                    break;
                }
            }
            if (damagedLowered) break;
        }
        if (damagedLowered) break;
    }
    ok &= expect(damagedLowered && !lowered::Verifier{}.verify(invalidLowered).succeeded(),
        "Lowered WIR verifier must reject value-copy glue for an intrusive reference-counted object");

    return ok ? 0 : 1;
}
