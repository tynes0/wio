#include "wio/codegen/wir_cpp_backend.h"
#include "wio/codegen/wir_native_bridge_plan.h"
#include "wio/common/logger.h"
#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"
#include "wio/wir/lowered_ir_printer.h"
#include "wio/wir/lowered_ir_verifier.h"
#include "wio/wir/lowering_pipeline.h"
#include "wio/wir/typed_ir_builder.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace
{
    bool expect(bool value, const char* message)
    {
        if (!value)
            std::cerr << message << '\n';
        return value;
    }

    std::optional<wio::wir::lowered::Module> buildNoNativeProbe()
    {
        using namespace wio;
        Lexer lexer(R"WIO(
object VmOnly {
    public fn Marker() -> string { return "VM_ONLY_OBJECT_MARKER"; }
}
fn Entry() -> i32 {
    let payload: any = VmOnly();
    if (payload is VmOnly) { return 0; }
    return 1;
}
)WIO",
                    "bridge_empty_probe.wio");
        Parser parser(lexer.lex());
        const auto program = parser.parseProgram();
        sema::SemanticAnalyzer analyzer;
        analyzer.analyze(program);
        if (Logger::get().getErrorCount())
            return std::nullopt;
        const auto typed = wir::typed::Builder{}.build(program);
        if (!typed.succeeded())
            return std::nullopt;
        auto lowered = wir::LoweringPipeline{}.lower(typed.module());
        if (!lowered.succeeded())
            return std::nullopt;
        return lowered.takeModule();
    }
} // namespace

