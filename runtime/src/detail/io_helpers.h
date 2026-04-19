#pragma once

#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <span>
#include <type_traits>
#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
    #include <conio.h>
#else
    #include <unistd.h>
    #include <termios.h>
#endif

namespace wio::runtime::detail::io_helpers
{
    enum class WriteError : uint8_t
    {
        none = 0,
        null_file,
        null_data,
        size_overflow,
        io_error,
        partial_write
    };

    enum class ReadError : uint8_t
    {
        none = 0,
        null_file,
        invalid_argument,
        eof,
        io_error,
        partial_read,
        platform_error
    };

    struct WriteResult
    {
        WriteError error = WriteError::none;
        std::size_t bytesRequested = 0;
        std::size_t bytesWritten = 0;

        [[nodiscard]] constexpr bool Ok() const noexcept
        {
            return error == WriteError::none;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return Ok();
        }
    };

    struct ReadResult
    {
        ReadError error = ReadError::none;
        std::size_t bytesRequested = 0;
        std::size_t bytesRead = 0;

        [[nodiscard]] constexpr bool Ok() const noexcept
        {
            return error == ReadError::none;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return Ok();
        }
    };

    namespace detail
    {
        [[nodiscard]] inline WriteResult MakeWriteResult(const WriteError error, const std::size_t bytesRequested, const std::size_t bytesWritten) noexcept
        {
            return {
                .error = error,
                .bytesRequested = bytesRequested,
                .bytesWritten = bytesWritten
            };
        }

        [[nodiscard]] inline ReadResult MakeReadResult(const ReadError error, const std::size_t bytesRequested, const std::size_t bytesRead) noexcept
        {
            return {
                .error = error,
                .bytesRequested = bytesRequested,
                .bytesRead = bytesRead
            };
        }

        template <typename T>
        using Decay = std::decay_t<T>;

        template <typename T>
        inline constexpr bool IsCharArrayV =
            std::is_array_v<std::remove_reference_t<T>> &&
            std::is_same_v<std::remove_extent_t<std::remove_reference_t<T>>, char>;

        template <typename T>
        inline constexpr bool IsWritableTextV =
            std::is_same_v<Decay<T>, std::string_view> ||
            std::is_same_v<Decay<T>, std::string> ||
            std::is_same_v<Decay<T>, const char*> ||
            std::is_same_v<Decay<T>, char*> ||
            IsCharArrayV<T>;

        [[nodiscard]] inline WriteResult WriteBytesImpl(FILE* file, const void* data, std::size_t size) noexcept
        {
            if (file == nullptr)
                return MakeWriteResult(WriteError::null_file, size, 0);

            if (size == 0)
                return MakeWriteResult(WriteError::none, 0, 0);

            if (data == nullptr)
                return MakeWriteResult(WriteError::null_data, size, 0);

            const std::size_t written = std::fwrite(data, 1, size, file);
            if (written == size)
                return MakeWriteResult(WriteError::none, size, written);

            return MakeWriteResult(std::ferror(file) ? WriteError::io_error : WriteError::partial_write, size, written);
        }

#if defined(_WIN32) && !defined(__MINGW32__)
        [[nodiscard]] inline WriteResult WriteBytesUnlocked(FILE* file, const void* data, std::size_t size) noexcept
        {
            return WriteBytesImpl(file, data, size);
        }

        [[nodiscard]] inline WriteResult WriteBytes(FILE* file, const void* data, std::size_t size) noexcept
        {
            if (file == nullptr)
                return MakeWriteResult(WriteError::null_file, size, 0);

            _lock_file(file);
            const WriteResult result = WriteBytesImpl(file, data, size);
            _unlock_file(file);
            return result;
        }
#elif defined(__GLIBC__) || defined(__APPLE__) || defined(__linux__)
        [[nodiscard]] inline WriteResult WriteBytesUnlocked(FILE* file, const void* data, std::size_t size) noexcept
        {
            if (file == nullptr)
                return MakeWriteResult(WriteError::null_file, size, 0);

            if (size == 0)
                return MakeWriteResult(WriteError::none, 0, 0);

            if (data == nullptr)
                return MakeWriteResult(WriteError::null_data, size, 0);

            const std::size_t written = std::fwrite_unlocked(data, 1, size, file);
            if (written == size)
                return MakeWriteResult(WriteError::none, size, written);

            return MakeWriteResult(std::ferror(file) ? WriteError::io_error : WriteError::partial_write, size, written);
        }

        [[nodiscard]] inline WriteResult WriteBytes(FILE* file, const void* data, std::size_t size) noexcept
        {
            if (file == nullptr)
                return MakeWriteResult(WriteError::null_file, size, 0);

            flockfile(file);
            const WriteResult result = WriteBytesUnlocked(file, data, size);
            funlockfile(file);
            return result;
        }
#else
        [[nodiscard]] inline WriteResult WriteBytesUnlocked(FILE* file, const void* data, std::size_t size) noexcept
        {
            return WriteBytesImpl(file, data, size);
        }

