#include "wio/codegen/wir_cpp_backend.h"
#include "wio/codegen/wir_cpp_scalar_format_plan.h"

#include <iostream>
#include <stdexcept>

namespace
{
    using namespace wio::wir;
    using namespace wio::wir::lowered;
    using Plan = wio::codegen::WirCppScalarFormatPlan;

    Module fixture()
    {
        Module module;
        const auto string = module.types.stringType();
        const auto integer = module.types.intern({.kind = TypeKind::I64});
        Function function{.id = FunctionId{0}, .name = "Render", .returnType = string};
        function.callableType = module.types.intern({.kind = TypeKind::Function, .arguments = {string}});
        BasicBlock block{.id = BlockId{0}};
        block.instructions = {
            {.opcode = Opcode::Constant, .result = ValueId{0}, .resultType = integer, .literal = std::int64_t{1234567}},
            {.opcode = Opcode::Interpolate,
             .result = ValueId{1},
             .resultType = string,
             .operands = {ValueId{0}},
             .signatureTypes = {integer},
             .stringSegments = {"", ""},
             .resultOwnership = typed::ValueOwnership::Owned},
            {.opcode = Opcode::Interpolate,
             .result = ValueId{2},
             .resultType = string,
             .operands = {ValueId{1}},
             .signatureTypes = {string},
             .stringSegments = {"<", ">"},
             .resultOwnership = typed::ValueOwnership::Owned},
            {.opcode = Opcode::DropValue, .operands = {ValueId{1}}},
            {.opcode = Opcode::Return, .operands = {ValueId{2}}}};
        function.blocks.push_back(std::move(block));
        module.functions.push_back(std::move(function));
        return module;
    }

    Plan build(const Module& module)
    {
        return Plan::build(module, module.functions[0]);
    }
    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    template <class Edit> void rejects(Edit edit, const char* message)
    {
        auto module = fixture();
        edit(module);
        expect(build(module).fragments.empty(), message);
    }
} // namespace

