#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"
#include "wio/wir/lowered_ir_printer.h"
#include "wio/wir/lowered_ir_verifier.h"
#include "wio/wir/lowering_pipeline.h"
#include "wio/wir/typed_ir_builder.h"
#include "wio/wir/typed_ir_printer.h"
#include "wio/wir/typed_ir_verifier.h"
#include "intrinsics.h"

#include <iostream>
#include <string>
#include <unordered_map>

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

    Lexer lexer(
        "realm std { component ResultError { public code: i32; } } "
        "object Option<T> {} "
        "object Result<T> {} "
        "object Boxed { "
        "  public value: i32; "
        "  OnConstruct(value: i32) { self.value = value; } "
        "} "
        "component Tuple { "
        "  public first: i32; public second: string; "
        "  OnConstruct(first: i32, second: string) { self.first = first; self.second = second; } "
        "} "
        "component Span { "
        "  public start: usize; public count: usize; "
        "  OnConstruct(start: usize, count: usize) { self.start = start; self.count = count; } "
        "} "
        "enum Mode { Idle = 0, Ready = 1 }; "
        "flagset Permission { None = 0u32, Read = 1u32, Write = 2u32 }; "
        "fn FromOption(value: Option<i32>) -> i32 { "
        "  return match (value) { Some(number): number; None(): 0; }; "
        "} "
        "fn FromResult(value: Result<i32>) -> i32 { "
        "  return match (value) { Ok(number): number; Err(error): -1; }; "
        "} "
        "fn BoxInteger(value: i32) -> any { return value; } "
        "fn ValueSurface() -> string { "
        "  mut scores: Dict<string, i32> = { \"hp\": 7, \"mana\": 3 }; "
        "  scores[\"hp\"] = 8; "
        "  scores.Set(\"rage\", 11); "
        "  let hp = scores[\"hp\"]; "
        "  let count = scores.Count(); "
        "  let word = \"Wio\"; "
        "  let letter = word[1usize]; "
        "  let unicode: text = u\"İstanbul 🌍\"; "
        "  let folded = unicode.CaseFold(); "
        "  let unicodeSummary = u$\"${folded}:${unicode.Count()}\"; "
        "  let mode = Mode::Ready; "
        "  let permissions = Permission::Read | Permission::Write; "
        "  let hasRead = permissions.Has(Permission::Read); "
        "  let tuple = Tuple(hp, word); "
        "  let span = Span(1usize, count); "
        "  let boxed: Boxed? = Boxed(hp); "
        "  let payload: any = hp; "
        "  let recovered = payload fit i32; "
        "  let matches = payload is i32; "
        "  return $\"${mode.Name()}:${recovered}:${matches}:${letter}:${tuple.first}:${span.count}:${hasRead}:${unicodeSummary}\"; "
        "} "
        "fn Entry() -> i32 { let summary = ValueSurface(); return summary.Count() fit i32; }",
        "typed_wir_value_container_surface_test.wio");
    Parser parser(lexer.lex());
    const Ref<Program> program = parser.parseProgram();
    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);

    typed::BuildResult build = typed::Builder{}.build(program);
    if (!build.succeeded())
    {
        for (const auto& diagnostic : build.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    }
    bool ok = expect(build.succeeded(), "Value/container source must build into Typed WIR");

    const typed::VerificationResult typedVerification = typed::Verifier{}.verify(build.module());
    if (!typedVerification.succeeded())
    {
        for (const auto& diagnostic : typedVerification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(typedVerification.succeeded(), "Value/container Typed WIR must verify");

    bool sawDictionaryCreate = false;
    bool sawDictionaryGet = false;
    bool sawDictionaryPlace = false;
    bool sawStringIndex = false;
    bool sawStringInterpolation = false;
    bool sawTextInterpolation = false;
    bool sawDictionaryIntrinsic = false;
    bool sawStringIntrinsic = false;
    bool sawTextIntrinsic = false;
    bool sawEnumIntrinsic = false;
    bool sawFlagsetIntrinsic = false;
    bool sawEnumConstant = false;
    bool sawAnyBox = false;
    bool sawAnyCast = false;
    bool sawAnyTest = false;
    bool sawNullableWrap = false;
    bool sawOption = false;
    bool sawResult = false;
    bool sawTuple = false;
    bool sawSpan = false;

    for (const Type& type : build.module().types.types())
    {
        sawOption |= type.nominalValueModel == NominalValueModel::Option;
        sawResult |= type.nominalValueModel == NominalValueModel::Result;
        sawTuple |= type.nominalValueModel == NominalValueModel::Tuple;
        sawSpan |= type.nominalValueModel == NominalValueModel::Span;
    }
    for (const typed::Function& function : build.module().functions)
    {
        for (const typed::BasicBlock& block : function.blocks)
        {
            for (const typed::Instruction& instruction : block.instructions)
            {
                sawDictionaryCreate |= instruction.opcode == typed::Opcode::DictionaryCreate;
                sawDictionaryGet |= instruction.opcode == typed::Opcode::DictionaryGet;
                sawDictionaryPlace |= instruction.opcode == typed::Opcode::DictionaryPlace;
                sawStringIndex |= instruction.opcode == typed::Opcode::IntrinsicCall &&
                    instruction.intrinsicFamily == IntrinsicFamily::String && instruction.selector == "Get";
                sawStringInterpolation |= instruction.opcode == typed::Opcode::Interpolate &&
                    instruction.intrinsicFamily == IntrinsicFamily::String;
                sawTextInterpolation |= instruction.opcode == typed::Opcode::Interpolate &&
                    instruction.intrinsicFamily == IntrinsicFamily::Text;
                sawDictionaryIntrinsic |= instruction.opcode == typed::Opcode::IntrinsicCall &&
                    instruction.intrinsicFamily == IntrinsicFamily::Dictionary;
                sawStringIntrinsic |= instruction.opcode == typed::Opcode::IntrinsicCall &&
                    instruction.intrinsicFamily == IntrinsicFamily::String;
                sawTextIntrinsic |= instruction.opcode == typed::Opcode::IntrinsicCall &&
                    instruction.intrinsicFamily == IntrinsicFamily::Text;
                sawEnumIntrinsic |= instruction.opcode == typed::Opcode::IntrinsicCall &&
                    instruction.intrinsicFamily == IntrinsicFamily::Enum;
                sawFlagsetIntrinsic |= instruction.opcode == typed::Opcode::IntrinsicCall &&
                    instruction.intrinsicFamily == IntrinsicFamily::Flagset;
                sawEnumConstant |= instruction.opcode == typed::Opcode::EnumConstant;
                sawAnyBox |= instruction.opcode == typed::Opcode::AnyBox;
                sawAnyCast |= instruction.opcode == typed::Opcode::AnyCheckedCast;
                sawAnyTest |= instruction.opcode == typed::Opcode::AnyTypeTest;
                sawNullableWrap |= instruction.opcode == typed::Opcode::NullableWrap;
            }
        }
    }

    ok &= expect(sawDictionaryCreate && sawDictionaryGet && sawDictionaryPlace && sawDictionaryIntrinsic,
        "Dictionary literal, read/write index, and intrinsic calls must be explicit WIR operations");
    ok &= expect(sawStringIndex && sawStringInterpolation && sawTextInterpolation &&
        sawStringIntrinsic && sawTextIntrinsic,
        "String/text indexing, interpolation, and intrinsics must remain backend-neutral");
    ok &= expect(sawEnumConstant && sawEnumIntrinsic && sawFlagsetIntrinsic,
        "Enum/flagset constants and intrinsics must be represented explicitly");
    ok &= expect(sawAnyBox && sawAnyCast && sawAnyTest && sawNullableWrap,
        "Any boxing/testing/casting and nullable wrapping must be explicit");
    ok &= expect(sawOption && sawResult && sawTuple && sawSpan,
        "Option, Result, Tuple, and Span nominal value identities must survive semantic mapping");

    const std::string typedText = typed::Printer{}.print(build.module());
    ok &= expect(typedText.find("dictionary-create") != std::string::npos &&
        typedText.find("dictionary-place") != std::string::npos &&
        typedText.find("interpolate text") != std::string::npos &&
        typedText.find("any-box") != std::string::npos &&
        typedText.find("value-model=tuple") != std::string::npos,
        "Typed WIR printer must expose value/container semantics");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    if (!lowering.succeeded())
    {
        for (const auto& diagnostic : lowering.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    }
    ok &= expect(lowering.succeeded(), "Value/container Typed WIR must lower and verify");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(loweredText.find("dictionary-get") != std::string::npos &&
        loweredText.find("intrinsic-call flagset") != std::string::npos &&
        loweredText.find("any-checked-cast") != std::string::npos &&
        loweredText.find("nullable-wrap") != std::string::npos,
        "Lowered WIR must preserve backend-neutral value/container operations");

    typed::Module malformed = build.module();
    bool damagedInterpolation = false;
    for (typed::Function& function : malformed.functions)
    {
        for (typed::BasicBlock& block : function.blocks)
        {
            for (typed::Instruction& instruction : block.instructions)
            {
                if (!damagedInterpolation && instruction.opcode == typed::Opcode::Interpolate)
                {
                    instruction.stringSegments.clear();
                    damagedInterpolation = true;
                }
            }
        }
    }
    ok &= expect(damagedInterpolation && !typed::Verifier{}.verify(malformed).succeeded(),
        "Typed verifier must reject malformed interpolation segment layouts");

    lowered::Module malformedLowered = lowering.module();
    bool damagedAny = false;
    for (lowered::Function& function : malformedLowered.functions)
    {
        for (lowered::BasicBlock& block : function.blocks)
        {
            for (lowered::Instruction& instruction : block.instructions)
            {
                if (!damagedAny && instruction.opcode == lowered::Opcode::AnyBox)
                {
                    instruction.targetType = malformedLowered.types.stringType();
                    damagedAny = true;
                }
            }
        }
    }
    ok &= expect(damagedAny && !lowered::Verifier{}.verify(malformedLowered).succeeded(),
        "Lowered verifier must reject any boxes with incorrect concrete source identity");

    std::unordered_map<std::string, int> nativeDictionary{{"hp", 7}};
    wio::intrinsics::Index(nativeDictionary, std::string{"hp"}) = 8;
    ok &= expect(wio::intrinsics::Index(nativeDictionary, std::string{"hp"}) == 8,
        "Native dictionary indexing must return a mutable mapped-value place");
    bool rejectedMissingKey = false;
    try
    {
        (void)wio::intrinsics::Index(nativeDictionary, std::string{"missing"});
    }
    catch (const wio::runtime::RuntimeException&)
    {
        rejectedMissingKey = true;
    }
    ok &= expect(rejectedMissingKey,
        "Native dictionary indexing must reject missing keys instead of inserting implicitly");

    return ok ? 0 : 1;
}
