#include "wio/bytecode/codec.h"
#include "wio/bytecode/compiler.h"
#include "wio/bytecode/verifier.h"
#include "wio/vm/machine.h"
#include "wio/wir/lowering_pipeline.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    namespace bytecode = wio::bytecode;
    namespace vm = wio::vm;

    constexpr std::uint32_t Void = 0;
    constexpr std::uint32_t I32 = 1;
    constexpr std::uint32_t RefI32 = 2;
    constexpr std::uint32_t Adder = 3;
    constexpr std::uint32_t Reader = 4;
    constexpr std::uint32_t Box = 5;
    constexpr std::uint32_t RefBox = 6;
    constexpr std::uint32_t AdderFactory = 7;
    constexpr std::uint32_t ReaderFactory = 8;

    bool expect(const bool condition, const std::string_view message)
    {
        if (condition)
            return true;
        std::cerr << message << '\n';
        return false;
    }

    bytecode::Instruction instruction(const bytecode::Opcode opcode)
    {
        bytecode::Instruction result;
        result.opcode = opcode;
        result.constant = 0;
        return result;
    }

    bytecode::Instruction constant(const std::uint32_t result, const bytecode::ConstantId value)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Constant);
        output.result = result;
        output.resultType = I32;
        output.constant = value;
        return output;
    }

    bytecode::Instruction binaryAdd(const std::uint32_t result, const std::uint32_t left, const std::uint32_t right)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Binary);
        output.result = result;
        output.resultType = I32;
        output.operands = {left, right};
        output.binaryOperator = 0;
        return output;
    }

    bytecode::Instruction returnValue(const std::uint32_t value)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Return);
        output.operands = {value};
        return output;
    }

    bytecode::Parameter parameter(const std::uint32_t value, const std::uint32_t type)
    {
        return bytecode::Parameter{.value = value, .type = type};
    }

    bytecode::Function function(const std::uint32_t id, const std::uint32_t returnType,
                                const std::uint32_t callableType, std::vector<bytecode::Parameter> parameters,
                                std::vector<bytecode::Instruction> instructions)
    {
        bytecode::Function output;
        output.id = id;
        output.returnType = returnType;
        output.callableType = callableType;
        output.parameters = std::move(parameters);
        bytecode::Block entry;
        entry.id = 0;
        entry.instructions = std::move(instructions);
        output.blocks.push_back(std::move(entry));
        return output;
    }

    bytecode::Instruction closure(const std::uint32_t result, const std::uint32_t type, const std::uint32_t callee,
                                  const std::uint32_t capture, const std::uint32_t captureType,
                                  const std::uint8_t captureKind)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::ClosureCreate);
        output.result = result;
        output.resultType = type;
        output.callee = callee;
        output.operands = {capture};
        output.signatureTypes = {captureType};
        output.captureKinds = {captureKind};
        return output;
    }

    bytecode::Instruction indirectCall(const std::uint32_t result, const std::uint32_t callable,
                                       std::vector<std::uint32_t> arguments, std::vector<std::uint32_t> signatureTypes)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::IndirectCall);
        output.result = result;
        output.resultType = I32;
        output.operands.reserve(arguments.size() + 1);
        output.operands.push_back(callable);
        output.operands.insert(output.operands.end(), arguments.begin(), arguments.end());
        output.signatureTypes = std::move(signatureTypes);
        return output;
    }

    bytecode::Module makeModule()
    {
        bytecode::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.constants = {
            bytecode::Constant{},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 1},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 5},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 10},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 40},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 2},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 9},
        };
        module.types = {
            bytecode::Type{.kind = 1},
            bytecode::Type{.kind = 5},
            bytecode::Type{.kind = 29, .arguments = {I32}, .flags = 0x01u},
            bytecode::Type{.kind = 33, .arguments = {I32, I32}},
            bytecode::Type{.kind = 33, .arguments = {I32}},
            bytecode::Type{.kind = 28,
                           .nominalKind = 2,
                           .ownership = 2,
                           .cleanup = 2,
                           .fields = {bytecode::Type::Field{.type = I32}}},
            bytecode::Type{.kind = 29, .arguments = {Box}, .flags = 0x01u},
            bytecode::Type{.kind = 33, .arguments = {I32, Adder}},
            bytecode::Type{.kind = 33, .arguments = {RefBox, Reader}},
        };

        module.functions.push_back(
            function(0, I32, Adder, {parameter(0, I32)}, {constant(1, 1), binaryAdd(2, 0, 1), returnValue(2)}));

        bytecode::Instruction loadCapture = instruction(bytecode::Opcode::Load);
        loadCapture.result = 2;
        loadCapture.resultType = I32;
        loadCapture.operands = {0};
        bytecode::Function capturedAdder = function(1, I32, Adder, {parameter(0, I32), parameter(1, I32)},
                                                    {std::move(loadCapture), binaryAdd(3, 2, 1), returnValue(3)});
        capturedAdder.captureParameterCount = 1;
        capturedAdder.captures = {bytecode::Function::Capture{.type = I32, .kind = 0}};
        module.functions.push_back(std::move(capturedAdder));

        module.functions.push_back(
            function(2, Adder, AdderFactory, {parameter(0, I32)}, {closure(1, Adder, 1, 0, I32, 0), returnValue(1)}));

        bytecode::Instruction reference = instruction(bytecode::Opcode::FunctionReference);
        reference.result = 1;
        reference.resultType = Adder;
        reference.callee = 0;
        module.functions.push_back(
            function(3, I32, Reader, {},
                     {constant(0, 2), std::move(reference), indirectCall(2, 1, {0}, {Adder, I32}), returnValue(2)}));

        bytecode::Instruction makeAdderCall = instruction(bytecode::Opcode::Call);
        makeAdderCall.result = 1;
        makeAdderCall.resultType = Adder;
        makeAdderCall.operands = {0};
        makeAdderCall.callee = 2;
        module.functions.push_back(function(4, I32, Reader, {},
                                            {constant(0, 4), std::move(makeAdderCall), constant(2, 5),
                                             indirectCall(3, 1, {2}, {Adder, I32}), returnValue(3)}));

        bytecode::Instruction selfField = instruction(bytecode::Opcode::FieldPlace);
        selfField.result = 1;
        selfField.resultType = RefI32;
        selfField.operands = {0};
        selfField.targetType = Box;
        selfField.projectionIndex = 0;
        bytecode::Instruction loadSelfField = instruction(bytecode::Opcode::Load);
        loadSelfField.result = 2;
        loadSelfField.resultType = I32;
        loadSelfField.operands = {1};
        bytecode::Function readSelf = function(5, I32, Reader, {parameter(0, RefBox)},
                                               {std::move(selfField), std::move(loadSelfField), returnValue(2)});
        readSelf.captureParameterCount = 1;
        readSelf.captures = {bytecode::Function::Capture{.type = RefBox, .kind = 2}};
        module.functions.push_back(std::move(readSelf));

        module.functions.push_back(function(6, Reader, ReaderFactory, {parameter(0, RefBox)},
                                            {closure(1, Reader, 5, 0, RefBox, 2), returnValue(1)}));

        bytecode::Instruction construct = instruction(bytecode::Opcode::ConstructObject);
        construct.result = 0;
        construct.resultType = Box;
        bytecode::Instruction boxField = instruction(bytecode::Opcode::FieldPlace);
        boxField.result = 2;
        boxField.resultType = RefI32;
        boxField.operands = {0};
        boxField.targetType = Box;
        boxField.projectionIndex = 0;
        bytecode::Instruction store = instruction(bytecode::Opcode::Store);
        store.operands = {2, 1};
        bytecode::Instruction makeReaderCall = instruction(bytecode::Opcode::MethodCall);
        makeReaderCall.result = 3;
        makeReaderCall.resultType = Reader;
        makeReaderCall.operands = {0};
        makeReaderCall.callee = 6;
        bytecode::Instruction release = instruction(bytecode::Opcode::Release);
        release.operands = {0};
        module.functions.push_back(function(7, I32, Reader, {},
                                            {std::move(construct), constant(1, 6), std::move(boxField),
                                             std::move(store), std::move(makeReaderCall), std::move(release),
                                             indirectCall(4, 3, {}, {Reader}), returnValue(4)}));
        return module;
    }

    bool verifySparseFunctionIdRemapping()
    {
        namespace typed = wio::wir::typed;
        using namespace wio::wir;

        typed::Module source;
        source.name = "sparse-function-ids";
        source.contract.logicalName = source.name;
        source.contract.stableKey = "program:" + source.name;
        source.contract.stableId = stableModuleHash(source.contract.stableKey);
        source.contract.callTable.stableId = stableModuleHash(source.contract.stableKey + ":sdk-call-table:v" +
                                                              std::to_string(ModuleAbiDescriptorVersion));
        const TypeId callable =
            source.types.intern(Type{.kind = TypeKind::Function, .arguments = {source.types.i32Type()}});

        typed::Function target;
        target.id = FunctionId{0};
        target.name = "Target";
        target.returnType = source.types.i32Type();
        target.callableType = callable;
        typed::BasicBlock targetEntry;
        targetEntry.id = BlockId{0};
        targetEntry.instructions = {
            typed::Instruction{.opcode = typed::Opcode::Constant,
                               .result = ValueId{0},
                               .resultType = source.types.i32Type(),
                               .literal = std::int64_t{7}},
            typed::Instruction{.opcode = typed::Opcode::Return, .operands = {ValueId{0}}},
        };
        target.blocks.push_back(std::move(targetEntry));
        source.functions.push_back(std::move(target));

        typed::Function caller;
        caller.id = FunctionId{1};
        caller.name = "Caller";
        caller.returnType = source.types.i32Type();
        caller.callableType = callable;
        typed::BasicBlock callerEntry;
        callerEntry.id = BlockId{0};
        callerEntry.instructions = {
            typed::Instruction{.opcode = typed::Opcode::Call,
                               .result = ValueId{0},
                               .resultType = source.types.i32Type(),
                               .callee = FunctionId{0}},
            typed::Instruction{.opcode = typed::Opcode::Return, .operands = {ValueId{0}}},
        };
        caller.blocks.push_back(std::move(callerEntry));
        source.functions.push_back(std::move(caller));

        LoweringResult lowering = LoweringPipeline{}.lower(source);
        if (!lowering.succeeded())
        {
            for (const LoweringDiagnostic& diagnostic : lowering.diagnostics())
                std::cerr << diagnostic.pass << ": " << diagnostic.message << '\n';
            return false;
        }
        wio::wir::lowered::Module sparse = lowering.module();
        sparse.functions[0].id = FunctionId{7};
        sparse.functions[1].id = FunctionId{19};
        sparse.functions[1].blocks[0].instructions[0].callee = FunctionId{7};

        const bytecode::CompileResult compilation = bytecode::Compiler{}.compile(sparse);
        if (!compilation.succeeded())
        {
            for (const bytecode::CompileDiagnostic& diagnostic : compilation.diagnostics())
                std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
            return false;
        }
        if (compilation.module().functions.size() != 2)
        {
            std::cerr << "remapped function count=" << compilation.module().functions.size() << '\n';
            return false;
        }
        const bytecode::Module& compiled = compilation.module();
        const bytecode::VerificationResult verification = bytecode::Verifier{}.verify(compiled);
        const bool valid = compiled.functions[0].id == 0 && compiled.functions[1].id == 1 &&
                           compiled.functions[1].blocks[0].instructions[0].callee == 0 && verification.succeeded();
        if (!valid)
        {
            std::cerr << "remapped ids=" << compiled.functions[0].id << ',' << compiled.functions[1].id
                      << " callee=" << compiled.functions[1].blocks[0].instructions[0].callee << '\n';
            for (const bytecode::VerificationDiagnostic& diagnostic : verification.diagnostics())
                std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        }
        return valid;
    }
} // namespace

