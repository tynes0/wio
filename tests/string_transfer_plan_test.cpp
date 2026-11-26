#include "wio/codegen/wir_cpp_backend.h"
#include "wio/codegen/wir_cpp_string_transfer_plan.h"

#include <iostream>
#include <stdexcept>

namespace
{
    using namespace wio::wir;
    using namespace wio::wir::lowered;
    using Plan = wio::codegen::WirCppStringTransferPlan;

    Module fixture()
    {
        Module module;
        const auto string = module.types.stringType();
        Function function{.id = FunctionId{0}, .name = "Forward", .returnType = string};
        function.callableType = module.types.intern({.kind = TypeKind::Function, .arguments = {string}});
        BasicBlock producer{.id = BlockId{0}};
        producer.instructions = {
            {.opcode = Opcode::Constant,
             .result = ValueId{0},
             .resultType = string,
             .literal = std::string{"a\0b", 3},
             .resultOwnership = typed::ValueOwnership::Owned},
            {.opcode = Opcode::Jump, .targets = {{.block = BlockId{1}, .arguments = {ValueId{0}}}}}};
        BasicBlock destination{.id = BlockId{1}};
        destination.parameters = {{.id = ValueId{1}, .type = string, .ownership = typed::ValueOwnership::Owned}};
        destination.instructions = {{.opcode = Opcode::Return, .operands = {ValueId{1}}}};
        function.blocks = {producer, destination};
        module.functions = {function};
        return module;
    }

    template <class Edit> void rejects(Edit edit, const char* message)
    {
        auto module = fixture();
        edit(module);
        if (!Plan::build(module, module.functions[0]).edgeDestinations.empty())
            throw std::runtime_error(message);
    }
} // namespace

int main()
{
    try
    {
        auto module = fixture();
        const auto plan = Plan::build(module, module.functions[0]);
        if (plan.edgeDestinations.size() != 1 || plan.edgeDestinations.at(0) != ValueId{1} ||
            module.functions[0].blocks[0].instructions.size() != 2)
            throw std::runtime_error("single-use adjacent string was not forwarded or IR was changed");
        for (const auto opcode : {Opcode::Call, Opcode::MethodCall, Opcode::VirtualCall, Opcode::InterfaceCall,
                                  Opcode::ExtensionCall, Opcode::NativeInvoke, Opcode::Interpolate})
        {
            auto candidate = module;
            candidate.functions[0].blocks[0].instructions[0].opcode = opcode;
            if (Plan::build(candidate, candidate.functions[0]).edgeDestinations.empty())
                throw std::runtime_error("fresh string prvalue producer was not supported");
        }
        rejects([](auto& m) { m.functions[0].isAsync = true; }, "async storage changed");
        rejects([](auto& m) { m.functions[0].coroutine = CoroutineLayout{}; }, "coroutine storage changed");
        rejects([](auto& m)
                { m.functions[0].blocks[0].instructions[0].resultOwnership = typed::ValueOwnership::Borrowed; },
                "borrowed source forwarded");
        rejects([](auto& m) { m.functions[0].blocks[1].parameters[0].ownership = typed::ValueOwnership::Borrowed; },
                "borrowed destination forwarded");
        rejects([](auto& m)
                { m.functions[0].blocks[0].instructions[0].resultType = m.types.intern({.kind = TypeKind::Text}); },
                "Text representation changed");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[0].opcode = Opcode::CopyValue; },
                "copy/source alias forwarded");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[0].opcode = Opcode::Load; },
                "load/source alias forwarded");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[0].borrowOrigin = ValueId{2}; },
                "borrow-derived producer forwarded");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions.back().opcode = Opcode::CondJump; },
                "conditional edge forwarded");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions.back().targets[0].block = BlockId{99}; },
                "missing destination forwarded");
        rejects([](auto& m) { m.functions[0].blocks[1].parameters.push_back(m.functions[0].blocks[1].parameters[0]); },
                "parallel phi assignment altered");
        rejects([](auto& m)
                { m.functions[0].blocks[0].instructions.back().targets[0].arguments.push_back(ValueId{0}); },
                "multiple edge arguments forwarded");
        rejects([](auto& m) { m.functions[0].blocks[1].instructions[0].borrowOrigin = ValueId{0}; },
                "metadata use ignored");
        rejects(
            [](auto& m)
            {
                m.functions[0].blocks[1].instructions.insert(m.functions[0].blocks[1].instructions.begin(),
                                                             {.opcode = Opcode::DropValue, .operands = {ValueId{0}}});
            },
            "source cleanup would reset forwarded destination");
        rejects(
            [](auto& m)
            {
                auto& code = m.functions[0].blocks[0].instructions;
                code.insert(code.end() - 1, {.opcode = Opcode::Call});
            },
            "intervening effect crossed edge");
        rejects([](auto& m) { m.functions[0].blocks[1].instructions[0].operands.push_back(ValueId{0}); },
                "additional semantic use ignored");

        // Only this synthetic emitter-shape check bypasses module-contract
        // verification; functional Wio fixtures verify the complete pipeline.
        const auto emitted = wio::codegen::WirCppBackend{}.generate(
            module, {.emitMain = false, .emitLineDirectives = false, .verifyInput = false});
        const auto begin = emitted.code().find("std::string _wio_f0() {");
        const auto body = emitted.code().substr(begin, emitted.code().find("\n}\n", begin) - begin);
        if (!emitted.succeeded() || body.find("_v0") != std::string::npos || body.find("_edge0") != std::string::npos ||
            body.find("_v1 =") == std::string::npos)
            throw std::runtime_error("emitter retained forwarded source/edge storage");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
