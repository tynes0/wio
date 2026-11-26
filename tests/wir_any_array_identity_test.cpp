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
#include <string>

namespace
{
    bool expect(const bool condition, const char* message)
    {
        if (!condition)
            std::cerr << message << '\n';
        return condition;
    }

    template <class Result> bool hasCode(const Result& result, const std::string& code)
    {
        return std::ranges::any_of(result.diagnostics(),
                                   [&](const auto& diagnostic) { return diagnostic.code == code; });
    }

    template <class Module, class Opcode> auto* firstBox(Module& module, const Opcode opcode)
    {
        for (auto& function : module.functions)
            if (function.name == "Entry")
                for (auto& block : function.blocks)
                    for (auto& instruction : block.instructions)
                        if (instruction.opcode == opcode)
                            return &instruction;
        using Instruction = typename decltype(module.functions.front().blocks.front().instructions)::value_type;
        return static_cast<Instruction*>(nullptr);
    }

    bool isMaterialized(const wio::wir::TypeTable& types, const wio::wir::TypeId id)
    {
        const auto& type = types.get(id);
        if (type.kind != wio::wir::TypeKind::Array && type.kind != wio::wir::TypeKind::Dictionary)
            return true;
        return type.name != "literal" &&
               std::ranges::all_of(type.arguments, [&](const auto child) { return isMaterialized(types, child); });
    }
} // namespace

int main()
{
    using namespace wio;
    using namespace wio::wir;
    std::ifstream stream(std::string(WIO_TEST_SOURCE_DIR) + "/tests/vm_native_any_array_identity.wio");
    const std::string source{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (!expect(!source.empty(), "fixture must be readable"))
        return 1;
    Lexer lexer(source, "vm_native_any_array_identity.wio");
    Parser parser(lexer.lex());
    const auto program = parser.parseProgram();
    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);
    if (Logger::get().getErrorCount())
        return 1;
    auto built = typed::Builder{}.build(program);
    if (!expect(built.succeeded(), "Typed WIR fixture must build"))
        return 1;
    auto typedModule = built.module();
    auto* box = firstBox(typedModule, typed::Opcode::AnyBox);
    if (!expect(box != nullptr, "Entry must box the inferred array"))
        return 1;
    const auto dynamic = box->targetType;
    bool ok = expect(typedModule.types.get(dynamic).kind == TypeKind::Array &&
                         typedModule.types.get(dynamic).name.empty() && !typedModule.types.get(dynamic).staticExtent,
                     "Any payload must use the canonical dynamic array identity");
    bool keptLiteralValue = false;
    for (const auto& function : typedModule.functions)
        for (const auto& block : function.blocks)
            for (const auto& instruction : block.instructions)
                if (instruction.result == box->operands.front() && function.name == "Entry")
                    keptLiteralValue = typedModule.types.get(instruction.resultType).name == "literal";
    ok &= expect(keptLiteralValue, "boxing must not retag the source SSA value");

    Type fixedType = typedModule.types.get(dynamic);
    fixedType.staticExtent = 3;
    const auto fixed = typedModule.types.intern(fixedType);
    ok &= expect(canonicalAnyPayloadType(typedModule.types, fixed) == fixed,
                 "declared fixed arrays must keep their identity");
    ok &= expect(!isAnyPayloadTypeCompatible(typedModule.types, fixed, dynamic),
                 "fixed array cannot be erased to a dynamic payload");
    Type wrongElement = typedModule.types.get(dynamic);
    wrongElement.arguments = {typedModule.types.boolType()};
    const auto wrong = typedModule.types.intern(wrongElement);
    ok &= expect(!isAnyPayloadTypeCompatible(typedModule.types, dynamic, wrong),
                 "normalization must not change element type");
    Type literalType = typedModule.types.get(dynamic);
    literalType.name = "literal";
    literalType.staticExtent = 2;
    const auto literal = typedModule.types.intern(literalType);
    const auto dictionary = typedModule.types.intern(
        Type{.kind = TypeKind::Dictionary, .arguments = {typedModule.types.stringType(), literal}});
    const auto materializedDictionary = canonicalAnyPayloadType(typedModule.types, dictionary);
    ok &= expect(typedModule.types.get(materializedDictionary).arguments[1] == dynamic &&
                     isAnyPayloadTypeCompatible(typedModule.types, dictionary, materializedDictionary),
                 "dictionary children must use materialized array identities");
    const auto callable =
        typedModule.types.intern(Type{.kind = TypeKind::Function, .arguments = {literal, typedModule.types.i32Type()}});
    ok &= expect(canonicalAnyPayloadType(typedModule.types, callable) == callable,
                 "function signatures must not be erased like container inference");

    auto badTyped = typedModule;
    auto* badBox = firstBox(badTyped, typed::Opcode::AnyBox);
    badBox->targetType = fixed;
    badBox->signatureTypes = {fixed};
    ok &= expect(hasCode(typed::Verifier{}.verify(badTyped), "WIR1461"),
                 "Typed verifier must reject literal-to-fixed forgery");

    const auto lowering = LoweringPipeline{}.lower(typedModule);
    for (const auto& diagnostic : lowering.diagnostics())
        std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    if (!expect(lowering.succeeded(), "Generic Any boxes must specialize and lower"))
        return 1;
    for (const auto& function : lowering.module().functions)
        for (const auto& block : function.blocks)
            for (const auto& instruction : block.instructions)
                if (instruction.opcode == lowered::Opcode::AnyBox)
                    ok &= expect(isMaterialized(lowering.module().types, instruction.targetType),
                                 "specialized and nested payloads must be materialized");
    auto badLowered = lowering.module();
    auto* loweredBox = firstBox(badLowered, lowered::Opcode::AnyBox);
    if (!expect(loweredBox != nullptr, "lowering must preserve the box"))
        return 1;
    loweredBox->targetType = wrong;
    loweredBox->signatureTypes = {wrong};
    ok &= expect(hasCode(lowered::Verifier{}.verify(badLowered), "LIR1442"),
                 "Lowered verifier must reject wrong-element payload metadata");

    const auto compiled = bytecode::Compiler{}.compile(lowering.module());
    if (!expect(compiled.succeeded(), "canonical payload must compile to bytecode"))
        return 1;
    ok &= expect(bytecode::Verifier{}.verify(compiled.module()).succeeded(), "valid bytecode must verify");
    auto badBytecode = compiled.module();
    // Bytecode names are interned string IDs, unlike the WIR function names.
    bool changed = false;
    for (auto& function : badBytecode.functions)
        if (badBytecode.string(function.name) == "Entry")
            for (auto& block : function.blocks)
                for (auto& instruction : block.instructions)
                    if (!changed && instruction.opcode == bytecode::Opcode::AnyBox)
                    {
                        instruction.targetType = fixed.value();
                        instruction.signatureTypes = {fixed.value()};
                        changed = true;
                    }
    ok &= expect(changed && hasCode(bytecode::Verifier{}.verify(badBytecode), "WBC1065"),
                 "bytecode verifier must reject forged fixed payload identity");
    return ok ? 0 : 1;
}