int main()
{
    try
    {
        auto module = fixture();
        expect(build(module).fragments.at(1) == Plan::Storage::IntegerBytes, "integer fragment not selected");
        auto boolean = module;
        boolean.functions[0].blocks[0].instructions[0].resultType = boolean.types.boolType();
        boolean.functions[0].blocks[0].instructions[0].literal = true;
        boolean.functions[0].blocks[0].instructions[1].signatureTypes = {boolean.types.boolType()};
        expect(build(boolean).fragments.at(1) == Plan::Storage::BooleanView, "bool view not selected");
        rejects([](auto& m) { m.functions[0].isAsync = true; }, "async changed");
        rejects([](auto& m) { m.functions[0].coroutine = CoroutineLayout{}; }, "coroutine changed");
        rejects([](auto& m)
                { m.functions[0].blocks[0].instructions[1].resultOwnership = typed::ValueOwnership::Borrowed; },
                "borrow changed");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[1].borrowOrigin = ValueId{0}; },
                "derived value changed");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[1].stringSegments = {"x", ""}; },
                "prefix discarded");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[1].signatureTypes = {m.types.boolType()}; },
                "signature mismatch ignored");
        rejects(
            [](auto& m)
            {
                m.functions[0].blocks[0].instructions[2].operands.push_back(ValueId{1});
                m.functions[0].blocks[0].instructions[2].signatureTypes.push_back(m.types.stringType());
                m.functions[0].blocks[0].instructions[2].stringSegments.push_back("");
            },
            "multiple consumer positions accepted");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[2].opcode = Opcode::Call; },
                "escaping use changed");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions.back().borrowOrigin = ValueId{1}; },
                "metadata use ignored");
        rejects(
            [](auto& m)
            {
                m.functions[0].blocks[0].instructions.back().targets = {
                    {.block = BlockId{0}, .arguments = {ValueId{1}}}};
            },
            "phi transport changed");
        rejects(
            [](auto& m)
            {
                auto& c = m.functions[0].blocks[0].instructions;
                std::swap(c[2], c[3]);
            },
            "cleanup before consumer ignored");
        for (const auto kind : {TypeKind::I8, TypeKind::U8, TypeKind::Byte, TypeKind::Char, TypeKind::F64,
                                TypeKind::Text, TypeKind::Reference})
        {
            auto candidate = module;
            const auto type = candidate.types.intern({.kind = kind});
            candidate.functions[0].blocks[0].instructions[0].resultType = type;
            candidate.functions[0].blocks[0].instructions[1].signatureTypes = {type};
            expect(build(candidate).fragments.empty(), "unsupported scalar representation changed");
        }
        // A hook/effect between producer and final assembly is allowed: it must
        // remain between FORMAT and READ, never before a deferred conversion.
        auto across = module;
        auto& code = across.functions[0].blocks[0].instructions;
        auto tail = std::vector<Instruction>(code.begin() + 2, code.end());
        code.resize(2);
        code.push_back({.opcode = Opcode::Jump, .targets = {{.block = BlockId{1}}}});
        across.functions[0].blocks.push_back({.id = BlockId{1}, .instructions = std::move(tail)});
        across.functions[0].blocks[1].instructions.insert(across.functions[0].blocks[1].instructions.begin(),
                                                          {.opcode = Opcode::Call});
        expect(build(across).fragments.size() == 1, "stable bytes across an intervening hook rejected");
        auto misplaced = across;
        misplaced.functions[0].blocks[0].instructions.insert(misplaced.functions[0].blocks[0].instructions.end() - 1,
                                                             {.opcode = Opcode::DropValue, .operands = {ValueId{1}}});
        expect(build(misplaced).fragments.empty(), "cleanup on another path ignored");
        for (const auto* candidate : {&module, &boolean})
        {
            const auto emitted = wio::codegen::WirCppBackend{}.generate(
                *candidate, {.emitMain = false, .emitLineDirectives = false, .verifyInput = false});
            const auto begin = emitted.code().find("std::string _wio_f0() {");
            expect(emitted.succeeded() && begin != std::string::npos, "missing synthetic emitted function");
            const auto body = emitted.code().substr(begin, emitted.code().find("\n}\n", begin) - begin);
            expect(body.find("std::optional<std::string> _v1") == std::string::npos &&
                       body.find("_v1.reset()") == std::string::npos,
                   "source string storage/cleanup retained");
            expect(body.find(candidate == &module ? "_v1.format(_v0);" : "_v1 = std::string_view{") !=
                       std::string::npos,
                   "format did not execute at producer");
            expect(body.find(candidate == &module ? "_formatted.append(_v1.view())" : "_formatted.append(_v1)") !=
                       std::string::npos,
                   "consumer reformatted or copied fragment");
            for (const auto& segments : {std::vector<std::string>{"", ""}, std::vector<std::string>{"", "tail"}})
            {
                auto single = *candidate;
                single.functions[0].blocks[0].instructions[2].stringSegments = segments;
                const auto result = wio::codegen::WirCppBackend{}.generate(
                    single, {.emitMain = false, .emitLineDirectives = false, .verifyInput = false});
                const auto start = result.code().find("std::string _wio_f0() {");
                expect(result.succeeded() && start != std::string::npos, "missing fragment guard fixture");
                const auto content = result.code().substr(start, result.code().find("\n}\n", start) - start);
                expect(content.find("format_single(_v1)") == std::string::npos &&
                           content.find("append_formatted_literal_tail(_formatted, _v1") == std::string::npos,
                       "private integer/bool fragment entered an owning formatter");
                expect(content.find(candidate == &module ? "_formatted.append(_v1.view())"
                                                         : "_formatted.append(_v1)") != std::string::npos,
                       "bare/suffix fragment lost its direct view append");
            }
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