        [[nodiscard]] inline WriteResult WriteBytes(FILE* file, const void* data, std::size_t size) noexcept
        {
            return WriteBytesImpl(file, data, size);
        }
#endif

        [[nodiscard]] inline bool IsSpace(const int ch) noexcept
        {
            return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
        }
    }

    [[nodiscard]] inline WriteResult Write(FILE* file, const void* data, std::size_t size) noexcept
    {
        return detail::WriteBytes(file, data, size);
    }

    [[nodiscard]] inline WriteResult WriteUnlocked(FILE* file, const void* data, std::size_t size) noexcept
    {
        return detail::WriteBytesUnlocked(file, data, size);
    }

    [[nodiscard]] inline WriteResult Write(FILE* file, std::string_view value) noexcept
    {
        return Write(file, value.data(), value.size());
    }

    [[nodiscard]] inline WriteResult WriteUnlocked(FILE* file, std::string_view value) noexcept
    {
        return WriteUnlocked(file, value.data(), value.size());
    }

    [[nodiscard]] inline WriteResult Write(FILE* file, const std::string& value) noexcept
    {
        return Write(file, value.data(), value.size());
    }

    [[nodiscard]] inline WriteResult WriteUnlocked(FILE* file, const std::string& value) noexcept
    {
        return WriteUnlocked(file, value.data(), value.size());
    }

    [[nodiscard]] inline WriteResult Write(FILE* file, const char* value) noexcept
    {
        if (value == nullptr)
            return detail::MakeWriteResult(WriteError::null_data, 0, 0);

        return Write(file, value, std::strlen(value));
    }

    [[nodiscard]] inline WriteResult WriteUnlocked(FILE* file, const char* value) noexcept
    {
        if (value == nullptr)
            return detail::MakeWriteResult(WriteError::null_data, 0, 0);

        return WriteUnlocked(file, value, std::strlen(value));
    }

    template <std::size_t Size>
    [[nodiscard]] inline WriteResult Write(FILE* file, const char (&value)[Size]) noexcept
    {
        static_assert(Size > 0);
        return Write(file, value, Size - 1);
    }

    template <std::size_t Size>
    [[nodiscard]] inline WriteResult WriteUnlocked(FILE* file, const char (&value)[Size]) noexcept
    {
        static_assert(Size > 0);
        return WriteUnlocked(file, value, Size - 1);
    }

    [[nodiscard]] inline WriteResult Write(FILE* file, std::span<const std::byte> bytes) noexcept
    {
        return Write(file, bytes.data(), bytes.size());
    }

    [[nodiscard]] inline WriteResult WriteUnlocked(FILE* file, std::span<const std::byte> bytes) noexcept
    {
        return WriteUnlocked(file, bytes.data(), bytes.size());
    }

    [[nodiscard]] inline ReadResult ReadChar(FILE* file, char& outChar) noexcept
    {
        if (file == nullptr)
            return detail::MakeReadResult(ReadError::null_file, 1, 0);

        const int ch = std::fgetc(file);
        if (ch == EOF)
        {
            return std::feof(file)
                ? detail::MakeReadResult(ReadError::eof, 1, 0)
                : detail::MakeReadResult(ReadError::io_error, 1, 0);
        }

        outChar = static_cast<char>(ch);
        return detail::MakeReadResult(ReadError::none, 1, 1);
    }

    [[nodiscard]] inline ReadResult ReadCount(FILE* file, std::string& outValue, const std::size_t count)
    {
        outValue.clear();

        if (file == nullptr)
            return detail::MakeReadResult(ReadError::null_file, count, 0);

        if (count == 0)
            return detail::MakeReadResult(ReadError::none, 0, 0);

        outValue.resize(count);
        const std::size_t bytesRead = std::fread(outValue.data(), 1, count, file);

        if (bytesRead < count)
        {
            outValue.resize(bytesRead);

            if (std::ferror(file))
                return detail::MakeReadResult(ReadError::io_error, count, bytesRead);

            if (std::feof(file))
                return detail::MakeReadResult(bytesRead == 0 ? ReadError::eof : ReadError::partial_read, count, bytesRead);

            return detail::MakeReadResult(ReadError::partial_read, count, bytesRead);
        }

        return detail::MakeReadResult(ReadError::none, count, bytesRead);
    }

