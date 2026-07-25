#include "std_time.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>

namespace wio::runtime::std_time
{
    namespace
    {
        bool safeLocalTime(const std::time_t value, std::tm& output) noexcept
        {
#if defined(_WIN32)
            return localtime_s(&output, &value) == 0;
#else
            return localtime_r(&value, &output) != nullptr;
#endif
        }

        bool safeGmTime(const std::time_t value, std::tm& output) noexcept
        {
#if defined(_WIN32)
            return gmtime_s(&output, &value) == 0;
#else
            return gmtime_r(&value, &output) != nullptr;
#endif
        }

        std::time_t utcTime(std::tm& value) noexcept
        {
#if defined(_WIN32)
            return _mkgmtime(&value);
#else
            return timegm(&value);
#endif
        }

        std::int64_t floorMilliseconds(const std::int64_t value) noexcept
        {
            const std::int64_t remainder = value % 1000;
            return remainder < 0 ? value - (1000 + remainder) : value - remainder;
        }
    }

    std::int64_t UnixSeconds() noexcept
    {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::int64_t UnixMilliseconds() noexcept
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::int64_t UnixNanoseconds() noexcept
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::int64_t MonotonicNanoseconds() noexcept
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void SleepMilliseconds(const std::uint64_t milliseconds) noexcept
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }

    void SleepNanoseconds(const std::uint64_t nanoseconds) noexcept
    {
        std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds));
    }

    bool IsLeapYear(const std::int32_t year) noexcept
    {
        return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    }

    std::int32_t DaysInMonth(const std::int32_t year, const std::int32_t month) noexcept
    {
        static constexpr std::int32_t Days[] = {
            0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
        };
        if (month < 1 || month > 12)
            return 0;
        if (month == 2 && IsLeapYear(year))
            return 29;
        return Days[month];
    }

    bool BreakDown(
        const std::int64_t unixMilliseconds,
        const bool local,
        std::int32_t& year,
        std::int32_t& month,
        std::int32_t& day,
        std::int32_t& hour,
        std::int32_t& minute,
        std::int32_t& second,
        std::int32_t& millisecond,
        std::int32_t& weekDay,
        std::int32_t& yearDay) noexcept
    {
        const std::int64_t floored = floorMilliseconds(unixMilliseconds);
        const std::time_t seconds = static_cast<std::time_t>(floored / 1000);
        std::tm value{};
        if (!(local ? safeLocalTime(seconds, value) : safeGmTime(seconds, value)))
            return false;

        year = value.tm_year + 1900;
        month = value.tm_mon + 1;
        day = value.tm_mday;
        hour = value.tm_hour;
        minute = value.tm_min;
        second = value.tm_sec;
        millisecond = static_cast<std::int32_t>(unixMilliseconds - floored);
        weekDay = value.tm_wday;
        yearDay = value.tm_yday + 1;
        return true;
    }

    bool ToUnixMilliseconds(
        const std::int32_t year,
        const std::int32_t month,
        const std::int32_t day,
        const std::int32_t hour,
        const std::int32_t minute,
        const std::int32_t second,
        const std::int32_t millisecond,
        const bool local,
        std::int64_t& result) noexcept
    {
        if (month < 1 || month > 12 ||
            day < 1 || day > DaysInMonth(year, month) ||
            hour < 0 || hour > 23 ||
            minute < 0 || minute > 59 ||
            second < 0 || second > 60 ||
            millisecond < 0 || millisecond > 999)
            return false;

        std::tm value{};
        value.tm_year = year - 1900;
        value.tm_mon = month - 1;
        value.tm_mday = day;
        value.tm_hour = hour;
        value.tm_min = minute;
        value.tm_sec = second;
        value.tm_isdst = local ? -1 : 0;
        const std::time_t seconds = local ? std::mktime(&value) : utcTime(value);
        if (seconds == static_cast<std::time_t>(-1))
            return false;

        result = static_cast<std::int64_t>(seconds) * 1000 + millisecond;
        return true;
    }

    std::string FormatIso8601(const std::int64_t unixMilliseconds, const bool local)
    {
        std::int32_t year = 0;
        std::int32_t month = 0;
        std::int32_t day = 0;
        std::int32_t hour = 0;
        std::int32_t minute = 0;
        std::int32_t second = 0;
        std::int32_t millisecond = 0;
        std::int32_t weekDay = 0;
        std::int32_t yearDay = 0;
        if (!BreakDown(
                unixMilliseconds,
                local,
                year,
                month,
                day,
                hour,
                minute,
                second,
                millisecond,
                weekDay,
                yearDay))
            return {};

        std::ostringstream stream;
        stream << std::setfill('0')
               << std::setw(4) << year << '-'
               << std::setw(2) << month << '-'
               << std::setw(2) << day << 'T'
               << std::setw(2) << hour << ':'
               << std::setw(2) << minute << ':'
               << std::setw(2) << second << '.'
               << std::setw(3) << millisecond;
        if (!local)
            stream << 'Z';
        return stream.str();
    }
}
