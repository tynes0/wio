#include "wio/codegen/wir_cpp_backend.h"
#include "wio/codegen/wir_cpp_string_init_plan.h"

#include <iostream>
#include <stdexcept>

namespace
{
    using namespace wio::wir;
    using namespace wio::wir::lowered;
    using Plan = wio::codegen::WirCppStringInitPlan;

    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    Module fixture()
    {
        Module module;
        const auto string = module.types.stringType();
        const auto reference = module.types.intern(
            {.kind = TypeKind::Reference, .arguments = {string}, .ownership = OwnershipModel::Borrowed});
        Function function{.id = FunctionId{0}, .name = "Initialize", .returnType = module.types.voidType()};
        function.callableType = module.types.intern({.kind = TypeKind::Function, .arguments = {function.returnType}});
        BasicBlock body{.id = BlockId{0}};
        body.instructions = {{.opcode = Opcode::Constant,
                              .result = ValueId{0},
                              .resultType = string,
                              .literal = std::string{"a\0b", 3},
                              .resultOwnership = typed::ValueOwnership::Owned},
                             {.opcode = Opcode::LocalPlace,
                              .result = ValueId{1},
                              .resultType = reference,
                              .storageClass = StorageClass::Stack,
                              .escapeClass = EscapeClass::Local},
                             {.opcode = Opcode::PlaceInit, .operands = {ValueId{1}, ValueId{0}}},
                             {.opcode = Opcode::DropPlace, .operands = {ValueId{1}}},
                             {.opcode = Opcode::Return}};
        function.blocks.push_back(std::move(body));
        module.functions.push_back(std::move(function));
        return module;
    }

    Plan build(const Module& module)
    {
        const auto& function = module.functions[0];
        return Plan::build(module, function, wio::codegen::WirCppStoragePlan::build(module, function));
    }

    template <class Edit> void rejects(Edit edit, const char* message)
    {
        auto module = fixture();
        edit(module);
        expect(build(module).producerCells.empty(), message);
    }
} // namespace

