#include "wio/wir/formatting_call_lowerer.h"
#include "wio/wir/typed_ir_verifier.h"
#include "wio/wir/lowering_pipeline.h"
#include "wio/bytecode/compiler.h"
#include "wio/bytecode/verifier.h"

#include <iostream>
#include <stdexcept>

namespace
{
    using namespace wio::wir;
    using namespace wio::wir::typed;

    void expect(const bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    Module fixture()
    {
        Module module;
        module.name = "formatting-calls";
        module.contract.logicalName = "formatting-calls";
        module.contract.stableKey = "wio.module:formatting-calls";
        module.contract.stableId = stableModuleHash(module.contract.stableKey);
        module.contract.callTable.stableId = stableModuleHash(module.contract.stableKey + ":sdk-call-table:v" +
                                                              std::to_string(ModuleAbiDescriptorVersion));
        const auto string = module.types.stringType();
        const auto object = module.types.internNominal({.kind = TypeKind::Named,
                                                        .name = "Label",
                                                        .nominalKind = NominalKind::Object,
                                                        .ownership = OwnershipModel::ReferenceCounted,
                                                        .cleanup = CleanupKind::ReleaseReference});
        const auto reference = module.types.intern({.kind = TypeKind::Reference,
                                                    .arguments = {object},
                                                    .isMutable = true,
                                                    .ownership = OwnershipModel::Borrowed});
        module.types.getMutable(object).methods.push_back({.name = "ToString",
                                                           .returnType = string,
                                                           .function = FunctionId{1},
                                                           .visibility = FieldVisibility::Public});
        Function entry{.id = FunctionId{0}, .name = "Entry", .returnType = string};
        entry.callableType = module.types.intern({.kind = TypeKind::Function, .arguments = {object, string}});
        entry.parameters = {{.id = ValueId{0}, .type = object, .ownership = ValueOwnership::Owned}};
        BasicBlock body{.id = BlockId{0}};
        body.instructions = {{.opcode = Opcode::Interpolate,
                              .result = ValueId{1},
                              .resultType = string,
                              .operands = {ValueId{0}, ValueId{0}},
                              .signatureTypes = {object, object},
                              .stringSegments = {"prefix", ":", "suffix"},
                              .intrinsicFamily = IntrinsicFamily::String,
                              .resultOwnership = ValueOwnership::Owned},
                             {.opcode = Opcode::Release, .operands = {ValueId{0}}},
                             {.opcode = Opcode::Return, .operands = {ValueId{1}}}};
        entry.blocks.push_back(std::move(body));
        Function method{.id = FunctionId{1},
                        .name = "Label::ToString",
                        .returnType = string,
                        .ownerType = object,
                        .isMethod = true};
        method.callableType = module.types.intern({.kind = TypeKind::Function, .arguments = {string}});
        method.parameters = {{.id = ValueId{0},
                              .type = reference,
                              .ownership = ValueOwnership::Borrowed,
                              .borrowLifetime = BorrowLifetime::Caller}};
        BasicBlock result{.id = BlockId{0}};
        result.instructions = {{.opcode = Opcode::Constant,
                                .result = ValueId{1},
                                .resultType = string,
                                .literal = std::string{"label"},
                                .resultOwnership = ValueOwnership::Owned},
                               {.opcode = Opcode::Return, .operands = {ValueId{1}}}};
        method.blocks.push_back(std::move(result));
        module.functions = {std::move(entry), std::move(method)};
        return module;
    }

    template <class Edit> void rejects(Edit edit, const char* message)
    {
        auto module = fixture();
        edit(module);
        expect(FormattingCallLowerer::lower(module) == 0, message);
        expect(module.functions[0].blocks.size() == 1, "rejected formatting changed the CFG");
    }
} // namespace

int main()
{
    try
    {
        auto module = fixture();
        const auto input = Verifier{}.verify(module);
        for (const auto& diagnostic : input.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        expect(input.succeeded(), "input fixture failed verification");
        expect(FormattingCallLowerer::lower(module) == 1, "scalar hook interpolation was not lowered");
        const auto verified = Verifier{}.verify(module);
        for (const auto& diagnostic : verified.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        expect(verified.succeeded(), "lowered formatting failed verification");
        expect(module.functions[0].blocks.size() == 7, "each scalar hook needs a null/call/merge CFG");
        std::size_t calls = 0, identities = 0, drops = 0;
        for (const auto& block : module.functions[0].blocks)
            for (const auto& instruction : block.instructions)
            {
                calls += instruction.opcode == Opcode::VirtualCall;
                identities += instruction.opcode == Opcode::IdentityEqual;
                drops += instruction.opcode == Opcode::Release;
                if (instruction.opcode == Opcode::Interpolate)
                    for (const auto signature : instruction.signatureTypes)
                        expect(signature == module.types.stringType(), "hook leaked into final interpolation");
            }
        expect(calls == 2 && identities == 2 && drops == 5, "call/null/temporary cleanup contract changed");
        expect(FormattingCallLowerer::lower(module) == 0, "format lowering is not idempotent");
        for (const bool coroutine : {false, true})
        {
            auto async = fixture();
            async.functions[0].isAsync = true;
            if (coroutine)
                async.functions[0].coroutine = CoroutineLayout{};
            expect(FormattingCallLowerer::lower(async) == 1, "sync formatter in async context was not lowered");
            expect(FormattingCallLowerer::lower(async) == 0, "async format lowering is not idempotent");
        }
        rejects([](auto& value) { value.functions[1].isAsync = true; }, "async ToString was invoked synchronously");
        rejects([](auto& value) { value.functions[1].genericParameters.push_back(value.types.i32Type()); },
                "unspecialized formatter was selected");
        rejects(
            [](auto& value)
            {
                const auto parameter = value.types.intern({.kind = TypeKind::GenericParameter, .name = "T"});
                value.types.getMutable(value.functions[1].ownerType).arguments = {parameter};
            },
            "open generic owner was selected through a nongeneric method symbol");
        rejects(
            [](auto& value)
            {
                const auto owner = value.functions[1].ownerType;
                value.types.getMutable(owner).methods[0].visibility = FieldVisibility::Private;
            },
            "private formatter was exposed");
        rejects(
            [](auto& value)
            {
                const auto owner = value.functions[1].ownerType;
                auto duplicate = value.types.get(owner).methods[0];
                value.types.getMutable(owner).methods.push_back(std::move(duplicate));
            },
            "ambiguous formatter was guessed");
        auto container = fixture();
        const auto array = container.types.intern({.kind = TypeKind::Array,
                                                   .arguments = {container.functions[1].ownerType},
                                                   .ownership = OwnershipModel::OwnedValue,
                                                   .cleanup = CleanupKind::DestroyValue});
        container.functions[0].parameters[0].type = array;
        container.functions[0].callableType =
            container.types.intern({.kind = TypeKind::Function, .arguments = {array, container.types.stringType()}});
        container.functions[0].blocks[0].instructions[0].signatureTypes = {array, array};
        expect(FormattingCallLowerer::lower(container) == 2, "container traversal and child hook were not lowered");
        const auto containerVerified = Verifier{}.verify(container);
        for (const auto& diagnostic : containerVerified.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        expect(containerVerified.succeeded(), "container formatting failed verification");
        expect(container.functions.size() == 3, "container helper identity not interned");
        expect(FormattingCallLowerer::lower(container) == 0, "container formatting duplicated helpers");
        auto pod = fixture();
        const auto podType = pod.types.internNominal({.kind = TypeKind::Named,
                                                      .name = "NativePod",
                                                      .nominalKind = NominalKind::Component,
                                                      .nominalRepresentation = NominalRepresentation::NativePod,
                                                      .ownership = OwnershipModel::OwnedValue,
                                                      .nativeBinding = NativeTypeBinding{.cppName = "NativePod"}});
        expect(!requiresCleanup(pod.types.get(podType)) && supportsOwnedCopy(pod.types.get(podType)),
               "trivial native POD copy and cleanup protocols were conflated");
        expect(!supportsOwnedCopy(pod.types.get(pod.types.i32Type())), "primitive ownership protocol widened");
        pod.contract.reflection.push_back({.stableTypeId = stableModuleHash("formatting-calls.NativePod"),
                                           .logicalName = "NativePod",
                                           .type = podType,
                                           .nominalKind = NominalKind::Component});
        pod.functions[0].parameters[0].type = podType;
        pod.functions[0].callableType =
            pod.types.intern({.kind = TypeKind::Function, .arguments = {podType, pod.types.stringType()}});
        pod.functions[0].blocks[0].instructions[0].signatureTypes = {podType, podType};
        pod.functions[0].blocks[0].instructions.erase(pod.functions[0].blocks[0].instructions.begin() + 1);
        expect(FormattingCallLowerer::lower(pod) == 1, "native POD formatter was not materialized");
        const auto lowered = LoweringPipeline{}.lower(pod);
        for (const auto& diagnostic : lowered.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        expect(lowered.succeeded(), "POD owned-copy lowering failed");
        const auto bytecode = wio::bytecode::Compiler{}.compile(lowered.module());
        expect(bytecode.succeeded(), "POD formatter bytecode compilation failed");
        const auto checked = wio::bytecode::Verifier{}.verify(bytecode.module());
        for (const auto& diagnostic : checked.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        expect(checked.succeeded(), "POD owned-copy bytecode protocol rejected");
        for (const int invalidCase : {0, 1, 2, 3})
        {
            auto invalidPod = bytecode.module();
            auto& invalidType = invalidPod.types[podType.value()];
            if (invalidCase == 0)
                invalidType.nativeBinding.triviallyCopyable = false;
            if (invalidCase == 1)
                invalidType.nativeBinding.standardLayout = false;
            if (invalidCase == 2)
                invalidType.nominalKind = 2; // not an object/RC escape hatch
            if (invalidCase == 3)
                invalidType.cleanup = 2; // retain and POD copy remain distinct
            const auto rejectedPod = wio::bytecode::Verifier{}.verify(invalidPod);
            bool rejectedCopy = false;
            for (const auto& diagnostic : rejectedPod.diagnostics())
                rejectedCopy |= diagnostic.code == "WBC1119";
            expect(rejectedCopy, "native copy protocol accepted a forged non-POD ownership contract");
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
