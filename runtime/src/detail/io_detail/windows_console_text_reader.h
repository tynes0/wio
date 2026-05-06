#pragma once

#include "file_text_reader.h"

#if defined(_WIN32)

#ifndef NOMINMAX
    #define NOMINMAX
#endif

#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <windows.h>

namespace wio::runtime::console::detail
{
    class WindowsConsoleTextReader final : public TextReader
    {
    public:
        WindowsConsoleTextReader(FILE* file, const DWORD standardHandleId) noexcept
            : m_FileFallback(file)
            , m_Handle(::GetStdHandle(standardHandleId))
        {
            DWORD mode = 0;
            m_IsConsole =
                m_Handle != nullptr &&
                m_Handle != INVALID_HANDLE_VALUE &&
                ::GetConsoleMode(m_Handle, &mode) != FALSE;
        }

        [[nodiscard]] Result<int> Read() noexcept override
        {
            if (!m_IsConsole)
                return m_FileFallback.Read();

            auto cp = ReadCodePoint();
            if (!cp)
                return cp.Error();

            return static_cast<int>(cp.Value());
        }

        [[nodiscard]] Result<std::string> ReadLine() override
        {
            if (!m_IsConsole)
                return m_FileFallback.ReadLine();

            std::string output;
            while (true)
            {
                while (m_PendingCount > 0)
                {
                    auto cp = ReadCodePoint();
                    if (!cp)
                        return cp.Error();

                    if (cp.Value() == U'\r' || cp.Value() == U'\n')
                    {
                        if (cp.Value() == U'\r' && m_PendingCount > 0 && m_PendingCodePoints[m_PendingStart] == U'\n')
                            (void)ReadCodePoint();
                        return output;
                    }

                    auto encoded = AppendUtf8(output, cp.Value());
                    if (!encoded)
                        return encoded.Error();
                }

                std::array<WCHAR, 256> buffer {};
                DWORD read = 0;
                if (::ReadConsoleW(m_Handle, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) == FALSE)
                {
                    return MakeConsoleError(
                        ConsoleStatus::PlatformError,
                        ConsoleErrorDomain::Win32,
                        static_cast<int>(::GetLastError())
                    );
                }

                if (read == 0)
                {
                    if (!output.empty())
                        return output;
                    return MakeConsoleError(ConsoleStatus::EndOfFile, ConsoleErrorDomain::Win32);
                }

                for (std::size_t i = 0; i < static_cast<std::size_t>(read); ++i)
                {
                    auto cp = DecodeBufferedCodePoint(buffer, static_cast<std::size_t>(read), i);
                    if (!cp)
                        return cp.Error();

                    if (cp.Value() == U'\r' || cp.Value() == U'\n')
                    {
                        if (cp.Value() == U'\r' && i + 1U < static_cast<std::size_t>(read) && buffer[i + 1U] == L'\n')
                            ++i;

                        auto pending = StorePending(buffer, static_cast<std::size_t>(read), i + 1U);
                        if (!pending)
                            return pending.Error();

                        return output;
                    }

                    auto encoded = AppendUtf8(output, cp.Value());
                    if (!encoded)
                        return encoded.Error();
                }
            }
        }

        [[nodiscard]] Result<std::string> ReadCount(const std::size_t count) override
        {
            if (!m_IsConsole)
                return m_FileFallback.ReadCount(count);

            std::string output;
            for (std::size_t i = 0; i < count; ++i)
            {
                auto cp = ReadCodePoint();
                if (!cp)
                {
                    if (cp.Error().Status == ConsoleStatus::EndOfFile && !output.empty())
                        return output;
                    return cp.Error();
                }

                auto encoded = AppendUtf8(output, cp.Value());
                if (!encoded)
                    return encoded.Error();
            }

            return output;
        }

        [[nodiscard]] Result<std::string> ReadUntil(
            const char delimiter,
            const bool includeDelimiter = false) override
        {
            if (!m_IsConsole)
                return m_FileFallback.ReadUntil(delimiter, includeDelimiter);

            std::string output;
            while (true)
            {
                auto cp = ReadCodePoint();
                if (!cp)
                {
                    if (cp.Error().Status == ConsoleStatus::EndOfFile && !output.empty())
                        return output;
                    return cp.Error();
                }

                if (cp.Value() == static_cast<unsigned char>(delimiter))
                {
                    if (includeDelimiter)
                    {
                        auto encoded = AppendUtf8(output, cp.Value());
                        if (!encoded)
                            return encoded.Error();
                    }
                    return output;
                }

                auto encoded = AppendUtf8(output, cp.Value());
                if (!encoded)
                    return encoded.Error();
            }
        }

        [[nodiscard]] Result<std::string> ReadWord() override
        {
            if (!m_IsConsole)
                return m_FileFallback.ReadWord();

            std::string output;
            bool started = false;
            while (true)
            {
                auto cp = ReadCodePoint();
                if (!cp)
                {
                    if (cp.Error().Status == ConsoleStatus::EndOfFile && started)
                        return output;
                    return cp.Error();
                }

                const char32_t ch = cp.Value();
                const bool space = ch == U' ' || ch == U'\t' || ch == U'\r' || ch == U'\n' || ch == U'\f' || ch == U'\v';
                if (!started)
                {
                    if (space)
                        continue;
                    started = true;
                }
                else if (space)
                {
                    return output;
                }

                auto encoded = AppendUtf8(output, ch);
                if (!encoded)
                    return encoded.Error();
            }
        }