    [[nodiscard]] inline ReadResult ReadUntil(FILE* file, std::string& outValue, const char delimiter, const bool includeDelimiter = false, const bool stopOnEof = true)
    {
        outValue.clear();

        if (file == nullptr)
            return detail::MakeReadResult(ReadError::null_file, 0, 0);

        std::size_t bytesRead = 0;

        while (true)
        {
            const int ch = std::fgetc(file);
            if (ch == EOF)
            {
                if (std::ferror(file))
                    return detail::MakeReadResult(ReadError::io_error, 0, bytesRead);

                if (std::feof(file))
                {
                    if (stopOnEof && !outValue.empty())
                        return detail::MakeReadResult(ReadError::none, 0, bytesRead);

                    return detail::MakeReadResult(outValue.empty() ? ReadError::eof : ReadError::partial_read, 0, bytesRead);
                }

                return detail::MakeReadResult(ReadError::io_error, 0, bytesRead);
            }

            ++bytesRead;

            if (static_cast<char>(ch) == delimiter)
            {
                if (includeDelimiter)
                    outValue.push_back(static_cast<char>(ch));

                return detail::MakeReadResult(ReadError::none, 0, bytesRead);
            }

            outValue.push_back(static_cast<char>(ch));
        }
    }

    [[nodiscard]] inline ReadResult ReadLine(FILE* file, std::string& outValue)
    {
        return ReadUntil(file, outValue, '\n', false, true);
    }

    [[nodiscard]] inline ReadResult ReadWord(FILE* file, std::string& outValue)
    {
        outValue.clear();

        if (file == nullptr)
            return detail::MakeReadResult(ReadError::null_file, 0, 0);

        std::size_t bytesRead = 0;

        while (true)
        {
            const int ch = std::fgetc(file);
            if (ch == EOF)
            {
                if (std::ferror(file))
                    return detail::MakeReadResult(ReadError::io_error, 0, bytesRead);

                return detail::MakeReadResult(ReadError::eof, 0, bytesRead);
            }

            ++bytesRead;

            if (!detail::IsSpace(ch))
            {
                outValue.push_back(static_cast<char>(ch));
                break;
            }
        }

        while (true)
        {
            const int ch = std::fgetc(file);
            if (ch == EOF)
            {
                if (std::ferror(file))
                    return detail::MakeReadResult(ReadError::io_error, 0, bytesRead);

                return detail::MakeReadResult(ReadError::none, 0, bytesRead);
            }

            ++bytesRead;

            if (detail::IsSpace(ch))
                return detail::MakeReadResult(ReadError::none, 0, bytesRead);

            outValue.push_back(static_cast<char>(ch));
        }
    }

    [[nodiscard]] inline ReadResult ReadCharHidden(char& outChar) noexcept
    {
#if defined(_WIN32)
        const int ch = _getch();
        if (ch == EOF)
            return detail::MakeReadResult(ReadError::io_error, 1, 0);

        outChar = static_cast<char>(ch);
        return detail::MakeReadResult(ReadError::none, 1, 1);
#else
        termios oldTerm{};
        if (tcgetattr(STDIN_FILENO, &oldTerm) != 0)
            return detail::MakeReadResult(ReadError::platform_error, 1, 0);

        termios newTerm = oldTerm;
        newTerm.c_lflag &= static_cast<unsigned>(~(ICANON | ECHO));

        if (tcsetattr(STDIN_FILENO, TCSANOW, &newTerm) != 0)
            return detail::MakeReadResult(ReadError::platform_error, 1, 0);

        unsigned char ch = 0;
        const ssize_t n = ::read(STDIN_FILENO, &ch, 1);

        (void)tcsetattr(STDIN_FILENO, TCSANOW, &oldTerm);

        if (n < 0)
            return detail::MakeReadResult(ReadError::io_error, 1, 0);
        if (n == 0)
            return detail::MakeReadResult(ReadError::eof, 1, 0);

        outChar = static_cast<char>(ch);
        return detail::MakeReadResult(ReadError::none, 1, 1);
#endif
    }

    [[nodiscard]] inline ReadResult InputLine(std::string& outValue)
    {
        return ReadLine(stdin, outValue);
    }

    [[nodiscard]] inline ReadResult InputCount(std::string& outValue, const std::size_t count)
    {
        return ReadCount(stdin, outValue, count);
    }

    [[nodiscard]] inline ReadResult InputChar(char& outChar) noexcept
    {
        return ReadChar(stdin, outChar);
    }

    [[nodiscard]] inline ReadResult InputWord(std::string& outValue)
    {
        return ReadWord(stdin, outValue);
    }

    [[nodiscard]] inline ReadResult InputCharHidden(char& outChar) noexcept
    {
        return ReadCharHidden(outChar);
    }

    [[nodiscard]] inline ReadResult InputUntil(std::string& outValue, const char delimiter, const bool includeDelimiter = false)
    {
        return ReadUntil(stdin, outValue, delimiter, includeDelimiter, true);
    }
}