int main()
{
    using namespace wio;
    std::ifstream stream(std::string(WIO_TEST_SOURCE_DIR) + "/tests/vm_native_bridge_pruning.wio");
    std::string source{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (!expect(!source.empty(), "fixture must be readable"))
        return 1;
    // Deterministic code-size gate: unrelated imported Wio bodies must not
    // increase the producer's C++ surface. This is not a runtime benchmark.
    for (int index = 0; index < 128; ++index)
        source += "\nfn DeadStress" + std::to_string(index) + "(value: i32) -> i32 { return value * " +
                  std::to_string(index + 1) + " + 7; }\n";
    Lexer lexer(source, "vm_native_bridge_pruning.wio");
    Parser parser(lexer.lex());
    const auto program = parser.parseProgram();
    // The compiler driver resolves convenience imports; this standalone unit
    // supplies all external async declarations explicitly instead.
    std::erase_if(program->statements,
                  [](const auto& statement)
                  {
                      const auto* use = statement ? statement->template as<UseStatement>() : nullptr;
                      return use && use->isStdLib && use->modulePath == "async";
                  });
    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);
    if (Logger::get().getErrorCount())
        return 1;
    const auto typed = wir::typed::Builder{}.build(program);
    if (!expect(typed.succeeded(), "fixture must build Typed WIR"))
        return 1;
    const auto lowering = wir::LoweringPipeline{}.lower(typed.module());
    for (const auto& diagnostic : lowering.diagnostics())
        std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    if (!expect(lowering.succeeded(), "fixture must lower"))
        return 1;
    const auto& module = lowering.module();
    const auto original = wir::lowered::Printer{}.print(module);
    const auto plan = codegen::WirNativeBridgePlan::build(module);
    const auto verification = wir::lowered::Verifier{}.verify(plan.module);
    for (const auto& diagnostic : verification.diagnostics())
        std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    bool ok = expect(verification.succeeded(), "pruned dependency closure must verify");
    ok &= expect(wir::lowered::Printer{}.print(module) == original, "selection must not mutate VM module");
    ok &= expect(!plan.module.contract.application && plan.module.contract.exports.empty(),
                 "bridge must not export application or SDK program dispatch");
    const auto selected = [&](const std::string& name)
    { return std::ranges::any_of(plan.module.functions, [&](const auto& fn) { return fn.name == name; }); };
    ok &= expect(!selected("Entry") && !selected("VmCallback") && !selected("UnusedBody"),
                 "VM-only free bodies must not become C++ roots");
    ok &= expect(selected("Helper") && plan.module.globals.size() == 1,
                 "hosted method dependencies and global initializers must survive");
    ok &= expect(!plan.dynamicPayloadTypes.empty(), "Any payload discovery must precede pruning");
    for (const auto& fn : plan.module.functions)
    {
        const auto originalFn = std::ranges::find(module.functions, fn.id, &wir::lowered::Function::id);
        ok &= expect(originalFn != module.functions.end() && originalFn->name == fn.name,
                     "stable sparse function IDs must not be renumbered");
    }
    for (const auto& reflected : plan.module.contract.reflection)
    {
        const auto originalType =
            std::ranges::find(module.contract.reflection, reflected.type, &wir::ReflectionDescriptor::type);
        ok &= expect(originalType != module.contract.reflection.end() &&
                         originalType->stableTypeId == reflected.stableTypeId,
                     "ABI type identities must survive selection unchanged");
    }
    const auto full = codegen::WirCppBackend{}.generate(module, {.emitMain = false, .emitNativeTaskSidecars = true});
    const auto bridge = codegen::WirCppBackend{}.generate(
        module, {.emitMain = false, .emitNativeTaskSidecars = true, .nativeBridgeOnly = true});
    for (const auto& diagnostic : bridge.diagnostics())
        std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    ok &= expect(full.succeeded() && bridge.succeeded(), "both full and pruned producers must generate");
    ok &= expect(bridge.code().find("DEAD_FREE_BODY") == std::string::npos &&
                     bridge.code().find("DEAD_OBJECT_BODY") == std::string::npos &&
                     bridge.code().find("ENTRY_VM_ONLY_MARKER") == std::string::npos,
                 "bridge must not contain unrelated Wio executable bodies");
    ok &= expect(bridge.code().find("WioGetNativeTaskApi") != std::string::npos &&
                     bridge.code().find("WioGetNativeAbiRegistry") != std::string::npos,
                 "native registry and async sidecars must remain");
    ok &= expect(bridge.code().size() * 4 < full.code().size() * 3,
                 "stress gate must remove at least 25 percent of generated bytes");
    const auto repeated = codegen::WirCppBackend{}.generate(
        module, {.emitMain = false, .emitNativeTaskSidecars = true, .nativeBridgeOnly = true});
    ok &= expect(bridge.code() == repeated.code(), "selection/emission must be deterministic");
    const auto invalid = codegen::WirCppBackend{}.generate(module, {.nativeBridgeOnly = true});
    ok &= expect(!invalid.succeeded() && invalid.code().empty() && invalid.diagnostics().front().code == "WCPP1220",
                 "invalid bridge mode must fail early");
    auto malformed = module;
    for (auto& function : malformed.functions)
        if (function.name == "Entry")
            function.callableType = wir::TypeId{};
    const auto rejected = codegen::WirCppBackend{}.generate(
        malformed, {.emitMain = false, .emitNativeTaskSidecars = true, .nativeBridgeOnly = true});
    ok &= expect(!rejected.succeeded() && rejected.code().empty() && rejected.diagnostics().front().code == "WCPP1000",
                 "pruning must not conceal malformed VM-only input");
    const auto noNative = buildNoNativeProbe();
    ok &= expect(noNative.has_value(), "no-native Any probe must be a valid canonical program");
    if (noNative)
    {
        const auto empty = codegen::WirNativeBridgePlan::build(*noNative);
        ok &=
            expect(empty.module.functions.empty() && empty.module.globals.empty() && empty.dynamicPayloadTypes.empty(),
                   "no-native program must not root VM-only Any code");
        const auto emptyBridge = codegen::WirCppBackend{}.generate(
            *noNative, {.emitMain = false, .emitNativeTaskSidecars = true, .nativeBridgeOnly = true});
        ok &= expect(emptyBridge.succeeded() && emptyBridge.code().find("VM_ONLY_OBJECT_MARKER") == std::string::npos,
                     "empty registry bridge must generate without unrelated object bodies");
    }
    std::cout << "bridge selection: functions " << module.functions.size() << " -> " << plan.module.functions.size()
              << ", generated bytes " << full.code().size() << " -> " << bridge.code().size() << '\n';
    return ok ? 0 : 1;
}
