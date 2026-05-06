#pragma once

#include "file_text_writer.h"

#if defined(_WIN32)

#ifndef NOMINMAX
    #define NOMINMAX
#endif

#include <array>
#include <cstdio>
#include <memory>
#include <string_view>
#include <windows.h>

namespace wio::runtime::console::detail
{
    class WindowsConsoleTextWriter final : public TextWriter
    {
    public:
        WindowsConsoleTextWriter(FILE* file, const DWORD standardHandleId) noexcept
            : m_FileFallback(file)
            , m_Handle(::GetStdHandle(standardHandleId))
        {
            DWORD mode = 0;
            m_IsConsole =
                m_Handle != nullptr &&
                m_Handle != INVALID_HANDLE_VALUE &&
                ::GetConsoleMode(m_Handle, &mode) != FALSE;
        }

        [[nodiscard]] Result<IoCount> Write(const std::string_view text) noexcept override
        {
            if (!m_IsConsole)
                return m_FileFallback.Write(text);

            if (text.empty())
                return IoCount {};

            std::size_t offset = 0;
            while (offset < text.size())
            {
                std::array<WCHAR, 1024> wide {};
                std::size_t wideSize = 0;
                const std::size_t chunkStart = offset;

                while (offset < text.size() && wideSize + 2 <= wide.size())
                {
                    auto decoded = DecodeOne(text, offset);
                    if (!decoded)
                        return decoded.Error();

                    const char32_t cp = decoded.Value();
                    if (cp <= 0xFFFFU)
                    {
                        wide[wideSize++] = static_cast<WCHAR>(cp);
                    }
                    else
                    {
                        const char32_t value = cp - 0x10000U;
                        wide[wideSize++] = static_cast<WCHAR>(0xD800U + (value >> 10U));
                        wide[wideSize++] = static_cast<WCHAR>(0xDC00U + (value & 0x3FFU));
                    }
                }

                if (wideSize == 0)
                    return MakeConsoleError(ConsoleStatus::EncodingError, ConsoleErrorDomain::Win32);

                DWORD written = 0;
                if (::WriteConsoleW(
                        m_Handle,
                        wide.data(),
                        static_cast<DWORD>(wideSize),
                        &written,
                        nullptr) == FALSE)
                {
                    return MakeConsoleError(
                        ConsoleStatus::PlatformError,
                        ConsoleErrorDomain::Win32,
                        static_cast<int>(::GetLastError())
                    );
                }

                if (written != wideSize)
                {
                    return IoCount {
                        .Requested = text.size(),
                        .Processed = chunkStart
                    };
                }
            }

            return IoCount {
                .Requested = text.size(),
                .Processed = text.size()
            };
        }

        [[nodiscard]] Result<void> Flush() noexcept override
        {
            return m_FileFallback.Flush();
        }

    private:
        [[nodiscard]] static Result<char32_t> DecodeOne(
            const std::string_view text,
            std::size_t& offset) noexcept
        {
            const auto fail = []
            {
                return MakeConsoleError(ConsoleStatus::EncodingError, ConsoleErrorDomain::Win32);
            };

            if (offset >= text.size())
                return fail();

            const auto byte0 = static_cast<unsigned char>(text[offset]);
            if (byte0 < 0x80U)
            {
                ++offset;
                return byte0;
            }

            int extra;
            char32_t codePoint;
            char32_t minimum;

            if ((byte0 & 0xE0U) == 0xC0U)
            {
                extra = 1;
                codePoint = static_cast<char32_t>(byte0 & 0x1FU);
                minimum = 0x80U;
            }
            else if ((byte0 & 0xF0U) == 0xE0U)
            {
                extra = 2;
                codePoint = static_cast<char32_t>(byte0 & 0x0FU);
                minimum = 0x800U;
            }
            else if ((byte0 & 0xF8U) == 0xF0U)
            {
                extra = 3;
                codePoint = static_cast<char32_t>(byte0 & 0x07U);
                minimum = 0x10000U;
            }
            else
            {
                return fail();
            }

            if (offset + static_cast<std::size_t>(extra) >= text.size())
                return fail();

            for (int i = 1; i <= extra; ++i)
            {
                const auto byte = static_cast<unsigned char>(text[offset + static_cast<std::size_t>(i)]);
                if ((byte & 0xC0U) != 0x80U)
                    return fail();

                codePoint = static_cast<char32_t>((codePoint << 6U) | static_cast<char32_t>(byte & 0x3FU));
            }

            if (codePoint < minimum || codePoint > 0x10FFFFU || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
                return fail();

            offset += static_cast<std::size_t>(extra + 1);
            return codePoint;
        }

    private:
        FileTextWriter m_FileFallback;
        HANDLE m_Handle = nullptr;
        bool m_IsConsole = false;
    };

    [[nodiscard]] inline TextWriterPtr MakeWindowsConsoleWriter(FILE* file, const DWORD standardHandleId)
    {
        return std::make_shared<WindowsConsoleTextWriter>(file, standardHandleId);
    }
}

#endif
