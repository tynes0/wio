#include "std_console.h"

#include "exception.h"
#include "detail/io_helpers.h"

#include <charconv>

namespace wio::runtime::std_console
{
    namespace
    {
        using detail::io_helpers::Write;

        template <typename TValue>
        std::int32_t writeIntegralValue(TValue value)
        {
            using Casted =
                std::conditional_t<std::is_same_v<TValue, std::int8_t>, std::int32_t,
                std::conditional_t<std::is_same_v<TValue, std::uint8_t>, std::uint32_t, TValue>>;

            char buffer[64];
            const auto castedValue = static_cast<Casted>(value);
            auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), castedValue);
            if (ec != std::errc{})
                return -1;

            return Write(stdout, buffer, static_cast<std::size_t>(ptr - buffer)).Ok() ? 0 : -1;
        }

        template <typename TValue>
        std::int32_t writeFloatingValue(TValue value)
        {
            char buffer[64];
            auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);
            if (ec != std::errc{})
                return -1;

            return Write(stdout, buffer, static_cast<std::size_t>(ptr - buffer)).Ok() ? 0 : -1;
        }
    }

    std::int32_t WriteValue(bool value)
    {
        return Write(stdout, value ? "true" : "false").Ok() ? 0 : -1;
    }

    std::int32_t WriteValue(char value)
    {
        return Write(stdout, &value, 1).Ok() ? 0 : -1;
    }

    std::int32_t WriteValue(std::int8_t value)
    {
        return writeIntegralValue(value);
    }

    std::int32_t WriteValue(std::int16_t value)
    {
        return writeIntegralValue(value);
    }

    std::int32_t WriteValue(std::int32_t value)
    {
        return writeIntegralValue(value);
    }

    std::int32_t WriteValue(std::int64_t value)
    {
        return writeIntegralValue(value);
    }

    std::int32_t WriteValue(std::uint8_t value)
    {
        return writeIntegralValue(value);
    }

    std::int32_t WriteValue(std::uint16_t value)
    {
        return writeIntegralValue(value);
    }

    std::int32_t WriteValue(std::uint32_t value)
    {
        return writeIntegralValue(value);
    }

    std::int32_t WriteValue(std::uint64_t value)
    {
        return writeIntegralValue(value);
    }

    std::int32_t WriteValue(float value)
    {
        return writeFloatingValue(value);
    }

    std::int32_t WriteValue(double value)
    {
        return writeFloatingValue(value);
    }

    std::int32_t WriteValue(const char* value)
    {
        return Write(stdout, value != nullptr ? value : "(null)").Ok() ? 0 : -1;
    }

    std::int32_t WriteValue(char* value)
    {
        return WriteValue(static_cast<const char*>(value));
    }

    std::int32_t WriteValue(const std::string& value)
    {
        return Write(stdout, value).Ok() ? 0 : -1;
    }

    std::int32_t WriteValue(std::string_view value)
    {
        return Write(stdout, value).Ok() ? 0 : -1;
    }

    std::string Input(const std::string& prompt)
    {
        (void)WriteValue(prompt);
        std::string result;
        (void)detail::io_helpers::InputLine(result);
        return result;
    }

    std::string InputN(const std::size_t count)
    {
        std::string result;
        (void)detail::io_helpers::InputCount(result, count);
        return result;
    }

    char InputChar(bool isHidden)
    {
        char result = '\0';
        if (!isHidden)
            (void)detail::io_helpers::InputChar(result);
        else
            (void)detail::io_helpers::InputCharHidden(result);
        return result;
    }

    std::string InputWord()
    {
        std::string result;
        (void)detail::io_helpers::InputWord(result);
        return result;
    }

    std::string InputUntil(const char delimiter, const bool includeDelimiter)
    {
        std::string result;
        (void)detail::io_helpers::InputUntil(result, delimiter, includeDelimiter);
        return result;
    }
}
