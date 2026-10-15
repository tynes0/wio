#include "wio/codegen/wir_cpp_storage_plan.h"
#include <wir_inline_place.h>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{
    void expect(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    struct Deleter
    {
        static inline int released = 0;

        void operator()(int* value) const noexcept
        {
            ++released;
            delete value;
        }
    };

    struct ThrowingValue
    {
        static inline int live = 0;
        static inline bool fail = false;

        ThrowingValue()
        {
            ++live;
        }

        ThrowingValue(ThrowingValue&&)
        {
            if (fail)
                throw std::runtime_error("construction failure");
            ++live;
        }

        ThrowingValue& operator=(ThrowingValue&&)
        {
            if (fail)
                throw std::runtime_error("assignment failure");
            return *this;
        }

        ~ThrowingValue()
        {
            --live;
        }
    };

    void storageLifetime()
    {
        using wio::wir_backend::InlinePlace;
        static_assert(!std::is_copy_constructible_v<InlinePlace<std::string>>);
        static_assert(std::is_nothrow_move_constructible_v<InlinePlace<std::string>>);
        auto cell = InlinePlace<std::string>::local();
        bool rejected = false;
        try
        {
            (void)cell.read();
        }
        catch (const std::runtime_error&)
        {
            rejected = true;
        }
        expect(rejected, "uninitialized inline cell read succeeded");
        cell.write("before");
        const auto snapshot = cell.read();
        cell.write(std::string(2048, 'x'));
        expect(snapshot == "before", "owned snapshot changed after cell replacement");
        cell.clear();
        cell.clear();
        rejected = false;
        try
        {
            (void)std::as_const(cell).read();
        }
        catch (const std::runtime_error&)
        {
            rejected = true;
        }
        expect(rejected, "cleared inline cell read succeeded");
        cell.write("reinitialized");
        auto moved = std::move(cell);
        expect(moved.read() == "reinitialized", "cell move lost its value");
        moved = std::move(moved);
        expect(moved.read() == "reinitialized", "self move destroyed the cell");
        expect(moved.take() == "reinitialized", "move out lost its value");
        moved.clear();
        using Token = std::unique_ptr<int, Deleter>;
        {
            auto first = InlinePlace<Token>::local();
            first.write(Token{new int(1)});
            auto second = std::move(first);
            first.clear();
            expect(Deleter::released == 0, "moving the cell released its resource");
            second.clear();
            second.clear();
            expect(Deleter::released == 1, "resource was not released exactly once");
            second.write(Token{new int(2)});
            throw std::runtime_error("scope unwind");
        }
    }

    void constructionFailure()
    {
        auto cell = wio::wir_backend::InlinePlace<ThrowingValue>::local();
        ThrowingValue::fail = true;
        try
        {
            cell.write(ThrowingValue{});
        }
        catch (const std::runtime_error&)
        {
        }
        expect(ThrowingValue::live == 0, "failed construction left an active value");
        ThrowingValue::fail = false;
        cell.write(ThrowingValue{});
        expect(ThrowingValue::live == 1, "successful construction lifetime is wrong");
        ThrowingValue::fail = true;
        try
        {
            cell.write(ThrowingValue{});
        }
        catch (const std::runtime_error&)
        {
        }
        expect(ThrowingValue::live == 1, "failed assignment lost the active value");
        cell.clear();
        expect(ThrowingValue::live == 0, "failed assignment value was not destroyed");
        ThrowingValue::fail = false;
    }

    void planBoundaries()
    {
        using namespace wio::wir;
        namespace l = lowered;
        l::Module module;
        const auto string = module.types.stringType();
        const auto reference =
            module.types.intern(Type{.kind = TypeKind::Reference, .arguments = {string}, .isMutable = true});
        l::Function function;
        l::BasicBlock block;
        block.id = BlockId{0};
        block.instructions = {{.opcode = l::Opcode::LocalPlace,
                               .result = ValueId{1},
                               .resultType = reference,
                               .storageClass = l::StorageClass::Stack,
                               .escapeClass = l::EscapeClass::Store},
                              {.opcode = l::Opcode::PlaceInit, .operands = {ValueId{1}, ValueId{0}}},
                              {.opcode = l::Opcode::Load,
                               .result = ValueId{2},
                               .resultType = string,
                               .operands = {ValueId{1}},
                               .resultOwnership = typed::ValueOwnership::Borrowed,
                               .borrowOrigin = ValueId{1}},
                              {.opcode = l::Opcode::CopyValue,
                               .result = ValueId{3},
                               .resultType = string,
                               .operands = {ValueId{2}},
                               .resultOwnership = typed::ValueOwnership::Owned},
                              {.opcode = l::Opcode::IntrinsicCall,
                               .operands = {ValueId{4}, ValueId{3}},
                               .selector = "Append",
                               .intrinsicFamily = IntrinsicFamily::String},
                              {.opcode = l::Opcode::DropPlace, .operands = {ValueId{1}}},
                              {.opcode = l::Opcode::Return}};
        function.blocks.push_back(block);
        using Plan = wio::codegen::WirCppStoragePlan;
        auto plan = Plan::build(module, function);
        expect(plan.stringPlaces.contains(1) && plan.plainValues.contains(1), "closed string cell was not promoted");
        expect(plan.borrowedAppendCopies.contains(3), "adjacent append-only snapshot was not elided");
        auto movedCell = function;
        auto& move = movedCell.blocks.front().instructions[2];
        move.opcode = l::Opcode::MoveValue;
        move.resultOwnership = typed::ValueOwnership::Owned;
        move.borrowOrigin = {};
        expect(Plan::build(module, movedCell).stringPlaces.contains(1), "owned move out prevented cell promotion");
        auto heapCell = function;
        heapCell.blocks.front().instructions.front().storageClass = l::StorageClass::Heap;
        expect(Plan::build(module, heapCell).stringPlaces.empty(), "heap string cell was promoted");
        const auto rejectCell = [&](l::Instruction use)
        {
            auto escaped = function;
            escaped.blocks.front().instructions.insert(escaped.blocks.front().instructions.begin() + 2, std::move(use));
            expect(!Plan::build(module, escaped).stringPlaces.contains(1), "escaping string cell was promoted");
        };
        rejectCell({.opcode = l::Opcode::Borrow, .operands = {ValueId{1}}});
        rejectCell({.opcode = l::Opcode::Call, .operands = {ValueId{1}}});
        rejectCell({.opcode = l::Opcode::ClosureCreate, .operands = {ValueId{1}}});
        rejectCell({.opcode = l::Opcode::Store, .operands = {ValueId{4}, ValueId{1}}});
        rejectCell({.opcode = l::Opcode::Jump, .targets = {{.block = BlockId{1}, .arguments = {ValueId{1}}}}});
        rejectCell({.opcode = l::Opcode::Unary, .borrowOrigin = ValueId{1}});
        auto async = function;
        async.isAsync = true;
        expect(Plan::build(module, async).stringPlaces.empty(), "async string frame changed");
        const auto rejectCopy = [&](l::Function changed)
        { expect(Plan::build(module, changed).borrowedAppendCopies.empty(), "observable string copy was removed"); };
        auto changed = function;
        changed.blocks.front().instructions[4].selector = "Insert";
        rejectCopy(changed);
        changed = function;
        changed.blocks.front().instructions[4].opcode = l::Opcode::Call;
        rejectCopy(changed);
        changed = function;
        changed.blocks.front().instructions[2].resultOwnership = typed::ValueOwnership::Owned;
        rejectCopy(changed);
        changed = function;
        changed.blocks.front().instructions.insert(changed.blocks.front().instructions.end() - 1,
                                                   {.opcode = l::Opcode::DropValue, .operands = {ValueId{3}}});
        rejectCopy(changed);
        changed = function;
        changed.blocks.front().instructions.insert(changed.blocks.front().instructions.begin() + 4,
                                                   {.opcode = l::Opcode::Store, .operands = {ValueId{4}, ValueId{0}}});
        rejectCopy(changed);
    }
} // namespace

int main()
{
    try
    {
        try
        {
            storageLifetime();
        }
        catch (const std::runtime_error& error)
        {
            expect(std::string_view{error.what()} == "scope unwind", error.what());
        }
        expect(Deleter::released == 2, "unwind did not release the second resource exactly once");
        constructionFailure();
        planBoundaries();
        std::cout << "managed-storage-plan-lifetime-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
