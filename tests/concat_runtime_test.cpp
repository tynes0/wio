#include <string_concat.h>
#include <text.h>

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace
{
    bool failNextAllocation = false;

    void expect(const bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
} // namespace

void* operator new(const std::size_t size)
{
    if (std::exchange(failNextAllocation, false))
        throw std::bad_alloc();
    if (auto* result = std::malloc(size ? size : 1))
        return result;
    throw std::bad_alloc();
}

void* operator new[](const std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* pointer) noexcept
{
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept
{
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept
{
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept
{
    std::free(pointer);
}

int main()
{
#if defined(_MSC_VER) && defined(_DEBUG)
    // A failed assertion must fail this test, never open a modal CRT dialog.
    for (const auto kind : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT})
    {
        _CrtSetReportMode(kind, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(kind, _CRTDBG_FILE_STDERR);
    }
#endif
    using wio::runtime::AppendConcat;
    using wio::runtime::Text;
    try
    {
        for (std::size_t size = 0; size < 128; ++size)
        {
            std::string value(size, 'x');
            value.append("a\0b", 3);
            const auto saved = value;
            const std::string suffix("z\0q", 3);
            AppendConcat(value, value, suffix, value, suffix);
            expect(value == saved + saved + suffix + saved + suffix, "string self/NUL concat mismatch");
            value.reserve(4096);
            const auto* buffer = value.data();
            AppendConcat(value, suffix, suffix);
            expect(value.data() == buffer, "reserved buffer discarded");
            const auto result = value;
            AppendConcat(value);
            expect(value == result, "empty concat changed value");
        }

        const auto a = Text::FromUtf8("🌍é日本語");
        const auto b = Text::FromUtf8("B⭐");
        expect(wio::runtime::Concat(a, b, a, b) == a + b + a + b, "flat text concat mismatch");
        const std::string bytes("a\0b", 3);
        expect(wio::runtime::Concat(bytes, bytes, bytes) == bytes + bytes + bytes,
               "flat string concat lost embedded NUL");
        auto unicode = a;
        AppendConcat(unicode, unicode, b, unicode);
        expect(unicode == a + a + b + a, "text self concat mismatch");
        auto left = a;
        auto right = b;
        expect(std::move(left) + right == a + b, "text owned left mismatch");
        left = a;
        right = b;
        expect(left + std::move(right) == a + b, "text owned right mismatch");
        left = a;
        right = b;
        expect(std::move(left) + std::move(right) == a + b, "text owned pair mismatch");
        left = a;
        expect(left + std::move(left) == a + a, "text mixed self alias mismatch");
        left = a;
        expect(std::move(left) + std::move(left) == a + a, "text owned self alias mismatch");

        std::string destination = "unchanged";
        const auto before = destination;
        const std::string large(8192, 'p');
        failNextAllocation = true;
        bool threw = false;
        try
        {
            AppendConcat(destination, destination, large, destination);
        }
        catch (const std::bad_alloc&)
        {
            threw = true;
        }

        expect(threw && destination == before, "allocation failure partially changed string");
        auto textDestination = a;
        const auto textLarge = Text::FromUtf8(large);
        failNextAllocation = true;
        threw = false;
        try
        {
            AppendConcat(textDestination, textDestination, textLarge, textDestination);
        }
        catch (const std::bad_alloc&)
        {
            threw = true;
        }

        expect(threw && textDestination == a, "allocation failure partially changed text");
    }
    catch (const std::exception& error)
    {
        failNextAllocation = false;
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
