#pragma once

#include <charconv>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <span>
#include <type_traits>
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace wio::runtime::std_io
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

    namespace detail
    {
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
            {
                return
                {
                    .error = WriteError::null_file,
                    .bytesRequested = size,
                    .bytesWritten = 0
                };
            }

            if (size == 0)
            {
                return
                {
                    .error = WriteError::none,
                    .bytesRequested = 0,
                    .bytesWritten = 0
                };
            }

            if (data == nullptr)
            {
                return
                {
                    .error = WriteError::null_data,
                    .bytesRequested = size,
                    .bytesWritten = 0
                };
            }

            const std::size_t written = std::fwrite(data, 1, size, file);
            if (written == size)
            {
                return
                {
                    .error = WriteError::none,
                    .bytesRequested = size,
                    .bytesWritten = written
                };
            }

            return
            {
                .error = std::ferror(file) ? WriteError::io_error : WriteError::partial_write,
                .bytesRequested = size,
                .bytesWritten = written
            };
        }

    #if defined(_WIN32) && !defined(__MINGW32__)
        [[nodiscard]] inline WriteResult WriteBytesUnlocked(FILE* file, const void* data, std::size_t size) noexcept
        {
            return WriteBytesImpl(file, data, size);
        }

        [[nodiscard]] inline WriteResult WriteBytes(FILE* file, const void* data, std::size_t size) noexcept
        {
            if (file == nullptr)
            {
                return
                {
                    .error = WriteError::null_file,
                    .bytesRequested = size,
                    .bytesWritten = 0
                };
            }

            _lock_file(file);
            const WriteResult result = WriteBytesImpl(file, data, size);
            _unlock_file(file);
            return result;
        }
    #elif defined(__GLIBC__) || defined(__APPLE__) || defined(__linux__)
        [[nodiscard]] inline WriteResult WriteBytesUnlocked(FILE* file, const void* data, std::size_t size) noexcept
        {
            if (file == nullptr)
            {
                return
                {
                    .error = WriteError::null_file,
                    .bytesRequested = size,
                    .bytesWritten = 0
                };
            }

            if (size == 0)
            {
                return
                {
                    .error = WriteError::none,
                    .bytesRequested = 0,
                    .bytesWritten = 0
                };
            }

            if (data == nullptr)
            {
                return
                {
                    .error = WriteError::null_data,
                    .bytesRequested = size,
                    .bytesWritten = 0
                };
            }

            const std::size_t written = std::fwrite_unlocked(data, 1, size, file);
            if (written == size)
            {
                return
                {
                    .error = WriteError::none,
                    .bytesRequested = size,
                    .bytesWritten = written
                };
            }

            return
            {
                .error = std::ferror(file) ? WriteError::io_error : WriteError::partial_write,
                .bytesRequested = size,
                .bytesWritten = written
            };
        }

        [[nodiscard]] inline WriteResult WriteBytes(FILE* file, const void* data, std::size_t size) noexcept
        {
            if (file == nullptr)
            {
                return
                {
                    .error = WriteError::null_file,
                    .bytesRequested = size,
                    .bytesWritten = 0
                };
            }

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
    } // namespace detail

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
        {
            return
            {
                .error = WriteError::null_data,
                .bytesRequested = 0,
                .bytesWritten = 0
            };
        }

        return Write(file, value, std::strlen(value));
    }

    [[nodiscard]] inline WriteResult WriteUnlocked(FILE* file, const char* value) noexcept
    {
        if (value == nullptr)
        {
            return
            {
                .error = WriteError::null_data,
                .bytesRequested = 0,
                .bytesWritten = 0
            };
        }

        return WriteUnlocked(file, value, std::strlen(value));
    }

    template <std::size_t Size>
    [[nodiscard]] WriteResult Write(FILE* file, const char (&value)[Size]) noexcept
    {
        static_assert(Size > 0);
        return Write(file, value, Size - 1);
    }

    template <std::size_t Size>
    [[nodiscard]] WriteResult WriteUnlocked(FILE* file, const char (&value)[Size]) noexcept
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

    template <typename T>
    [[nodiscard]] WriteResult WriteObject(FILE* file, const T& value) noexcept
        requires (std::is_trivially_copyable_v<T>)
    {
        return Write(file, std::addressof(value), sizeof(T));
    }

    template <typename T>
    [[nodiscard]] WriteResult WriteObjectUnlocked(FILE* file, const T& value) noexcept
        requires (std::is_trivially_copyable_v<T>)
    {
        return WriteUnlocked(file, std::addressof(value), sizeof(T));
    }

    template <typename... TArgs>
    [[nodiscard]] WriteResult WriteMany(FILE* file, const TArgs&... args) noexcept
    {
        if (file == nullptr)
        {
            return
            {
                .error = WriteError::null_file,
                .bytesRequested = 0,
                .bytesWritten = 0
            };
        }

    #if defined(_WIN32) && !defined(__MINGW32__)
        _lock_file(file);
        auto unlock = [&]() noexcept { _unlock_file(file); };
    #elif defined(__GLIBC__) || defined(__APPLE__) || defined(__linux__)
        flockfile(file);
        auto unlock = [&]() noexcept { funlockfile(file); };
    #else
        auto unlock = []() noexcept {};
    #endif

        WriteResult finalResult
        {
            .error = WriteError::none,
            .bytesRequested = 0,
            .bytesWritten = 0
        };

        bool failed = false;

        auto writeOne = [&]<typename T0>(const T0& arg) noexcept
        {
            if (failed)
            {
                return;
            }

            using T = T0;
            static_assert(detail::IsWritableTextV<T>,
                "WriteMany only supports std::string, std::string_view, const char*, char*, and char arrays.");

            const WriteResult result = WriteUnlocked(file, arg);
            finalResult.bytesRequested += result.bytesRequested;
            finalResult.bytesWritten += result.bytesWritten;

            if (!result.Ok())
            {
                finalResult.error = result.error;
                failed = true;
            }
        };

        (writeOne(args), ...);
        unlock();
        return finalResult;
    }

    template <typename T>
    std::int32_t WriteValue(const T& value)
    {
        FILE* file = stdout;

        using U = std::decay_t<T>;

        WriteResult result{};

        if constexpr (std::is_same_v<U, bool>)
        {
            result = Write(file, value ? "true" : "false");
        }
        else if constexpr (std::is_same_v<U, const char*> ||
                           std::is_same_v<U, char*>)
        {
            result = Write(file, value != nullptr ? value : "(null)");
        }
        else if constexpr (std::is_same_v<U, std::string> ||
                           std::is_same_v<U, std::string_view>)
        {
            result = Write(file, value);
        }
        else if constexpr (std::is_integral_v<U>)
        {
            using Casted =
                std::conditional_t<std::is_same_v<U, std::int8_t>, std::int32_t,
                std::conditional_t<std::is_same_v<U, std::uint8_t>, std::uint32_t, U>>;

            char buffer[64];
            const auto v = static_cast<Casted>(value);

            auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), v);

            if (ec != std::errc{})
            {
                return -1;
            }

            result = Write(file, buffer, static_cast<std::size_t>(ptr - buffer));
        }
        else if constexpr (std::is_floating_point_v<U>)
        {
            char buffer[64];

            auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);

            if (ec != std::errc{})
            {
                return -1;
            }

            result = Write(file, buffer, static_cast<std::size_t>(ptr - buffer));
        }
        else
        {
            static_assert(sizeof(T) == 0,
                "WriteValue: Unsupported type");
        }

        return result.Ok() ? 0 : -1;
    }
}