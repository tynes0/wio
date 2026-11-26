#include "wio/codegen/wir_cpp_storage_plan.h"
#include "wio/codegen/wir_cpp_backend.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
} // namespace

int main(int argc, char* argv[])
{
    using namespace wio::wir;
    namespace l = wio::wir::lowered;
    using Plan = wio::codegen::WirCppStoragePlan;
    try
    {
        l::Module module;
        const auto integer = module.types.intern(Type{.kind = TypeKind::I32});
        const auto reference =
            module.types.intern(Type{.kind = TypeKind::Reference, .arguments = {integer}, .isMutable = true});
        l::Function function;
        function.parameters.push_back({.id = ValueId{0}, .type = integer});
        l::BasicBlock block;
        block.id = BlockId{0};
        block.instructions = {
            {.opcode = l::Opcode::LocalPlace,
             .result = ValueId{1},
             .resultType = reference,
             .storageClass = l::StorageClass::Stack,
             .escapeClass = l::EscapeClass::Store},
            {.opcode = l::Opcode::PlaceInit, .operands = {ValueId{1}, ValueId{0}}},
            {.opcode = l::Opcode::Load, .result = ValueId{2}, .resultType = integer, .operands = {ValueId{1}}},
            {.opcode = l::Opcode::Return, .operands = {ValueId{2}}}};
        function.blocks.push_back(block);
        auto plan = Plan::build(module, function);
        expect(plan.directControlFlow && plan.scalarPlaces.contains(1) && plan.plainValues.contains(0) &&
                   plan.plainValues.contains(1) && plan.plainValues.contains(2),
               "closed scalar cell and primitive SSA were not promoted");
        const auto rejects = [&](l::Instruction use)
        {
            auto escaped = function;
            escaped.blocks.front().instructions.insert(escaped.blocks.front().instructions.end() - 1, std::move(use));
            expect(!Plan::build(module, escaped).scalarPlaces.contains(1), "escaping scalar cell was promoted");
        };
        rejects({.opcode = l::Opcode::Borrow, .result = ValueId{3}, .resultType = reference, .operands = {ValueId{1}}});
        rejects({.opcode = l::Opcode::Call, .operands = {ValueId{1}}});
        rejects({.opcode = l::Opcode::Store, .operands = {ValueId{4}, ValueId{1}}});
        rejects({.opcode = l::Opcode::ClosureCreate, .operands = {ValueId{1}}});
        rejects({.opcode = l::Opcode::Jump, .targets = {{.block = BlockId{1}, .arguments = {ValueId{1}}}}});
        rejects({.opcode = l::Opcode::Unary, .operands = {ValueId{2}}, .borrowOrigin = ValueId{1}});
        auto async = function;
        async.isAsync = true;
        plan = Plan::build(module, async);
        expect(!plan.directControlFlow && plan.scalarPlaces.empty() && plan.plainValues.empty(),
               "async frame representation changed");
        auto heap = function;
        heap.blocks.front().instructions.front().storageClass = l::StorageClass::Heap;
        expect(Plan::build(module, heap).scalarPlaces.empty(), "heap-classified storage was promoted");
        auto owned = function;
        owned.blocks.front().instructions.insert(owned.blocks.front().instructions.end() - 1,
                                                 {.opcode = l::Opcode::DropValue, .operands = {ValueId{2}}});
        expect(!Plan::build(module, owned).plainValues.contains(2), "explicit lifetime slot lost its reset path");
        if (argc == 2)
        {
            l::Module phi;
            phi.name = "scalar-phi-parallel";
            phi.contract.logicalName = phi.name;
            phi.contract.stableKey = "program:" + phi.name;
            phi.contract.stableId = stableModuleHash(phi.contract.stableKey);
            phi.contract.callTable.stableId = stableModuleHash(phi.contract.stableKey + ":sdk-call-table:v" +
                                                               std::to_string(ModuleAbiDescriptorVersion));
            const auto scalar = phi.types.i32Type();
            l::Function entry;
            entry.id = FunctionId{0};
            entry.name = "Entry";
            entry.returnType = scalar;
            entry.callableType = phi.types.intern(Type{.kind = TypeKind::Function, .arguments = {scalar}});
            l::BasicBlock start;
            start.id = BlockId{0};
            start.instructions = {
                {.opcode = l::Opcode::Constant, .result = ValueId{0}, .resultType = scalar, .literal = std::int64_t{1}},
                {.opcode = l::Opcode::Constant, .result = ValueId{1}, .resultType = scalar, .literal = std::int64_t{2}},
                {.opcode = l::Opcode::Constant, .result = ValueId{2}, .resultType = scalar, .literal = std::int64_t{0}},
                {.opcode = l::Opcode::Jump,
                 .targets = {{.block = BlockId{1}, .arguments = {ValueId{0}, ValueId{1}, ValueId{0}}}}}};
            l::BasicBlock loop;
            loop.id = BlockId{1};
            loop.parameters = {{.id = ValueId{3}, .type = scalar},
                               {.id = ValueId{4}, .type = scalar},
                               {.id = ValueId{5}, .type = scalar}};
            loop.instructions.push_back({.opcode = l::Opcode::Binary,
                                         .result = ValueId{6},
                                         .resultType = phi.types.boolType(),
                                         .operands = {ValueId{5}, ValueId{2}},
                                         .binaryOperator = typed::BinaryOperator::Greater});
            l::Instruction branch;
            branch.opcode = l::Opcode::CondJump;
            branch.operands = {ValueId{6}};
            branch.targets.push_back({.block = BlockId{1}, .arguments = {ValueId{4}, ValueId{3}, ValueId{2}}});
            branch.targets.push_back({.block = BlockId{2}});
            loop.instructions.push_back(std::move(branch));
            l::BasicBlock finish;
            finish.id = BlockId{2};
            finish.instructions = {{.opcode = l::Opcode::Constant,
                                    .result = ValueId{7},
                                    .resultType = scalar,
                                    .literal = std::int64_t{10}},
                                   {.opcode = l::Opcode::Binary,
                                    .result = ValueId{8},
                                    .resultType = scalar,
                                    .operands = {ValueId{3}, ValueId{7}},
                                    .binaryOperator = typed::BinaryOperator::Multiply},
                                   {.opcode = l::Opcode::Binary,
                                    .result = ValueId{9},
                                    .resultType = scalar,
                                    .operands = {ValueId{8}, ValueId{4}},
                                    .binaryOperator = typed::BinaryOperator::Add},
                                   {.opcode = l::Opcode::Return, .operands = {ValueId{9}}}};
            entry.blocks = {std::move(start), std::move(loop), std::move(finish)};
            phi.functions.push_back(std::move(entry));
            const auto generated = wio::codegen::WirCppBackend{}.generate(phi);
            for (const auto& diagnostic : generated.diagnostics())
                std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
            expect(generated.succeeded(), "parallel phi fixture did not verify/generate");
            const auto path = std::filesystem::u8path(argv[1]);
            std::ofstream output{path, std::ios::binary};
            output << generated.code();
            expect(bool(output), "parallel phi fixture write failed");
        }
        std::cout << "Scalar storage plan preserves escape, borrow, phi and async boundaries\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
