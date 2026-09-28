#include "wio/bytecode/codec.h"
#include "wio/bytecode/compiler.h"
#include "wio/bytecode/disassembler.h"
#include "wio/bytecode/verifier.h"
#include "wio/wir/lowering_pipeline.h"

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

    wio::wir::typed::Module makeModule()
    {
        using namespace wio::wir;
        namespace typed = wio::wir::typed;

        typed::Module module;
        module.name = "bytecode-format-test";
        module.contract.logicalName = module.name;
        module.contract.stableKey = "program:" + module.name;
        module.contract.stableId = stableModuleHash(module.contract.stableKey);
        module.contract.callTable.stableId = stableModuleHash(
            module.contract.stableKey + ":sdk-call-table:v" + std::to_string(ModuleAbiDescriptorVersion));

        typed::Function function;
        function.id = FunctionId{0};
        function.name = "AddOne";
        function.returnType = module.types.i32Type();
        function.callableType = module.types.intern(
            Type{.kind = TypeKind::Function, .arguments = {module.types.i32Type(), module.types.i32Type()}});
        function.parameters.push_back(
            typed::Parameter{.id = ValueId{0}, .name = "value", .type = module.types.i32Type()});

        typed::BasicBlock entry;
        entry.id = BlockId{0};
        entry.name = "entry";
        entry.source.begin = {.file = "format_test.wio", .line = 2, .column = 1};
        entry.source.end = {.file = "format_test.wio", .line = 5, .column = 2};
        entry.instructions = {
            typed::Instruction{.opcode = typed::Opcode::Constant,
                               .result = ValueId{1},
                               .resultType = module.types.i32Type(),
                               .literal = std::int64_t{1}},
            typed::Instruction{.opcode = typed::Opcode::Binary,
                               .result = ValueId{2},
                               .resultType = module.types.i32Type(),
                               .operands = {ValueId{0}, ValueId{1}},
                               .binaryOperator = typed::BinaryOperator::Add},
            typed::Instruction{.opcode = typed::Opcode::Return, .operands = {ValueId{2}}}};
        function.blocks.push_back(std::move(entry));
        module.functions.push_back(std::move(function));
        return module;
    }
} // namespace

int main()
{
    namespace bytecode = wio::bytecode;
    using namespace wio::wir;

    bool ok = true;
    LoweringResult lowering = LoweringPipeline{}.lower(makeModule());
    if (!lowering.succeeded())
    {
        for (const auto& diagnostic : lowering.diagnostics())
            std::cerr << diagnostic.pass << ": " << diagnostic.message << '\n';
    }
    ok &= expect(lowering.succeeded(), "Bytecode fixture must lower successfully");
    if (!lowering.succeeded())
        return 1;

    bytecode::CompileResult compilation = bytecode::Compiler{}.compile(lowering.module());
    ok &= expect(compilation.succeeded(), "Verified Lowered WIR must compile to bytecode");
    if (!compilation.succeeded())
        return 1;
    const bytecode::Module& module = compilation.module();
    ok &= expect(bytecode::Verifier{}.verify(module).succeeded(), "Fresh bytecode module must verify");
    ok &= expect(module.functions.size() == 1 && module.functions.front().blocks.size() == 1 &&
                     module.functions.front().blocks.front().instructions.size() == 3,
                 "Bytecode compiler must preserve the function CFG and instructions");
    ok &= expect(module.functions.front().blocks.front().instructions[1].opcode == bytecode::Opcode::Binary,
                 "Lowered WIR opcode must map to the pinned bytecode instruction set");

    const std::vector<std::byte> first = bytecode::encode(module);
    const std::vector<std::byte> second = bytecode::encode(module);
    ok &= expect(first == second, "Bytecode encoding must be deterministic");
    const bytecode::DecodeResult decoded = bytecode::decode(first);
    ok &= expect(decoded.succeeded(), "Encoded bytecode must decode");
    ok &= expect(decoded.succeeded() && decoded.module == module, "Bytecode round trip must be lossless");
    ok &= expect(decoded.succeeded() && bytecode::Verifier{}.verify(decoded.module).succeeded(),
                 "Decoded bytecode must pass structural verification");

    const std::string disassembly = bytecode::Disassembler{}.disassemble(module);
    ok &= expect(disassembly.find("fn @0 \"AddOne\"") != std::string::npos &&
                     disassembly.find("Binary %v0 %v1") != std::string::npos &&
                     disassembly.find("Return %v2") != std::string::npos,
                 "Disassembler must expose functions, values, and pinned opcode names");

    std::vector<std::byte> corrupted = first;
    corrupted.back() ^= std::byte{0x01};
    ok &= expect(!bytecode::decode(corrupted).succeeded(), "Checksum must reject corrupted payloads");
    ok &= expect(!bytecode::decode(std::span{first}.first(first.size() - 1)).succeeded(),
                 "Loader must reject truncated files");

    bytecode::Module malformed = module;
    malformed.functions.front().blocks.front().instructions.back().targets.push_back(
        bytecode::BranchTarget{.block = 999});
    ok &= expect(!bytecode::Verifier{}.verify(malformed).succeeded(),
                 "Verifier must reject branch targets outside the function CFG");
    return ok ? 0 : 1;
}
