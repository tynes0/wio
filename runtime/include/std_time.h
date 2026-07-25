#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace wio::runtime::std_time
{
    [[nodiscard]] std::int64_t UnixSeconds() noexcept;
    [[nodiscard]] std::int64_t UnixMilliseconds() noexcept;
    [[nodiscard]] std::int64_t UnixNanoseconds() noexcept;
    [[nodiscard]] std::int64_t MonotonicNanoseconds() noexcept;
    void SleepMilliseconds(std::uint64_t milliseconds) noexcept;
    void SleepNanoseconds(std::uint64_t nanoseconds) noexcept;

    [[nodiscard]] bool IsLeapYear(std::int32_t year) noexcept;
    [[nodiscard]] std::int32_t DaysInMonth(std::int32_t year, std::int32_t month) noexcept;

    [[nodiscard]] bool BreakDown(
        std::int64_t unixMilliseconds,
        bool local,
        std::int32_t& year,
        std::int32_t& month,
        std::int32_t& day,
        std::int32_t& hour,
        std::int32_t& minute,
        std::int32_t& second,
        std::int32_t& millisecond,
        std::int32_t& weekDay,
        std::int32_t& yearDay) noexcept;

    [[nodiscard]] bool ToUnixMilliseconds(
        std::int32_t year,
        std::int32_t month,
        std::int32_t day,
        std::int32_t hour,
        std::int32_t minute,
        std::int32_t second,
        std::int32_t millisecond,
        bool local,
        std::int64_t& value) noexcept;

    [[nodiscard]] std::string FormatIso8601(std::int64_t unixMilliseconds, bool local);
}
