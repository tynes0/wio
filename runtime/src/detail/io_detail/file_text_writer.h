#pragma once

#include <cstdio>
#include <string_view>

#include "../../../include/std_console.h"
#include "io_helpers.h"

namespace wio::runtime::console::detail
{
    [[nodiscard]] inline ConsoleStatus MapWriteError(
        const ::wio::runtime::detail::io_helpers::WriteError error) noexcept
    {
        using WriteError = ::wio::runtime::detail::io_helpers::WriteError;

        switch (error)
        {
        case WriteError::none:
            return ConsoleStatus::Ok;
        case WriteError::null_file:
        case WriteError::null_data:
            return ConsoleStatus::NullArgument;
        case WriteError::size_overflow:
            return ConsoleStatus::InvalidRange;
        case WriteError::io_error:
            return ConsoleStatus::IoError;
        case WriteError::partial_write:
            return ConsoleStatus::PartialIo;
        
        }
        return ConsoleStatus::UnknownError;
    }

    class FileTextWriter final : public TextWriter
    {
    public:
        explicit FileTextWriter(FILE* file) noexcept
            : m_File(file)
        {
        }

        [[nodiscard]] Result<IoCount> Write(const std::string_view text) noexcept override
        {
            const auto result = ::wio::runtime::detail::io_helpers::Write(m_File, text);

            if (!result.Ok())
            {
                return MakeConsoleError(
                    MapWriteError(result.error),
                    ConsoleErrorDomain::Stdio
                );
            }

            return IoCount {
                .Requested = result.bytesRequested,
                .Processed = result.bytesWritten
            };
        }

        [[nodiscard]] Result<void> Flush() noexcept override
        {
            if (m_File == nullptr)
                return MakeConsoleError(ConsoleStatus::NullArgument, ConsoleErrorDomain::Stdio);

            if (std::fflush(m_File) != 0)
                return MakeConsoleError(ConsoleStatus::IoError, ConsoleErrorDomain::Stdio);

            return {};
        }

        [[nodiscard]] FILE* NativeFile() const noexcept
        {
            return m_File;
        }

    private:
        FILE* m_File = nullptr;
    };
}