    private:
        [[nodiscard]] Result<char32_t> ReadCodePoint() noexcept
        {
            if (m_PendingCount > 0)
            {
                const char32_t value = m_PendingCodePoints[m_PendingStart];
                m_PendingStart = (m_PendingStart + 1U) % m_PendingCodePoints.size();
                --m_PendingCount;
                if (m_PendingCount == 0)
                    m_PendingStart = 0;
                return value;
            }

            return ReadConsoleCodePoint();
        }

        [[nodiscard]] Result<char32_t> ReadConsoleCodePoint() const noexcept
        {
            WCHAR first = 0;
            DWORD read = 0;
            if (::ReadConsoleW(m_Handle, &first, 1, &read, nullptr) == FALSE)
            {
                return MakeConsoleError(
                    ConsoleStatus::PlatformError,
                    ConsoleErrorDomain::Win32,
                    static_cast<int>(::GetLastError())
                );
            }

            if (read == 0)
                return MakeConsoleError(ConsoleStatus::EndOfFile, ConsoleErrorDomain::Win32);

            if (first >= 0xD800 && first <= 0xDBFF)
            {
                WCHAR second = 0;
                read = 0;
                if (::ReadConsoleW(m_Handle, &second, 1, &read, nullptr) == FALSE || read == 0)
                    return MakeConsoleError(ConsoleStatus::EncodingError, ConsoleErrorDomain::Win32);

                if (second < 0xDC00 || second > 0xDFFF)
                    return MakeConsoleError(ConsoleStatus::EncodingError, ConsoleErrorDomain::Win32);

                return static_cast<char32_t>(
                    0x10000U +
                    ((static_cast<char32_t>(first) - 0xD800U) << 10U) +
                    (static_cast<char32_t>(second) - 0xDC00U)
                );
            }

            if (first >= 0xDC00 && first <= 0xDFFF)
                return MakeConsoleError(ConsoleStatus::EncodingError, ConsoleErrorDomain::Win32);

            return static_cast<char32_t>(first);
        }

        [[nodiscard]] Result<char32_t> DecodeBufferedCodePoint(const std::array<WCHAR, 256>& buffer, const std::size_t count, std::size_t& index) const noexcept
        {
            const WCHAR first = buffer[index];
            if (first >= 0xD800 && first <= 0xDBFF)
            {
                WCHAR second = 0;
                if (index + 1U < count)
                {
                    second = buffer[++index];
                }
                else
                {
                    DWORD read = 0;
                    if (::ReadConsoleW(m_Handle, &second, 1, &read, nullptr) == FALSE || read == 0)
                        return MakeConsoleError(ConsoleStatus::EncodingError, ConsoleErrorDomain::Win32);
                }

                if (second < 0xDC00 || second > 0xDFFF)
                    return MakeConsoleError(ConsoleStatus::EncodingError, ConsoleErrorDomain::Win32);

                return static_cast<char32_t>(
                    0x10000U +
                    ((static_cast<char32_t>(first) - 0xD800U) << 10U) +
                    (static_cast<char32_t>(second) - 0xDC00U)
                );
            }

            if (first >= 0xDC00 && first <= 0xDFFF)
                return MakeConsoleError(ConsoleStatus::EncodingError, ConsoleErrorDomain::Win32);

            return static_cast<char32_t>(first);
        }

        [[nodiscard]] Result<void> StorePending(
            const std::array<WCHAR, 256>& buffer,
            const std::size_t count,
            const std::size_t start) noexcept
        {
            if (start >= count)
                return {};

            m_PendingStart = 0;
            m_PendingCount = 0;

            for (std::size_t i = start; i < count; ++i)
            {
                if (m_PendingCount >= m_PendingCodePoints.size())
                    return MakeConsoleError(ConsoleStatus::InvalidRange, ConsoleErrorDomain::Win32);

                auto cp = DecodeBufferedCodePoint(buffer, count, i);
                if (!cp)
                    return cp.Error();

                m_PendingCodePoints[m_PendingCount++] = cp.Value();
            }

            return {};
        }

        [[nodiscard]] static Result<void> AppendUtf8(std::string& output, const char32_t cp)
        {
            try
            {
                if (cp <= 0x7FU)
                {
                    output.push_back(static_cast<char>(cp));
                }
                else if (cp <= 0x7FFU)
                {
                    output.push_back(static_cast<char>(0xC0U | (cp >> 6U)));
                    output.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
                }
                else if (cp <= 0xFFFFU)
                {
                    if (cp >= 0xD800U && cp <= 0xDFFFU)
                        return MakeConsoleError(ConsoleStatus::EncodingError, ConsoleErrorDomain::Win32);

                    output.push_back(static_cast<char>(0xE0U | (cp >> 12U)));
                    output.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
                    output.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
                }
                else if (cp <= 0x10FFFFU)
                {
                    output.push_back(static_cast<char>(0xF0U | (cp >> 18U)));
                    output.push_back(static_cast<char>(0x80U | ((cp >> 12U) & 0x3FU)));
                    output.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
                    output.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
                }
                else
                {
                    return MakeConsoleError(ConsoleStatus::EncodingError, ConsoleErrorDomain::Win32);
                }
            }
            catch (...)
            {
                return MakeConsoleError(ConsoleStatus::UnknownError, ConsoleErrorDomain::Win32);
            }

            return {};
        }

    private:
        FileTextReader m_FileFallback;
        HANDLE m_Handle = nullptr;
        bool m_IsConsole = false;
        std::array<char32_t, 512> m_PendingCodePoints {};
        std::size_t m_PendingStart = 0;
        std::size_t m_PendingCount = 0;
    };

    [[nodiscard]] inline TextReaderPtr MakeWindowsConsoleReader(FILE* file, const DWORD standardHandleId)
    {
        return std::make_shared<WindowsConsoleTextReader>(file, standardHandleId);
    }
}

#endif
