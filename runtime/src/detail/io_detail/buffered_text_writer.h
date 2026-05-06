#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "../../../include/std_console.h"

namespace wio::runtime::console::detail
{
    class BufferedTextWriter final : public TextWriter // NOLINT(cppcoreguidelines-special-member-functions)
    {
    public:
        BufferedTextWriter(TextWriterPtr inner, const std::size_t bufferSize)
            : m_Inner(std::move(inner))
            , m_Buffer(bufferSize)
        {
        }

        ~BufferedTextWriter() override
        {
            (void)Flush();
        }

        [[nodiscard]] Result<IoCount> Write(const std::string_view text) noexcept override
        {
            if (!m_Inner)
                return MakeConsoleError(ConsoleStatus::NullArgument);

            if (text.empty())
                return IoCount {};

            if (m_Buffer.empty())
                return m_Inner->Write(text);

            IoCount total { .Requested = text.size(), .Processed = 0 };
            std::size_t offset = 0;

            while (offset < text.size())
            {
                const std::size_t available = m_Buffer.size() - m_Size;
                if (available == 0)
                {
                    auto flushed = FlushBuffer();
                    if (!flushed)
                        return flushed.Error();
                    continue;
                }

                const std::size_t remaining = text.size() - offset;
                if (m_Size == 0 && remaining >= m_Buffer.size())
                {
                    auto direct = m_Inner->Write(text.substr(offset, remaining));
                    if (!direct)
                        return direct;
                    total.Processed += direct.Value().Processed;
                    return total;
                }

                const std::size_t take = remaining < available ? remaining : available;
                for (std::size_t i = 0; i < take; ++i)
                    m_Buffer[m_Size + i] = text[offset + i];

                m_Size += take;
                offset += take;
                total.Processed += take;

                if (m_Size == m_Buffer.size())
                {
                    auto flushed = FlushBuffer();
                    if (!flushed)
                        return flushed.Error();
                }
            }

            return total;
        }

        [[nodiscard]] Result<void> Flush() noexcept override
        {
            auto flushed = FlushBuffer();
            if (!flushed)
                return flushed;

            return m_Inner ? m_Inner->Flush() : MakeConsoleError(ConsoleStatus::NullArgument);
        }

    private:
        [[nodiscard]] Result<void> FlushBuffer() noexcept
        {
            if (m_Size == 0)
                return {};

            if (!m_Inner)
                return MakeConsoleError(ConsoleStatus::NullArgument);

            auto written = m_Inner->Write(std::string_view(m_Buffer.data(), m_Size));
            if (!written)
                return written.Error();

            if (written.Value().Processed != m_Size)
                return MakeConsoleError(ConsoleStatus::PartialIo);

            m_Size = 0;
            return {};
        }

    private:
        TextWriterPtr m_Inner;
        std::vector<char> m_Buffer;
        std::size_t m_Size = 0;
    };
}
