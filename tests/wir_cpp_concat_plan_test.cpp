#include "wio/codegen/wir_cpp_storage_plan.h"

#include <iostream>
#include <stdexcept>

namespace
{
    void expect(const bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
} // namespace

int main()
{
    using namespace wio::wir;
    namespace l = lowered;
    using Plan = wio::codegen::WirCppStoragePlan;
    try
    {
        for (const auto kind : {TypeKind::String, TypeKind::Text})
        {
            l::Module module;
            const auto value = module.types.intern(Type{.kind = kind});
            const auto reference =
                module.types.intern(Type{.kind = TypeKind::Reference, .arguments = {value}, .isMutable = true});
            l::Function function;
            l::BasicBlock block;
            block.instructions = {{.opcode = l::Opcode::LocalPlace, .result = ValueId{0}, .resultType = reference},
                                  {.opcode = l::Opcode::LocalPlace, .result = ValueId{1}, .resultType = reference},
                                  {.opcode = l::Opcode::Load,
                                   .result = ValueId{2},
                                   .resultType = value,
                                   .operands = {ValueId{0}},
                                   .resultOwnership = typed::ValueOwnership::Borrowed},
                                  {.opcode = l::Opcode::Load,
                                   .result = ValueId{3},
                                   .resultType = value,
                                   .operands = {ValueId{1}},
                                   .resultOwnership = typed::ValueOwnership::Borrowed},
                                  {.opcode = l::Opcode::Binary,
                                   .result = ValueId{4},
                                   .resultType = value,
                                   .operands = {ValueId{2}, ValueId{3}},
                                   .binaryOperator = typed::BinaryOperator::Add,
                                   .resultOwnership = typed::ValueOwnership::Owned},
                                  {.opcode = l::Opcode::Load,
                                   .result = ValueId{5},
                                   .resultType = value,
                                   .operands = {ValueId{0}},
                                   .resultOwnership = typed::ValueOwnership::Borrowed},
                                  {.opcode = l::Opcode::Binary,
                                   .result = ValueId{6},
                                   .resultType = value,
                                   .operands = {ValueId{4}, ValueId{5}},
                                   .binaryOperator = typed::BinaryOperator::Add,
                                   .resultOwnership = typed::ValueOwnership::Owned},
                                  {.opcode = l::Opcode::DropValue, .operands = {ValueId{4}}},
                                  {.opcode = l::Opcode::Replace, .operands = {ValueId{0}, ValueId{6}}},
                                  {.opcode = l::Opcode::Return}};
            // Intermediate cleanup may separate the root from its store.
            function.blocks.push_back(block);
            auto plan = Plan::build(module, function);
            expect(plan.localAppendSuffixes.at(6) == std::vector<ValueId>{ValueId{3}, ValueId{5}},
                   "concat suffix order/alias was lost");
            expect(plan.elidedConcatValues.contains(4) && plan.elidedConcatValues.contains(6),
                   "chain cleanup was not elided");
            const auto rejects = [&](const l::Function& candidate)
            { expect(Plan::build(module, candidate).localAppendSuffixes.empty(), "unsafe concat selected"); };
            auto candidate = function;
            candidate.isAsync = true;
            rejects(candidate);
            for (const auto opcode :
                 {l::Opcode::Call, l::Opcode::NativeInvoke, l::Opcode::Borrow, l::Opcode::Store, l::Opcode::ArrayPlace})
            {
                candidate = function;
                candidate.blocks[0].instructions.insert(candidate.blocks[0].instructions.begin() + 5,
                                                        {.opcode = opcode});
                rejects(candidate);
            }

            candidate = function;
            candidate.blocks[0].instructions.back().operands = {ValueId{4}};
            rejects(candidate);
            candidate = function;
            candidate.blocks[0].instructions.back().borrowOrigin = ValueId{4};
            rejects(candidate);
            candidate = function;
            candidate.blocks[0].instructions.back().targets.push_back({.arguments = {ValueId{4}}});
            rejects(candidate);
            candidate = function;
            candidate.blocks[0].instructions[1].opcode = l::Opcode::Borrow;
            rejects(candidate);
            candidate = function;
            candidate.blocks[0].instructions[0].opcode = l::Opcode::ArrayPlace;
            rejects(candidate);

            // A pure standalone tree reserves once and removes interiors.
            candidate = function;
            candidate.blocks[0].instructions[8].opcode = l::Opcode::Return;
            candidate.blocks[0].instructions[8].operands = {ValueId{6}};
            plan = Plan::build(module, candidate);
            expect(plan.localAppendSuffixes.empty() && plan.flattenedConcatOperands.contains(6) &&
                       plan.elidedConcatValues.contains(4),
                   "pure concat tree was not flattened");
            // A call is a flattening barrier but cannot observe the independent
            // owned intermediate when it receives no reference to that value.
            candidate.blocks[0].instructions.insert(candidate.blocks[0].instructions.begin() + 5,
                                                    {.opcode = l::Opcode::Call});
            plan = Plan::build(module, candidate);
            expect(plan.flattenedConcatOperands.empty() && plan.consumedConcatOperands.contains(4),
                   "last-use concat buffer was not consumed");
            expect(!plan.consumedConcatOperands.contains(2) && !plan.consumedConcatOperands.contains(3),
                   "borrowed storage was consumed");
            candidate.blocks[0].instructions.back().operands = {ValueId{4}};
            expect(!Plan::build(module, candidate).consumedConcatOperands.contains(4),
                   "multiply observed owned buffer was consumed");

            // Same-place direct field/global += is safe; parameter/proxy and
            // array-place references deliberately retain the ordinary path.
            candidate = function;
            candidate.blocks[0].instructions.erase(candidate.blocks[0].instructions.begin() + 5,
                                                   candidate.blocks[0].instructions.begin() + 8);
            candidate.blocks[0].instructions[5].operands = {ValueId{0}, ValueId{4}};
            candidate.blocks[0].instructions[0].opcode = l::Opcode::FieldPlace;
            candidate.blocks[0].instructions[0].operands = {ValueId{20}};
            expect(Plan::build(module, candidate).localAppendSuffixes.contains(4), "direct field append rejected");
            candidate.blocks[0].instructions[0].opcode = l::Opcode::GlobalPlace;
            candidate.blocks[0].instructions[0].global = GlobalId{1};
            expect(Plan::build(module, candidate).localAppendSuffixes.contains(4), "direct global append rejected");
            candidate.blocks[0].instructions[0].opcode = l::Opcode::Borrow;
            rejects(candidate);

            candidate = function;
            candidate.blocks[0].instructions[3].resultType = module.types.boolType();
            plan = Plan::build(module, candidate);
            expect(plan.localAppendSuffixes.empty() && plan.flattenedConcatOperands.empty(),
                   "mixed operand types entered concat helpers");
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
