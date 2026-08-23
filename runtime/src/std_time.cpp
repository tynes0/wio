#include "std_time.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <thread>
#include <vector>

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

        std::int64_t floorDivide(const std::int64_t value, const std::int64_t divisor) noexcept
        {
            const std::int64_t quotient = value / divisor;
            return value % divisor < 0 ? quotient - 1 : quotient;
        }

        std::int64_t daysFromCivil(
            std::int64_t year,
            const std::uint32_t month,
            const std::uint32_t day) noexcept
        {
            year -= month <= 2u ? 1 : 0;
            const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
            const auto yearOfEra = static_cast<std::uint32_t>(year - era * 400);
            const std::int64_t adjustedMonth = static_cast<std::int64_t>(month) + (month > 2u ? -3 : 9);
            const auto dayOfYear = static_cast<std::uint32_t>(
                (153 * adjustedMonth + 2) / 5 + static_cast<std::int64_t>(day) - 1);
            const auto dayOfEra = yearOfEra * 365u + yearOfEra / 4u - yearOfEra / 100u + dayOfYear;
            return era * 146097 + static_cast<std::int64_t>(dayOfEra) - 719468;
        }

        bool civilFromDays(
            std::int64_t days,
            std::int32_t& year,
            std::int32_t& month,
            std::int32_t& day,
            std::int32_t& yearDay) noexcept
        {
            const std::int64_t serialDays = days;
            days += 719468;
            const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
            const auto dayOfEra = static_cast<std::uint32_t>(days - era * 146097);
            const auto yearOfEra = static_cast<std::uint32_t>(
                (dayOfEra - dayOfEra / 1460u + dayOfEra / 36524u - dayOfEra / 146096u) / 365u);
            std::int64_t resolvedYear = static_cast<std::int64_t>(yearOfEra) + era * 400;
            const auto dayOfYear = dayOfEra - (365u * yearOfEra + yearOfEra / 4u - yearOfEra / 100u);
            const auto monthPrime = static_cast<std::uint32_t>((5u * dayOfYear + 2u) / 153u);
            const auto resolvedDay = dayOfYear - (153u * monthPrime + 2u) / 5u + 1u;
            const auto resolvedMonth = monthPrime < 10u ? monthPrime + 3u : monthPrime - 9u;
            resolvedYear += resolvedMonth <= 2u ? 1 : 0;
            if (resolvedYear < (std::numeric_limits<std::int32_t>::min)() ||
                resolvedYear > (std::numeric_limits<std::int32_t>::max)())
                return false;

            year = static_cast<std::int32_t>(resolvedYear);
            month = static_cast<std::int32_t>(resolvedMonth);
            day = static_cast<std::int32_t>(resolvedDay);
            yearDay = static_cast<std::int32_t>(
                serialDays -
                daysFromCivil(resolvedYear, 1u, 1u) + 1);
            return true;
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
        if (!local)
        {
            const std::int64_t totalSeconds = floorDivide(unixMilliseconds, 1000);
            const std::int64_t days = floorDivide(totalSeconds, 86400);
            const std::int64_t secondOfDay = totalSeconds - days * 86400;
            if (!civilFromDays(days, year, month, day, yearDay))
                return false;
            hour = static_cast<std::int32_t>(secondOfDay / 3600);
            minute = static_cast<std::int32_t>((secondOfDay % 3600) / 60);
            second = static_cast<std::int32_t>(secondOfDay % 60);
            millisecond = static_cast<std::int32_t>(unixMilliseconds - totalSeconds * 1000);
            std::int64_t normalizedWeekDay = (days + 4) % 7;
            if (normalizedWeekDay < 0) normalizedWeekDay += 7;
            weekDay = static_cast<std::int32_t>(normalizedWeekDay);
            return true;
        }

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
            second < 0 || second > 59 ||
            millisecond < 0 || millisecond > 999)
            return false;

        if (!local)
        {
            const std::int64_t days = daysFromCivil(
                static_cast<std::int64_t>(year),
                static_cast<std::uint32_t>(month),
                static_cast<std::uint32_t>(day));
            constexpr std::int64_t MillisecondsPerDay = 86400000;
            const std::int64_t minDays = (std::numeric_limits<std::int64_t>::min)() / MillisecondsPerDay;
            const std::int64_t maxDays = (std::numeric_limits<std::int64_t>::max)() / MillisecondsPerDay;
            if (days < minDays || days > maxDays)
                return false;
            const std::int64_t timeOfDay =
                static_cast<std::int64_t>(hour) * 3600000 +
                static_cast<std::int64_t>(minute) * 60000 +
                static_cast<std::int64_t>(second) * 1000 + millisecond;
            const std::int64_t base = days * MillisecondsPerDay;
            if (base > (std::numeric_limits<std::int64_t>::max)() - timeOfDay)
                return false;
            result = base + timeOfDay;
            return true;
        }

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

    std::string Format(const std::int64_t unixMilliseconds, const bool local, const std::string_view pattern)
    {
        const std::time_t seconds = static_cast<std::time_t>(unixMilliseconds / 1000);
        std::tm broken{};
        if (!(local ? safeLocalTime(seconds, broken) : safeGmTime(seconds, broken))) return {};
        std::string patternText(pattern);
        std::vector<char> buffer(128);
        for (;;)
        {
            const std::size_t written = std::strftime(buffer.data(), buffer.size(), patternText.c_str(), &broken);
            if (written != 0) return std::string(buffer.data(), written);
            if (buffer.size() >= 65536u) return {};
            buffer.resize(buffer.size() * 2u);
        }
    }

    std::int32_t LocalUtcOffsetMinutes(const std::int64_t unixMilliseconds) noexcept
    {
        const std::time_t seconds = static_cast<std::time_t>(unixMilliseconds / 1000);
        std::tm local{}, utc{};
        if (!safeLocalTime(seconds, local) || !safeGmTime(seconds, utc)) return 0;
        const std::time_t localValue = std::mktime(&local);
        const std::time_t utcAsLocal = std::mktime(&utc);
        return static_cast<std::int32_t>(std::difftime(localValue, utcAsLocal) / 60.0);
    }
}