int main()
{
    try
    {
        auto module = fixture();
        const auto plan = build(module);
        expect(plan.producerCells.size() == 1 && plan.producerCells.at(0) == ValueId{1} &&
                   plan.suppressedInstructions.size() == 2,
               "fresh closed cell was not initialized directly");
        for (const auto opcode : {Opcode::Interpolate, Opcode::Call, Opcode::MethodCall, Opcode::ExtensionCall,
                                  Opcode::VirtualCall, Opcode::InterfaceCall, Opcode::NativeInvoke})
        {
            auto candidate = module;
            candidate.functions[0].blocks[0].instructions[0].opcode = opcode;
            expect(!build(candidate).producerCells.empty(), "fresh producer omitted");
        }
        rejects([](auto& m) { m.functions[0].isAsync = true; }, "async cell changed");
        rejects([](auto& m) { m.functions[0].coroutine = CoroutineLayout{}; }, "coroutine cell changed");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[0].opcode = Opcode::CopyValue; }, "copy forwarded");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[0].opcode = Opcode::Load; }, "load forwarded");
        rejects([](auto& m)
                { m.functions[0].blocks[0].instructions[0].resultOwnership = typed::ValueOwnership::Borrowed; },
                "borrow forwarded");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[0].borrowOrigin = ValueId{9}; },
                "borrow origin ignored");
        rejects([](auto& m)
                { m.functions[0].blocks[0].instructions[0].resultType = m.types.intern({.kind = TypeKind::Text}); },
                "text changed");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[1].escapeClass = EscapeClass::Return; },
                "escape changed");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions.back().borrowOrigin = ValueId{0}; },
                "metadata use ignored");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions.back().operands = {ValueId{0}}; },
                "extra use ignored");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions.back().targets = {{.block = BlockId{9}}}; },
                "missing CFG target");
        rejects([](auto& m) { m.functions[0].blocks.resize(513); }, "block proof budget ignored");
        rejects(
            [](auto& m)
            {
                auto& code = m.functions[0].blocks[0].instructions;
                code.insert(code.begin() + 1, 32, Instruction{.opcode = Opcode::DropValue, .operands = {ValueId{9}}});
            },
            "instruction proof budget ignored");
        rejects(
            [](auto& m)
            {
                auto& code = m.functions[0].blocks[0].instructions;
                code.insert(code.begin() + 1, {.opcode = Opcode::Call});
            },
            "intervening call crossed");
        rejects(
            [](auto& m)
            {
                auto& code = m.functions[0].blocks[0].instructions;
                code.insert(code.begin() + 3, {.opcode = Opcode::Call, .operands = {ValueId{1}}});
            },
            "nonclosed cell forwarded");

        // Owned string-only cleanup can remain in place between producer and
        // initialization. Cleanup of other types/resources must be a barrier.
        for (const auto cleanup : {Opcode::DropValue, Opcode::Release})
        {
            auto candidate = module;
            auto& function = candidate.functions[0];
            function.parameters = {
                {.id = ValueId{9}, .type = candidate.types.stringType(), .ownership = typed::ValueOwnership::Owned}};
            function.blocks[0].instructions.insert(function.blocks[0].instructions.begin() + 1,
                                                   {.opcode = cleanup, .operands = {ValueId{9}}});
            expect(build(candidate).producerCells.size() == 1, "owned string cleanup rejected");
            function.parameters[0].ownership = typed::ValueOwnership::Borrowed;
            expect(build(candidate).producerCells.empty(), "borrowed cleanup crossed");
            function.parameters[0].ownership = typed::ValueOwnership::Owned;
            function.parameters[0].type = candidate.types.i32Type();
            expect(build(candidate).producerCells.empty(), "non-string cleanup crossed");
            function.parameters[0].type = candidate.types.stringType();
            function.blocks[0].instructions[1].operands = {ValueId{0}};
            expect(build(candidate).producerCells.empty(), "producer cleanup lost");
        }

        auto loop = module;
        auto& body = loop.functions[0].blocks[0].instructions;
        body.back() = {.opcode = Opcode::Jump, .targets = {{.block = BlockId{0}}}};
        expect(build(loop).producerCells.size() == 1, "cleared loop backedge rejected");
        body.erase(body.end() - 2);
        expect(build(loop).producerCells.empty(), "live loop value destroyed before producer");
        body.insert(body.begin(), {.opcode = Opcode::DropPlace, .operands = {ValueId{1}}});
        expect(build(loop).producerCells.size() == 1, "explicit pre-producer clear rejected");

        // Entry and backedge are joined: one non-clearing path must block the
        // transformation even if another predecessor clears the same cell.
        auto branch = module;
        auto& function = branch.functions[0];
        function.blocks[0].instructions.erase(function.blocks[0].instructions.end() - 2);
        function.blocks[0].instructions.back() = {.opcode = Opcode::CondJump,
                                                  .targets = {{.block = BlockId{1}}, {.block = BlockId{2}}}};
        function.blocks.push_back({.id = BlockId{1},
                                   .instructions = {{.opcode = Opcode::DropPlace, .operands = {ValueId{1}}},
                                                    {.opcode = Opcode::Jump, .targets = {{.block = BlockId{0}}}}}});
        function.blocks.push_back(
            {.id = BlockId{2}, .instructions = {{.opcode = Opcode::Jump, .targets = {{.block = BlockId{0}}}}}});
        expect(build(branch).producerCells.empty(), "may-initialized join was treated as empty");
        function.blocks[2].instructions.insert(function.blocks[2].instructions.begin(),
                                               {.opcode = Opcode::ReleasePlace, .operands = {ValueId{1}}});
        expect(build(branch).producerCells.size() == 1, "all cleared predecessors rejected");
        function.blocks.insert(function.blocks.begin(),
                               {.id = BlockId{9}, .instructions = {{.opcode = Opcode::Return}}});
        expect(build(branch).producerCells.empty(), "unreachable producer forwarded");

        auto storage = wio::codegen::WirCppStoragePlan::build(module, module.functions[0]);
        for (std::uint32_t id = 100; id < 164; ++id)
            storage.stringPlaces.insert(id);
        expect(Plan::build(module, module.functions[0], storage).producerCells.empty(), "cell proof budget ignored");

        // Only this synthetic emitter-shape fixture skips module verification.
        // Wio integration tests exercise the verified end-to-end path.
        const auto emitted = wio::codegen::WirCppBackend{}.generate(
            module, {.emitMain = false, .emitLineDirectives = false, .verifyInput = false});
        const auto begin = emitted.code().find("void _wio_f0() {");
        expect(begin != std::string::npos && emitted.succeeded(), "missing emitted function");
        const auto code = emitted.code().substr(begin, emitted.code().find("\n}\n", begin) - begin);
        expect(code.find("InlinePlace<std::string> _v1{};") != std::string::npos &&
                   code.find("_v1.initializeFrom([&]() -> std::string { return ") != std::string::npos &&
                   code.find("_v0") == std::string::npos && code.find(".write(") == std::string::npos,
               "emitter retained source staging or lost linear cell declaration");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