int main(const int argc, const char* const* argv)
{
    bool ok = true;
    ok &= expect(verifySparseFunctionIdRemapping(),
                 "Bytecode compilation must densely remap optimized WIR function identities");
    const bytecode::DecodeResult decoded = bytecode::decode(bytecode::encode(makeModule()));
    ok &= expect(decoded.succeeded(), "Callable VM fixture must survive the real .wiob codec and verifier");
    if (!decoded.succeeded())
    {
        std::cerr << decoded.error << '\n';
        return 1;
    }

    vm::Machine machine{decoded.module};
    const vm::ExecutionResult named = machine.invoke(3);
    ok &= expect(named.succeeded() && named.value().asSignedInteger() == 6,
                 "A named function value must execute through IndirectCall");

    const vm::ExecutionResult escaped = machine.invoke(4);
    ok &= expect(escaped.succeeded() && escaped.value().asSignedInteger() == 42,
                 "A value-capturing closure must outlive its creator frame");

    const vm::ExecutionResult retainedSelf = machine.invoke(7);
    ok &= expect(retainedSelf.succeeded() && retainedSelf.value().asSignedInteger() == 9,
                 "A retained-self closure must keep its object alive after the original release");

    bytecode::Module invalid = makeModule();
    bytecode::Instruction& badCall = invalid.functions[3].blocks[0].instructions[2];
    badCall.operands = {0, 0};
    badCall.signatureTypes = {I32, I32};
    const bytecode::VerificationResult verification = bytecode::Verifier{}.verify(invalid);
    ok &= expect(std::ranges::any_of(verification.diagnostics(), [](const bytecode::VerificationDiagnostic& diagnostic)
                                     { return diagnostic.code == "WBC1051"; }),
                 "The bytecode verifier must reject a non-callable indirect target with WBC1051");

    if (argc == 2)
    {
        std::ifstream stream{argv[1], std::ios::binary};
        const std::vector<char> raw{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
        std::vector<std::byte> bytes;
        bytes.reserve(raw.size());
        for (const char value : raw)
            bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
        const bytecode::DecodeResult realSource = bytecode::decode(bytes);
        ok &= expect(realSource.succeeded(), "Real-source callable bytecode must decode");
        if (realSource.succeeded())
        {
            const auto entry = std::ranges::find_if(realSource.module.functions,
                                                    [&](const bytecode::Function& function)
                                                    {
                                                        return function.name < realSource.module.strings.size() &&
                                                               realSource.module.strings[function.name] == "Entry";
                                                    });
            ok &= expect(entry != realSource.module.functions.end(), "Real-source bytecode must contain Entry");
            if (entry != realSource.module.functions.end())
            {
                vm::Machine realMachine{realSource.module};
                const vm::ExecutionResult result = realMachine.invoke(entry->id);
                if (!result.succeeded())
                    std::cerr << result.error().code << ": " << result.error().message << " (function "
                              << result.error().function << ", block " << result.error().block << ", instruction "
                              << result.error().instruction << ")\n";
                ok &= expect(result.succeeded() && result.value().asSignedInteger() == 36,
                             "Compiler-produced function references and closures must execute in the VM");
            }
        }
    }
    return ok ? 0 : 1;
}
