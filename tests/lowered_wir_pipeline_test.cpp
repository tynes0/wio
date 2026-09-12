#include "wio/wir/lowered_ir_printer.h"
#include "wio/wir/lowered_ir_verifier.h"
#include "wio/wir/lowering_pipeline.h"
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

    wio::wir::typed::Module makeSelectModule()
    {
        using namespace wio::wir;
        namespace typed = wio::wir::typed;

        typed::Module module;
        module.name = "lowering-test";
        module.contract.logicalName = module.name;
        module.contract.stableKey = "program:" + module.name;
        module.contract.stableId = stableModuleHash(module.contract.stableKey);
        module.contract.callTable.stableId = stableModuleHash(module.contract.stableKey + ":sdk-call-table:v" +
                                                              std::to_string(ModuleAbiDescriptorVersion));

        typed::Function function;
        function.id = FunctionId{0};
        function.name = "Choose";
        function.returnType = module.types.i32Type();
        function.callableType = module.types.intern(
            Type{.kind = TypeKind::Function, .arguments = {module.types.boolType(), module.types.i32Type()}});
        function.parameters.push_back(
            typed::Parameter{.id = ValueId{0}, .name = "condition", .type = module.types.boolType()});

        typed::BasicBlock entry;
        entry.id = BlockId{0};
        entry.name = "entry";
        entry.instructions = {typed::Instruction{.opcode = typed::Opcode::Constant,
                                                 .result = ValueId{1},
                                                 .resultType = module.types.i32Type(),
                                                 .literal = std::int64_t{10}},
                              typed::Instruction{.opcode = typed::Opcode::Constant,
                                                 .result = ValueId{2},
                                                 .resultType = module.types.i32Type(),
                                                 .literal = std::int64_t{20}},
                              typed::Instruction{.opcode = typed::Opcode::Select,
                                                 .result = ValueId{3},
                                                 .resultType = module.types.i32Type(),
                                                 .operands = {ValueId{0}, ValueId{1}, ValueId{2}}},
                              typed::Instruction{.opcode = typed::Opcode::Return, .operands = {ValueId{3}}}};
        function.blocks.push_back(std::move(entry));
        module.functions.push_back(std::move(function));
        return module;
    }
} // namespace

int main()
{
    using namespace wio::wir;
    namespace lowered = wio::wir::lowered;
    namespace typed = wio::wir::typed;

    bool ok = true;
    typed::Module typedModule = makeSelectModule();
    const typed::VerificationResult typedVerification = typed::Verifier{}.verify(typedModule);
    if (!typedVerification.succeeded())
    {
        for (const auto& diagnostic : typedVerification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        expect(false, "Typed select fixture must be valid before lowering");
        return 1;
    }

    LoweringResult lowering = LoweringPipeline{}.lower(typedModule);
    if (!lowering.succeeded())
    {
        for (const auto& diagnostic : lowering.diagnostics())
            std::cerr << diagnostic.pass << ": " << diagnostic.message << '\n';
        expect(false, "Canonical control-flow lowering must succeed");
        return 1;
    }
    ok &= expect(lowering.completedPasses() ==
                     std::vector<std::string>{
                         "verify-typed-wir", "materialize-generic-functions", "verify-specialized-wir",
                         "lower-canonical-control-flow", "lower-object-hierarchy", "fold-canonical-constants",
                         "simplify-control-flow", "propagate-trivial-values", "eliminate-dead-values",
                         "classify-storage-and-escapes", "eliminate-proven-bounds-checks", "verify-lowered-wir"},
                 "Lowering pipeline must expose deterministic completed-pass order");
    ok &= expect(lowering.module().functions.front().blocks.size() == 2,
                 "Canonical optimization must bypass select trampoline blocks");
    ok &= expect(lowered::Verifier{}.verify(lowering.module()).succeeded(),
                 "Pipeline output must remain valid Lowered WIR");

    const std::string printed = lowered::Printer{}.print(lowering.module());
    ok &= expect(printed.find("cond-jump %v0, ^b3(%v1), ^b3(%v2)") != std::string::npos &&
                     printed.find("^b3 \"select.merge.0\"(%v3 \"select.result\": !t2)") != std::string::npos &&
                     printed.find("return %v3") != std::string::npos,
                 "Lowered WIR printer must expose optimized canonical select control flow");

    typed::Module invalidTyped = makeSelectModule();
    invalidTyped.functions.front().blocks.front().instructions.pop_back();
    const LoweringResult rejected = LoweringPipeline{}.lower(invalidTyped);
    ok &= expect(!rejected.succeeded(), "Lowering must reject invalid Typed WIR before transformation");
    ok &= expect(rejected.completedPasses().empty(), "Failed Typed WIR verification must stop the pass pipeline");
    ok &= expect(!rejected.diagnostics().empty() && rejected.diagnostics().front().pass == "verify-typed-wir",
                 "Pre-lowering failure must identify the verifier pass");

    lowered::Module malformed = lowering.module();
    auto& conditional = malformed.functions.front().blocks.front().instructions.back();
    conditional.targets.front().arguments.clear();
    const lowered::VerificationResult malformedResult = lowered::Verifier{}.verify(malformed);
    ok &= expect(!malformedResult.succeeded(), "Lowered WIR verifier must reject missing merge arguments");

    return ok ? 0 : 1;
}
