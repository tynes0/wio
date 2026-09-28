#include "wio/bytecode/codec.h"
#include "wio/bytecode/compiler.h"
#include "wio/bytecode/disassembler.h"
#include "wio/bytecode/enum_encoding.h"
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
        module.contract.callTable.stableId = stableModuleHash(module.contract.stableKey + ":sdk-call-table:v" +
                                                              std::to_string(ModuleAbiDescriptorVersion));

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
        entry.instructions = {typed::Instruction{.opcode = typed::Opcode::Constant,
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
    ok &= expect(bytecode::encodeEnum(TypeKind::I32) == 5 && bytecode::encodeEnum(AsyncOperation::Wait) == 14 &&
                     bytecode::encodeEnum(typed::BinaryOperator::ShiftRight) == 15,
                 "Non-opcode semantic enums must use pinned bytecode values");
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

    bytecode::Module metadataModule = module;
    bytecode::StringTableBuilder strings{metadataModule};
    metadataModule.types.front().hasNativeBinding = true;
    metadataModule.types.front().nativeBinding =
        bytecode::Type::NativeBinding{.cppName = strings.intern("NativeCounter"),
                                      .header = strings.intern("counter.h"),
                                      .standardLayout = true,
                                      .triviallyCopyable = true};
    metadataModule.types.front().fields.push_back(
        bytecode::Type::Field{.name = strings.intern("value"), .type = 2, .visibility = 2, .isMutable = true});
    bytecode::Function& metadataFunction = metadataModule.functions.front();
    metadataFunction.captureParameterCount = 1;
    metadataFunction.captures.push_back(
        bytecode::Function::Capture{.name = strings.intern("offset"), .type = 2, .kind = 0});
    metadataFunction.hasNativeBinding = true;
    metadataFunction.nativeBinding.symbol = strings.intern("AddOneNative");
    metadataFunction.nativeBinding.header = strings.intern("counter.h");
    metadataFunction.nativeBinding.stableKey = strings.intern("native:AddOneNative");
    metadataFunction.nativeBinding.thunkSymbol = strings.intern("WioThunkAddOne");
    metadataFunction.nativeBinding.parameters.push_back(
        bytecode::Function::NativeAbiValue{.type = 2, .marshalling = 1});
    metadataFunction.nativeBinding.result = bytecode::Function::NativeAbiValue{.type = 2, .marshalling = 1};
    metadataFunction.hasCoroutine = true;
    metadataFunction.coroutine.resultType = 2;
    metadataFunction.coroutine.frameSlots.push_back(
        bytecode::Function::CoroutineFrameSlot{.slot = 0, .value = 0, .type = 2});
    metadataFunction.coroutine.states.push_back(
        bytecode::Function::CoroutineState{.index = 0, .suspendBlock = 0, .resumeBlock = 0, .resultType = 2});
    metadataModule.contract.imports.push_back(bytecode::Import{.stableId = 10,
                                                               .logicalName = strings.intern("std::math"),
                                                               .sourcePath = strings.intern("std/math.wio"),
                                                               .alias = strings.intern("math"),
                                                               .importedSymbols = {strings.intern("min")}});
    metadataModule.contract.exports.push_back(bytecode::Export{.stableId = 11,
                                                               .stableKey = strings.intern("export:AddOne"),
                                                               .logicalName = strings.intern("AddOne"),
                                                               .symbolName = strings.intern("WioAddOne"),
                                                               .function = 0,
                                                               .type = 2,
                                                               .parameterTypes = {2},
                                                               .returnType = 2});
    metadataModule.contract.callTableEntries.push_back(11);
    metadataModule.contract.attributes.push_back(bytecode::AttributeApplication{
        .stableId = 12,
        .canonicalName = strings.intern("Trace"),
        .originParent = strings.intern(""),
        .selector = strings.intern("AddOne"),
        .targetStableId = 11,
        .targetType = 2,
        .targetFunction = 0,
        .runtimeRetained = true,
        .arguments = {bytecode::AttributeArgument{
            .name = strings.intern("category"), .sourceText = strings.intern("math"), .type = 2}},
        .processors = {bytecode::AttributeProcessor{.stableId = 13,
                                                    .canonicalTypeName = strings.intern("TraceProcessor"),
                                                    .hookName = strings.intern("Before"),
                                                    .hookMode = strings.intern("pre"),
                                                    .processorType = 2,
                                                    .hookFunction = 0,
                                                    .valueType = 2}}});
    metadataModule.contract.reflection.push_back(
        bytecode::Reflection{.stableTypeId = 14,
                             .logicalName = strings.intern("Counter"),
                             .type = 2,
                             .runtimeVisible = true,
                             .fields = {bytecode::ReflectedField{
                                 .stableId = 15, .name = strings.intern("value"), .type = 2, .visibility = 2}},
                             .methods = {bytecode::ReflectedMethod{.stableId = 16,
                                                                   .name = strings.intern("AddOne"),
                                                                   .function = 0,
                                                                   .returnType = 2,
                                                                   .parameterTypes = {2},
                                                                   .visibility = 2}},
                             .cases = {bytecode::ReflectedCase{.stableId = 17, .name = strings.intern("Zero")}}});
    metadataModule.contract.systems.push_back(bytecode::System{.stableId = 18,
                                                               .logicalName = strings.intern("CounterSystem"),
                                                               .type = 2,
                                                               .start = 0,
                                                               .update = 0,
                                                               .close = 0});
    metadataModule.contract.hasApplication = true;
    metadataModule.contract.application = bytecode::Application{
        .stableId = 19,
        .logicalName = strings.intern("CounterApp"),
        .type = 2,
        .construct = 0,
        .entry = 0,
        .start = 0,
        .update = 0,
        .close = 0,
        .exit = 0,
        .systems = {2},
        .stages = {bytecode::Stage{.stableId = 20,
                                   .name = strings.intern("Update"),
                                   .after = strings.intern(""),
                                   .runs = {bytecode::StageRun{.targetName = strings.intern("CounterSystem"),
                                                               .methodName = strings.intern("Update"),
                                                               .targetType = 2,
                                                               .function = 0,
                                                               .resources = {bytecode::ResourceBinding{
                                                                   .name = strings.intern("counter"), .type = 2}}}}}}};
    metadataModule.contract.lifecycle =
        bytecode::Lifecycle{.apiVersion = 0, .load = 0, .update = 0, .unload = 0, .saveState = 0, .restoreState = 0};
    const bytecode::DecodeResult metadataRoundTrip = bytecode::decode(bytecode::encode(metadataModule));
    ok &= expect(metadataRoundTrip.succeeded() && metadataRoundTrip.module == metadataModule,
                 "Nominal, capture, native ABI, and coroutine metadata must round trip losslessly");
    ok &= expect(metadataRoundTrip.succeeded() && bytecode::Verifier{}.verify(metadataRoundTrip.module).succeeded(),
                 "Decoded rich metadata must pass bytecode verification");

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

    corrupted = first;
    const auto patchU64 = [](std::vector<std::byte>& bytes, const std::size_t offset, const std::uint64_t value)
    {
        for (std::size_t i = 0; i < 8; ++i)
            bytes[offset + i] = static_cast<std::byte>(value >> (i * 8u));
    };
    patchU64(corrupted, 56, bytecode::HeaderSize);
    patchU64(corrupted, 40, bytecode::payloadChecksum(std::span{corrupted}.subspan(bytecode::HeaderSize)));
    ok &= expect(!bytecode::decode(corrupted).succeeded(),
                 "Loader must reject checksum-valid sections overlapping the directory");

    bytecode::Module malformed = module;
    malformed.functions.front().blocks.front().instructions.back().targets.push_back(
        bytecode::BranchTarget{.block = 999});
    ok &= expect(!bytecode::Verifier{}.verify(malformed).succeeded(),
                 "Verifier must reject branch targets outside the function CFG");

    malformed = module;
    malformed.functions.front().blocks.front().instructions.pop_back();
    ok &= expect(!bytecode::Verifier{}.verify(malformed).succeeded(),
                 "Verifier must reject blocks without a final terminator");

    malformed = module;
    malformed.functions.front().blocks.front().instructions[1].operands.front() = 999;
    ok &= expect(!bytecode::Verifier{}.verify(malformed).succeeded(), "Verifier must reject undefined SSA operands");

    malformed = module;
    malformed.functions.front().blocks.front().instructions[1].result = 1;
    ok &= expect(!bytecode::Verifier{}.verify(malformed).succeeded(), "Verifier must reject duplicate SSA result ids");

    malformed = module;
    malformed.functions.front().blocks.front().instructions[1].opcode = static_cast<bytecode::Opcode>(0xffffu);
    ok &= expect(!bytecode::Verifier{}.verify(malformed).succeeded(), "Verifier must reject unknown pinned opcodes");
    return ok ? 0 : 1;
}
