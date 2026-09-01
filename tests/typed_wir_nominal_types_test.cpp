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

    bool ok = true;
    Lexer lexer(
        "component Point {} "
        "object Entity {} "
        "interface Runnable {} "
        "enum Mode { idle = 0 }; "
        "flagset Permission { none = 0u32 }; "
        "fn Consume(point: Point, entity: Entity, runnable: Runnable, mode: Mode, permission: Permission) {}",
        "typed_wir_nominal_types_test.wio");
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
    ok &= expect(build.succeeded(), "Nominal Wio types must build into Typed WIR");
    ok &= expect(
        typed::Verifier{}.verify(build.module()).succeeded(),
        "Nominal type metadata must pass Typed WIR verification");

    const std::unordered_map<std::string, NominalKind> expectedKinds{
        {"Point", NominalKind::Component},
        {"Entity", NominalKind::Object},
        {"Runnable", NominalKind::Interface},
        {"Mode", NominalKind::Enum},
        {"Permission", NominalKind::Flagset}
    };
    std::size_t matchedKinds = 0;
    for (const Type& type : build.module().types.types())
    {
        const auto expected = expectedKinds.find(type.name);
        if (expected != expectedKinds.end() && type.nominalKind == expected->second)
            ++matchedKinds;
    }
    ok &= expect(
        matchedKinds == expectedKinds.size(),
        "Typed WIR must distinguish component, object, interface, enum, and flagset types");

    typed::Module nativeModule = build.module();
    const TypeId nativeType = nativeModule.types.intern(Type{
        .kind = TypeKind::Named,
        .name = "ffi::NativePoint",
        .nominalKind = NominalKind::Component,
        .nominalRepresentation = NominalRepresentation::NativePod,
        .nativeBinding = NativeTypeBinding{.cppName = "ffi::NativePoint", .header = "native_point.h"}
    });
    ok &= expect(nativeType.isValid(), "Native POD component type must receive a stable type id");
    ok &= expect(
        typed::Verifier{}.verify(nativeModule).succeeded(),
        "Native POD representation must be valid for component types");

    typed::Module malformed = build.module();
    const TypeId malformedType = malformed.types.intern(Type{
        .kind = TypeKind::Named,
        .name = "BrokenHandle",
        .nominalKind = NominalKind::Object,
        .nominalRepresentation = NominalRepresentation::NativePod
    });
    ok &= expect(malformedType.isValid(), "Malformed nominal type must still be interned for verification");
    ok &= expect(
        !typed::Verifier{}.verify(malformed).succeeded(),
        "Native POD representation must be rejected for object handles");

    LoweringResult lowering = LoweringPipeline{}.lower(nativeModule);
    ok &= expect(lowering.succeeded(), "Nominal metadata must survive canonical lowering");
    const std::string typedText = typed::Printer{}.print(nativeModule);
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(
        typedText.find("nominal=component representation=native-pod") != std::string::npos &&
            loweredText.find("nominal=component representation=native-pod") != std::string::npos,
        "Typed and Lowered WIR printers must expose nominal ownership and representation");

    return ok ? 0 : 1;
}
