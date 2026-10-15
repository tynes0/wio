#include "wio/codegen/wir_cpp_storage_plan.h"
#include <integer_format.h>

#include <cstdint>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace
{
    void expect(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    class Grouping final : public std::numpunct<char>
    {
        char do_thousands_sep() const override
        {
            return '_';
        }

        std::string do_grouping() const override
        {
            return "\3";
        }
    };

    void integerAppend()
    {
        const auto original = std::locale();
        try
        {
            for (const auto& locale : {std::locale::classic(), std::locale(std::locale::classic(), new Grouping)})
            {
                std::locale::global(locale);
                std::string value = "prefix\0";
                value.push_back('\0');
                const auto prefix = value;
                value.reserve(8192);
                const auto* storage = value.data();
                std::ostringstream expected;
                expected << (std::numeric_limits<std::int64_t>::min)();
                expected << '/';
                expected << (std::numeric_limits<std::uint64_t>::max)();
                wio::runtime::AppendInteger(value, (std::numeric_limits<std::int64_t>::min)());
                value.push_back('/');
                wio::runtime::AppendInteger(value, (std::numeric_limits<std::uint64_t>::max)());
                expect(value == prefix + expected.str(), "append lost prefix/NUL/sign/extreme/locale behavior");
                expect(value.data() == storage, "integer append discarded reserved storage");
                for (int index = -128; index < 128; ++index)
                {
                    value.clear();
                    wio::runtime::AppendInteger(value, index);
                    expect(value == wio::runtime::StringifyInteger(index), "append and owned formatting differ");
                }
            }
        }
        catch (...)
        {
            std::locale::global(original);
            throw;
        }
        std::locale::global(original);
    }

    void copyBoundaries()
    {
        using namespace wio::wir;
        namespace l = lowered;
        using Plan = wio::codegen::WirCppStoragePlan;
        l::Module module;
        const auto string = module.types.stringType();
        l::Function function;
        l::BasicBlock block;
        block.id = BlockId{0};
        block.instructions = {{.opcode = l::Opcode::Load,
                               .result = ValueId{1},
                               .resultType = string,
                               .operands = {ValueId{0}},
                               .resultOwnership = typed::ValueOwnership::Borrowed},
                              {.opcode = l::Opcode::CopyValue,
                               .result = ValueId{2},
                               .resultType = string,
                               .operands = {ValueId{1}},
                               .resultOwnership = typed::ValueOwnership::Owned},
                              {.opcode = l::Opcode::IntrinsicCall,
                               .operands = {ValueId{3}, ValueId{2}},
                               .selector = "Contains",
                               .intrinsicFamily = IntrinsicFamily::String},
                              {.opcode = l::Opcode::Return}};
        function.blocks.push_back(block);
        const auto qualifies = [&](const l::Function& candidate)
        { return Plan::build(module, candidate).borrowedStringCopies.contains(2); };
        expect(qualifies(function), "readonly string search retained adjacent snapshot");
        auto cfg = function;
        cfg.blocks.push_back({.id = BlockId{1}, .instructions = {{.opcode = l::Opcode::Return}}});
        expect(qualifies(cfg), "another CFG block disabled a local proof");
        for (const auto operation :
             {typed::BinaryOperator::Add, typed::BinaryOperator::Equal, typed::BinaryOperator::NotEqual,
              typed::BinaryOperator::Less, typed::BinaryOperator::LessEqual, typed::BinaryOperator::Greater,
              typed::BinaryOperator::GreaterEqual})
        {
            auto binary = cfg;
            auto& sink = binary.blocks.front().instructions[2];
            sink.opcode = l::Opcode::Binary;
            sink.binaryOperator = operation;
            expect(qualifies(binary), "observing string binary operation retained adjacent snapshot");
        }
        for (const auto selector :
             {"Append",    "Contains", "StartsWith", "EndsWith", "IndexOf", "LastIndexOf", "Split",     "Slice",
              "SliceFrom", "Take",     "Skip",       "Left",     "Right",   "Trim",        "TrimStart", "TrimEnd",
              "Repeat",    "Lines",    "PadLeft",    "PadRight", "Count",   "Empty"})
        {
            auto observed = cfg;
            observed.blocks.front().instructions[2].selector = selector;
            expect(qualifies(observed), "known observer rejected");
        }
        for (const auto selector : {"Insert", "ReplaceInPlace", "Erase", "Clear", "ToLowerInPlace", "Unknown"})
        {
            auto mutating = cfg;
            mutating.blocks.front().instructions[2].selector = selector;
            expect(!qualifies(mutating), "unknown/mutating string operation lost snapshot");
        }
        auto unrelated = cfg;
        unrelated.blocks.front().instructions[2].operands = {ValueId{3}, ValueId{4}};
        unrelated.blocks.back().instructions.insert(unrelated.blocks.back().instructions.begin(),
                                                    {.opcode = l::Opcode::Return, .operands = {ValueId{2}}});
        expect(!qualifies(unrelated), "unrelated adjacent sink elided a later observable copy");
        auto mutation = cfg;
        mutation.blocks.front().instructions.insert(mutation.blocks.front().instructions.begin() + 2,
                                                    {.opcode = l::Opcode::Store, .operands = {ValueId{0}, ValueId{3}}});
        expect(!qualifies(mutation), "copy moved across an intervening mutation");
        for (const auto opcode : {l::Opcode::DropValue, l::Opcode::ClosureCreate, l::Opcode::Call, l::Opcode::Borrow})
        {
            auto observable = cfg;
            observable.blocks.back().instructions.insert(observable.blocks.back().instructions.begin(),
                                                         {.opcode = opcode, .operands = {ValueId{2}}});
            expect(!qualifies(observable), "another CFG use lost its owned snapshot");
        }
        auto edge = cfg;
        edge.blocks.front().instructions.back() = {.opcode = l::Opcode::Jump,
                                                   .targets = {{.block = BlockId{1}, .arguments = {ValueId{2}}}}};
        expect(!qualifies(edge), "phi transfer lost its owned snapshot");
        auto owned = cfg;
        owned.blocks.front().instructions[0].resultOwnership = typed::ValueOwnership::Owned;
        expect(!qualifies(owned), "owned load was treated as borrowed");
        auto async = cfg;
        async.isAsync = true;
        expect(!qualifies(async), "async representation changed");
    }
} // namespace

int main()
{
    try
    {
        integerAppend();
        copyBoundaries();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
