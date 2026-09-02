#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"
#include "wio/wir/lowered_ir_printer.h"
#include "wio/wir/lowered_ir_verifier.h"
#include "wio/wir/lowering_pipeline.h"
#include "wio/wir/typed_ir_builder.h"
#include "wio/wir/typed_ir_printer.h"
#include "wio/wir/typed_ir_verifier.h"
#include "wio_module_contract.h"

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

    std::int32_t exportedStub(const void*, std::uint32_t, void*, void*) { return 0; }
}

int main()
{
    using namespace wio;
    using namespace wio::wir;
    namespace typed = wio::wir::typed;
    namespace lowered = wio::wir::lowered;

    bool ok = true;
    Lexer lexer(
        "using cpp::header(\"module_sdk_fixture.h\"); "
        "[export] component Snapshot { value: i32; } "
        "[export, command(\"sum\")] fn Add(left: i32, right: i32) -> i32 { return left + right; } "
        "[export, instantiate(i32)] fn Identity<T>(value: T) -> T { return value; } "
        "[module::api_version] fn ApiVersion() -> u32 { return 11u32; } "
        "[module::load] fn Load() -> i32 { return 0; } "
        "[module::update] fn Update(delta: f32) { } "
        "[module::save_state] fn Save() -> i32 { return 7; } "
        "[module::restore_state] fn Restore(snapshot: i32) -> i32 { return snapshot; } "
        "[module::unload] fn Unload() { }",
        "typed_wir_module_sdk_model_test.wio");
    Parser parser(lexer.lex());
    const Ref<Program> program = parser.parseProgram();
    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);

    typed::BuildResult build = typed::Builder{}.build(program, typed::BuildOptions{
        .logicalModuleName = "example.module_sdk",
        .moduleKind = ModuleKind::WioLibrary,
        .stateSchemaVersion = 3u
    });
    if (!build.succeeded())
        for (const auto& diagnostic : build.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    ok &= expect(build.succeeded(), "Module/SDK source must build into Typed WIR");
    const typed::Module& module = build.module();
    ok &= expect(module.contract.kind == ModuleKind::WioLibrary &&
        module.contract.logicalName == "example.module_sdk" &&
        module.contract.stableId == stableModuleHash(module.contract.stableKey),
        "Exported source must carry a stable Wio-library identity");
    ok &= expect(module.contract.imports.size() == 1 &&
        module.contract.imports.front().kind == ModuleImportKind::NativeHeader,
        "Native header imports must be explicit module dependencies");
    ok &= expect(module.contract.exports.size() == 3 &&
        module.contract.callTable.entries.size() == module.contract.exports.size(),
        "Function, generic specialization and type exports must occupy stable call-table slots");

    bool genericExport = false;
    bool typeExport = false;
    bool commandExport = false;
    for (std::size_t index = 0; index < module.contract.exports.size(); ++index)
    {
        const ModuleExport& entry = module.contract.exports[index];
        ok &= expect(entry.callTableSlot == index &&
            module.contract.callTable.entries[index] == entry.stableId,
            "Every export must agree with its canonical SDK call-table slot");
        genericExport |= entry.kind == ModuleExportKind::GenericFunctionSpecialization &&
            entry.genericArguments.size() == 1 && entry.parameterTypes.size() == 1 &&
            entry.parameterTypes.front() == entry.genericArguments.front() &&
            entry.returnType == entry.genericArguments.front();
        typeExport |= entry.kind == ModuleExportKind::ComponentType && entry.type;
        commandExport |= entry.role == ModuleExportRole::Command && entry.roleName == "sum";
    }
    ok &= expect(genericExport && typeExport && commandExport,
        "Generic specialization and component reflection exports must remain distinct");
    ok &= expect(module.contract.lifecycle.apiVersion && module.contract.lifecycle.load &&
        module.contract.lifecycle.update && module.contract.lifecycle.unload &&
        module.contract.lifecycle.supportsStateTransfer() &&
        module.contract.lifecycle.stateSchemaVersion == 3u,
        "Lifecycle and paired hot-reload state hooks must be frozen in the module contract");
    ok &= expect(!module.contract.reflection.empty(),
        "Named module types must receive stable reflection descriptors");

    const typed::VerificationResult typedVerification = typed::Verifier{}.verify(module);
    if (!typedVerification.succeeded())
        for (const auto& diagnostic : typedVerification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    ok &= expect(typedVerification.succeeded(), "Canonical Typed WIR module contract must verify");

    const std::string typedText = typed::Printer{}.print(module);
    ok &= expect(typedText.find("contract kind=wio-library") != std::string::npos &&
        typedText.find("generic-function-specialization") != std::string::npos &&
        typedText.find("lifecycle api-version=") != std::string::npos,
        "Typed WIR text must expose module, generic export and lifecycle decisions");

    LoweringResult lowering = LoweringPipeline{}.lower(module);
    ok &= expect(lowering.succeeded() && lowering.module().contract == module.contract,
        "Lowering must preserve the complete module and SDK contract exactly");
    ok &= expect(lowered::Verifier{}.verify(lowering.module()).succeeded(),
        "Lowered WIR module contract must verify");
    ok &= expect(lowered::Printer{}.print(lowering.module()).find("abi-v11") != std::string::npos,
        "Lowered WIR text must expose the selected SDK ABI descriptor version");

    const WioSdkCallEntry table[] = {{
        WioStableSdkId("fixture:export:Add"), &exportedStub, nullptr
    }};
    const WioSdkExportDescriptor exports[] = {{
        WioStableSdkId("fixture:export:Add"), "fixture:export:Add", "Add", "Add", nullptr, 0u,
        WIO_SDK_EXPORT_FUNCTION, WIO_SDK_EXPORT_ORDINARY, 0u
    }};
    const WioSdkModuleContract sdkContract{
        WIO_SDK_MODULE_CONTRACT_VERSION,
        sizeof(WioSdkModuleContract),
        WIO_SDK_MODULE_WIO_LIBRARY,
        1u,
        WioStableSdkId("fixture"),
        "fixture",
        WIO_SDK_LIFECYCLE_SAVE_STATE | WIO_SDK_LIFECYCLE_RESTORE_STATE,
        1u,
        exports,
        0u,
        nullptr,
        1u,
        table
    };
    ok &= expect(WioValidateSdkModuleContract(&sdkContract) &&
        WioFindSdkExport(&sdkContract, WioStableSdkId("fixture:export:Add")) == &exports[0],
        "SDK sidecar must validate and resolve stable exports without C++ symbol guessing");

    typed::Module damaged = module;
    damaged.contract.exports.front().callTableSlot = 99u;
    ok &= expect(!typed::Verifier{}.verify(damaged).succeeded(),
        "Typed verifier must reject drift between export descriptors and call-table slots");

    return ok ? 0 : 1;
}
