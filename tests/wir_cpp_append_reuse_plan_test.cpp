#include "wio/codegen/wir_cpp_storage_plan.h"

#include <iostream>
#include <stdexcept>

int main()
{
    using namespace wio::wir;
    namespace l = lowered;
    try
    {
        for (const auto kind : {TypeKind::String, TypeKind::Text})
        {
            l::Module module;
            const auto value = module.types.intern(Type{.kind = kind});
            const auto reference =
                module.types.intern(Type{.kind = TypeKind::Reference, .arguments = {value}, .isMutable = true});
            l::Function function;
            function.parameters.push_back({.id = ValueId{10}, .type = value});
            l::BasicBlock block;
            block.instructions = {{.opcode = l::Opcode::LocalPlace, .result = ValueId{1}, .resultType = reference},
                                  {.opcode = l::Opcode::Load,
                                   .result = ValueId{2},
                                   .resultType = value,
                                   .operands = {ValueId{1}},
                                   .resultOwnership = typed::ValueOwnership::Borrowed},
                                  {.opcode = l::Opcode::Binary,
                                   .result = ValueId{3},
                                   .resultType = value,
                                   .operands = {ValueId{2}, ValueId{10}},
                                   .binaryOperator = typed::BinaryOperator::Add,
                                   .resultOwnership = typed::ValueOwnership::Owned},
                                  {.opcode = l::Opcode::Replace, .operands = {ValueId{1}, ValueId{3}}},
                                  {.opcode = l::Opcode::Return}};
            function.blocks.push_back(block);
            const auto qualifies = [&](const l::Function& candidate)
            { return wio::codegen::WirCppStoragePlan::build(module, candidate).localAppendSuffixes.contains(3); };
            if (!qualifies(function))
                throw std::runtime_error("local append was not selected");
            const auto rejects = [&](l::Function candidate)
            {
                if (qualifies(candidate))
                    throw std::runtime_error("unsafe append fusion was selected");
            };
            auto candidate = function;
            candidate.blocks[0].instructions[3].opcode = l::Opcode::Store;
            if (!qualifies(candidate))
                throw std::runtime_error("local store append was not selected");
            candidate = function;
            candidate.blocks[0].instructions[0].resultType =
                module.types.intern(Type{.kind = TypeKind::Reference, .arguments = {value}, .isMutable = false});
            rejects(candidate);
            candidate = function;
            candidate.isAsync = true;
            rejects(candidate);
            for (const auto opcode : {l::Opcode::FieldPlace, l::Opcode::GlobalPlace, l::Opcode::Borrow})
            {
                candidate = function;
                candidate.blocks[0].instructions[0].opcode = opcode;
                rejects(candidate);
            }
            candidate = function;
            candidate.blocks[0].instructions[3].operands[0] = ValueId{9};
            rejects(candidate);
            candidate = function;
            candidate.blocks[0].instructions.back().operands = {ValueId{3}};
            rejects(candidate);
            candidate = function;
            candidate.blocks[0].instructions[2].operands[1] = ValueId{2};
            rejects(candidate);
            for (const auto position : {2, 3})
            {
                candidate = function;
                candidate.blocks[0].instructions.insert(candidate.blocks[0].instructions.begin() + position,
                                                        {.opcode = l::Opcode::Call});
                rejects(candidate);
            }
            candidate = function;
            candidate.blocks[0].instructions[1].resultOwnership = typed::ValueOwnership::Owned;
            rejects(candidate);
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
