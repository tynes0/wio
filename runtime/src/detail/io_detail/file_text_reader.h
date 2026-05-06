#pragma once

#include <cstdio>
#include <string>

#include "../../../include/std_console.h"
#include "io_helpers.h"

namespace wio::runtime::console::detail
{
    [[nodiscard]] inline ConsoleStatus MapReadError(const ::wio::runtime::detail::io_helpers::ReadError error) noexcept
    {
        using ReadError = ::wio::runtime::detail::io_helpers::ReadError;

        switch (error)
        {
        case ReadError::none:
            return ConsoleStatus::Ok;
        case ReadError::null_file:
            return ConsoleStatus::NullArgument;
        case ReadError::invalid_argument:
            return ConsoleStatus::InvalidArgument;
        case ReadError::eof:
            return ConsoleStatus::EndOfFile;
        case ReadError::io_error:
            return ConsoleStatus::IoError;
        case ReadError::partial_read:
            return ConsoleStatus::PartialIo;
        case ReadError::platform_error:
            return ConsoleStatus::PlatformError;
        }
        return ConsoleStatus::UnknownError;
    }

    class FileTextReader final : public TextReader
    {
    public:
        explicit FileTextReader(FILE* file) noexcept
            : m_File(file)
        {
        }

        [[nodiscard]] Result<int> Read() noexcept override
        {
            char ch = '\0';
            const auto result = ::wio::runtime::detail::io_helpers::ReadChar(m_File, ch);

            if (!result.Ok())
            {
                return MakeConsoleError(
                    MapReadError(result.error),
                    ConsoleErrorDomain::Stdio
                );
            }

            return static_cast<int>(static_cast<unsigned char>(ch));
        }

        [[nodiscard]] Result<std::string> ReadLine() override
        {
            std::string value;
            const auto result = ::wio::runtime::detail::io_helpers::ReadLine(m_File, value);

            if (!result.Ok())
            {
                return MakeConsoleError(
                    MapReadError(result.error),
                    ConsoleErrorDomain::Stdio
                );
            }

            if (!value.empty() && value.back() == '\r')
                value.pop_back();

            return value;
        }

        [[nodiscard]] Result<std::string> ReadCount(const std::size_t count) override
        {
            std::string value;
            const auto result = ::wio::runtime::detail::io_helpers::ReadCount(m_File, value, count);

            if (!result.Ok())
            {
                return MakeConsoleError(
                    MapReadError(result.error),
                    ConsoleErrorDomain::Stdio
                );
            }

            return value;
        }

        [[nodiscard]] Result<std::string> ReadUntil(
            const char delimiter,
            const bool includeDelimiter = false) override
        {
            std::string value;
            const auto result = ::wio::runtime::detail::io_helpers::ReadUntil(
                m_File,
                value,
                delimiter,
                includeDelimiter,
                true
            );

            if (!result.Ok())
            {
                return MakeConsoleError(
                    MapReadError(result.error),
                    ConsoleErrorDomain::Stdio
                );
            }

            return value;
        }


        [[nodiscard]] Result<std::string> ReadWord() override
        {
            std::string value;
            const auto result = ::wio::runtime::detail::io_helpers::ReadWord(m_File, value);

            if (!result.Ok())
            {
                return MakeConsoleError(
                    MapReadError(result.error),
                    ConsoleErrorDomain::Stdio
                );
            }

            return value;
        }

        [[nodiscard]] FILE* NativeFile() const noexcept
        {
            return m_File;
        }

    private:
        FILE* m_File = nullptr;
    };
}
