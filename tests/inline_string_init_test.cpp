#include "wir_inline_place.h"

#include <iostream>

namespace
{
    using wio::wir_backend::InlinePlace;

    template <class T, class Factory>
    concept Initializable =
        requires(InlinePlace<T>& cell, Factory&& factory) { cell.initializeFrom(std::forward<Factory>(factory)); };

    struct StringReference
    {
        std::string& operator()() const;
    };
    struct StringValue
    {
        std::string operator()() &&;
    };
    struct IntegerValue
    {
        int operator()() const;
    };
    static_assert(Initializable<std::string, StringValue>);
    static_assert(!Initializable<std::string, StringReference>);
    static_assert(!Initializable<int, IntegerValue>);

    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
} // namespace

int main()
{
    try
    {
        InlinePlace<std::string> cell;
        for (const auto& value : {std::string{}, std::string{"short"}, std::string(4096, 'x'), std::string{"a\0b", 3}})
        {
            cell.initializeFrom([&]() -> std::string { return value; });
            expect(cell.read() == value, "direct initialization changed bytes");
            bool invoked = false, rejected = false;
            try
            {
                cell.initializeFrom(
                    [&]() -> std::string
                    {
                        invoked = true;
                        return "replacement";
                    });
            }
            catch (const std::logic_error&)
            {
                rejected = true;
            }
            expect(rejected && !invoked && cell.read() == value, "live cell overwritten or factory invoked");
            auto moved = std::move(cell);
            expect(moved.take() == value, "move/take changed initialized bytes");
            cell.clear();
        }
        bool threw = false;
        try
        {
            cell.initializeFrom([]() -> std::string { throw std::bad_alloc{}; });
        }
        catch (const std::bad_alloc&)
        {
            threw = true;
        }
        expect(threw, "factory failure swallowed");
        bool empty = false;
        try
        {
            (void)cell.read();
        }
        catch (const std::runtime_error&)
        {
            empty = true;
        }
        expect(empty, "failed factory marked the cell initialized");
        cell.clear();
        cell.initializeFrom([]() -> std::string { return "retry"; });
        expect(cell.take() == "retry", "retry after exception failed");
        cell.write("unchanged write");
        cell.write("replacement");
        expect(cell.read() == "replacement", "ordinary write changed");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
