#include "wio/wir/numeric_append_optimizer.h"

#include <iostream>
#include <stdexcept>

namespace
{
    using namespace wio::wir;
    using Opcode = lowered::Opcode;

    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    lowered::Module fixture()
    {
        lowered::Module module;
        const auto string = module.types.stringType();
        const auto reference =
            module.types.intern(Type{.kind = TypeKind::Reference, .arguments = {string}, .isMutable = true});
        lowered::Function function;
        function.id = FunctionId{0};
        function.returnType = module.types.voidType();
        const auto integer = module.types.intern(Type{.kind = TypeKind::I64});
        function.parameters = {{.id = ValueId{1}, .type = reference}, {.id = ValueId{2}, .type = integer}};
        lowered::BasicBlock block;
        block.id = BlockId{0};
        lowered::Instruction format;
        format.opcode = Opcode::Interpolate;
        format.result = ValueId{10};
        format.resultType = string;
        format.resultOwnership = typed::ValueOwnership::Owned;
        format.operands = {ValueId{2}};
        format.signatureTypes = {integer};
        format.intrinsicFamily = IntrinsicFamily::String;
        format.stringSegments = {"", ""};
        lowered::Instruction append;
        append.opcode = Opcode::IntrinsicCall;
        append.operands = {ValueId{1}, ValueId{10}};
        append.signatureTypes = {reference, string};
        append.intrinsicFamily = IntrinsicFamily::String;
        append.targetType = string;
        append.selector = "Append";
        lowered::Instruction cleanup;
        cleanup.opcode = Opcode::DropValue;
        cleanup.operands = {ValueId{10}};
        lowered::Instruction finish;
        finish.opcode = Opcode::Return;
        block.instructions = {format, append, cleanup, finish};
        function.blocks.push_back(std::move(block));
        module.functions.push_back(std::move(function));
        return module;
    }

    template <class Edit> void rejects(Edit edit, const char* message)
    {
        auto module = fixture();
        edit(module, module.functions[0], module.functions[0].blocks[0].instructions);
        expect(NumericAppendOptimizer::optimize(module) == 0, message);
    }
} // namespace

int main()
{
    try
    {
        auto module = fixture();
        expect(NumericAppendOptimizer::optimize(module) == 1, "sole numeric formatting was not fused");
        const auto& code = module.functions[0].blocks[0].instructions;
        expect(code.size() == 2 && code[0].opcode == Opcode::IntrinsicCall && code[0].operands[1] == ValueId{2} &&
                   code[0].signatureTypes[1] == module.functions[0].parameters[1].type,
               "numeric append did not preserve the exact scalar identity or remove cleanup");
        expect(NumericAppendOptimizer::optimize(module) == 0, "numeric fusion is not idempotent");
        rejects([](auto&, auto&, auto& code) { code[0].stringSegments[0] = "prefix"; }, "prefix was discarded");
        rejects([](auto&, auto&, auto& code) { code[0].stringSegments[1] = "suffix"; }, "suffix was discarded");
        rejects([](auto&, auto&, auto& code)
                { code.insert(code.begin() + 1, lowered::Instruction{.opcode = Opcode::Call}); },
                "formatting moved across an effect");
        rejects([](auto&, auto&, auto& code) { code.insert(code.begin() + 2, code[1]); },
                "shared formatting was fused twice");
        rejects([](auto& module, auto&, auto& code) { code[0].signatureTypes[0] = module.types.stringType(); },
                "nonprimitive formatter was elided");
        rejects([](auto&, auto& function, auto&) { function.isAsync = true; }, "async frame was rewritten");
        rejects([](auto&, auto& function, auto&) { function.coroutine.emplace(); }, "coroutine frame was rewritten");
        rejects([](auto&, auto&, auto& code) { code[0].borrowOrigin = ValueId{2}; },
                "borrowed format result was fused");
        rejects([](auto&, auto&, auto& code) { code[0].operands.push_back(ValueId{2}); },
                "multi-value format was fused");
        rejects(
            [](auto&, auto& function, auto& code)
            {
                lowered::BasicBlock other;
                other.id = BlockId{1};
                other.instructions.push_back(code[2]);
                function.blocks.push_back(std::move(other));
            },
            "cross-block cleanup was elided");
        rejects([](auto&, auto&, auto& code) { code[3].targets.push_back({BlockId{1}, {ValueId{10}}}); },
                "branch argument use was ignored");
        rejects(
            [](auto& module, auto&, auto& code)
            {
                code[1].signatureTypes[0] = module.types.intern(
                    Type{.kind = TypeKind::Reference, .arguments = {module.types.stringType()}, .isMutable = false});
            },
            "immutable receiver was rewritten");
        rejects([](auto&, auto&, auto& code) { code[1].selector = "Push"; }, "unrelated intrinsic was rewritten");
        rejects([](auto&, auto&, auto& code) { code.insert(code.begin() + 3, code[2]); },
                "duplicate cleanup was hidden");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
