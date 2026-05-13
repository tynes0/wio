#pragma once

#include <cstddef>
#include <cstdint>
#include <cinttypes>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
    #include <cwchar>
#endif

#include "exception.h"
#include "../src/detail/io_detail/io_helpers.h"

namespace wio::runtime::std_io
{
    namespace io = ::wio::runtime::detail::io_helpers;

#if defined(_WIN32)
    using NativeFileHandle = void*;
#else
    using NativeFileHandle = int;
#endif

    using NativeErrorCode = std::uint64_t;

    [[nodiscard]] NativeFileHandle InvalidNativeFileHandle() noexcept;

    enum class OpenMode : std::uint32_t
    {
        none            = 0,
        read            = 1u << 0u,
        write           = 1u << 1u,
        append          = 1u << 2u,
        create          = 1u << 3u,
        create_new      = 1u << 4u,
        truncate        = 1u << 5u,
        binary          = 1u << 6u,
        text            = 1u << 7u,
        share_read      = 1u << 8u,
        share_write     = 1u << 9u,
        share_delete    = 1u << 10u,
        sequential      = 1u << 11u,
        random_access   = 1u << 12u,
        write_through   = 1u << 13u,
        temporary       = 1u << 14u,
        delete_on_close = 1u << 15u,
        inheritable     = 1u << 16u
    };

    [[nodiscard]] constexpr OpenMode operator|(const OpenMode lhs, const OpenMode rhs) noexcept
    {
        return static_cast<OpenMode>(
            static_cast<std::uint32_t>(lhs) |
            static_cast<std::uint32_t>(rhs)
        );
    }

    [[nodiscard]] constexpr OpenMode operator&(const OpenMode lhs, const OpenMode rhs) noexcept
    {
        return static_cast<OpenMode>(
            static_cast<std::uint32_t>(lhs) &
            static_cast<std::uint32_t>(rhs)
        );
    }

    constexpr OpenMode& operator|=(OpenMode& lhs, const OpenMode rhs) noexcept
    {
        lhs = lhs | rhs;
        return lhs;
    }

    [[nodiscard]] constexpr bool HasFlag(const OpenMode value, const OpenMode flag) noexcept
    {
        return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0u;
    }

    inline constexpr OpenMode open_read =
        OpenMode::read |
        OpenMode::share_read |
        OpenMode::sequential;

    inline constexpr OpenMode open_read_binary =
        open_read |
        OpenMode::binary;

    inline constexpr OpenMode open_write =
        OpenMode::write |
        OpenMode::create |
        OpenMode::truncate |
        OpenMode::share_read;

    inline constexpr OpenMode open_write_binary =
        open_write |
        OpenMode::binary;

    inline constexpr OpenMode open_append =
        OpenMode::write |
        OpenMode::append |
        OpenMode::create |
        OpenMode::share_read;

    inline constexpr OpenMode open_append_binary =
        open_append |
        OpenMode::binary;

    inline constexpr OpenMode open_read_write_existing =
        OpenMode::read |
        OpenMode::write |
        OpenMode::share_read;

    inline constexpr OpenMode open_read_write_create =
        OpenMode::read |
        OpenMode::write |
        OpenMode::create |
        OpenMode::share_read;

    inline constexpr OpenMode open_read_write_truncate =
        OpenMode::read |
        OpenMode::write |
        OpenMode::create |
        OpenMode::truncate |
        OpenMode::share_read;

    enum class FileError : std::uint8_t
    {
        none = 0,
        null_path,
        empty_path,
        invalid_path,
        invalid_utf8,
        allocation_failed,
        invalid_argument,
        invalid_mode,
        unsupported_mode,
        already_open,
        not_open,
        open_failed,
        close_failed,
        sync_failed,
        seek_failed,
        tell_failed,
        size_failed
    };

    enum class SeekOrigin : std::uint8_t
    {
        begin = 0,
        current,
        end
    };

    [[nodiscard]] const char* ToString(FileError error) noexcept;
    [[nodiscard]] const char* ToString(io::ReadError error) noexcept;
    [[nodiscard]] const char* ToString(io::WriteError error) noexcept;

    class FileException final : public ::wio::runtime::RuntimeException
    {
    public:
        explicit FileException(
            FileError error,
            NativeErrorCode nativeError = 0,
            std::string context = {}
        );

        [[nodiscard]] FileError Error() const noexcept;
        [[nodiscard]] NativeErrorCode NativeError() const noexcept;

    private:
        FileError error_;
        NativeErrorCode nativeError_;
    };

    class FileReadException final : public ::wio::runtime::RuntimeException
    {
    public:
        explicit FileReadException(
            io::ReadError error,
            std::size_t bytesRequested,
            std::size_t bytesRead,
            NativeErrorCode nativeError = 0,
            std::string context = {}
        );

        [[nodiscard]] io::ReadError Error() const noexcept;
        [[nodiscard]] std::size_t BytesRequested() const noexcept;
        [[nodiscard]] std::size_t BytesRead() const noexcept;
        [[nodiscard]] NativeErrorCode NativeError() const noexcept;

    private:
        io::ReadError error_;
        std::size_t bytesRequested_;
        std::size_t bytesRead_;
        NativeErrorCode nativeError_;
    };

    class FileWriteException final : public ::wio::runtime::RuntimeException
    {
    public:
        explicit FileWriteException(
            io::WriteError error,
            std::size_t bytesRequested,
            std::size_t bytesWritten,
            NativeErrorCode nativeError = 0,
            std::string context = {}
        );

        [[nodiscard]] io::WriteError Error() const noexcept;
        [[nodiscard]] std::size_t BytesRequested() const noexcept;
        [[nodiscard]] std::size_t BytesWritten() const noexcept;
        [[nodiscard]] NativeErrorCode NativeError() const noexcept;

    private:
        io::WriteError error_;
        std::size_t bytesRequested_;
        std::size_t bytesWritten_;
        NativeErrorCode nativeError_;
    };

    struct File
    {
        NativeFileHandle data = InvalidNativeFileHandle();
        OpenMode mode = OpenMode::none;
    };

    [[nodiscard]] bool IsOpen(const File& file) noexcept;
    [[nodiscard]] bool IsReadable(const File& file) noexcept;
    [[nodiscard]] bool IsWritable(const File& file) noexcept;
    [[nodiscard]] bool IsAppendOnly(const File& file) noexcept;

    [[nodiscard]] File Open(const char* path, OpenMode mode);
    [[nodiscard]] File Open(std::string_view path, OpenMode mode);

#if defined(_WIN32)
    [[nodiscard]] File Open(const wchar_t* path, OpenMode mode);
    [[nodiscard]] File Open(std::wstring_view path, OpenMode mode);
#endif

    [[nodiscard]] File Attach(NativeFileHandle handle, OpenMode mode);
    void Reset(File& file, NativeFileHandle handle, OpenMode mode);
    [[nodiscard]] NativeFileHandle Release(File& file) noexcept;
    void Close(File& file);
    void Sync(File& file);

    void Seek(File& file, std::int64_t offset, SeekOrigin origin = SeekOrigin::begin);
    [[nodiscard]] std::int64_t Tell(const File& file);
    [[nodiscard]] std::uint64_t Size(const File& file);

    [[nodiscard]] std::string ReadSome(File& file, std::size_t maxSize);
    [[nodiscard]] std::vector<std::byte> ReadSomeBytes(File& file, std::size_t maxSize);

    [[nodiscard]] std::string Read(File& file, std::size_t size);
    [[nodiscard]] std::vector<std::byte> ReadBytes(File& file, std::size_t size);
    [[nodiscard]] char ReadChar(File& file);
    [[nodiscard]] std::string ReadCount(File& file, std::size_t count);
    [[nodiscard]] std::string ReadAll(File& file);
    [[nodiscard]] std::string ReadUntil(
        File& file,
        char delimiter,
        bool includeDelimiter = false,
        bool stopOnEof = true
    );
    [[nodiscard]] std::string ReadLine(File& file, bool trimCarriageReturn = true);
    [[nodiscard]] std::string ReadWord(File& file);

    [[nodiscard]] std::size_t Write(File& file, const void* data, std::size_t size);
    [[nodiscard]] std::size_t Write(File& file, std::span<const std::byte> bytes);
    [[nodiscard]] std::size_t Write(File& file, std::string_view value);
    [[nodiscard]] std::size_t Write(File& file, const std::string& value);
    [[nodiscard]] std::size_t Write(File& file, const char* value);

    template <std::size_t Size>
    [[nodiscard]] std::size_t Write(File& file, const char (&value)[Size])
    {
        static_assert(Size > 0);
        return Write(file, value, Size - 1);
    }

    [[nodiscard]] std::size_t WriteLine(File& file);
    [[nodiscard]] std::size_t WriteLine(File& file, std::string_view value);

    [[nodiscard]] inline std::uintptr_t InvalidNativeFileHandleValue() noexcept
    {
    #if defined(_WIN32)
        return reinterpret_cast<std::uintptr_t>(InvalidNativeFileHandle());
    #else
        return static_cast<std::uintptr_t>(InvalidNativeFileHandle());
    #endif
    }

    [[nodiscard]] inline File AttachRaw(const std::uintptr_t handle, const OpenMode mode)
    {
    #if defined(_WIN32)
        return Attach(reinterpret_cast<NativeFileHandle>(handle), mode);
    #else
        return Attach(static_cast<NativeFileHandle>(handle), mode);
    #endif
    }

    inline void ResetRaw(File& file, const std::uintptr_t handle, const OpenMode mode)
    {
    #if defined(_WIN32)
        Reset(file, reinterpret_cast<NativeFileHandle>(handle), mode);
    #else
        Reset(file, static_cast<NativeFileHandle>(handle), mode);
    #endif
    }

    [[nodiscard]] inline File OpenStringArg(std::string_view path, const OpenMode mode)
    {
        return Open(path, mode);
    }

    [[nodiscard]] inline std::size_t WriteStringArg(File& file, std::string_view value)
    {
        return Write(file, value);
    }

    [[nodiscard]] inline std::uintptr_t ReleaseRaw(File& file) noexcept
    {
    #if defined(_WIN32)
        return reinterpret_cast<std::uintptr_t>(Release(file));
    #else
        return static_cast<std::uintptr_t>(Release(file));
    #endif
    }

    [[nodiscard]] inline std::vector<unsigned char> ReadSomeByteValues(File& file, const std::size_t maxSize)
    {
        const auto bytes = ReadSomeBytes(file, maxSize);
        std::vector<unsigned char> result;
        result.reserve(bytes.size());
        for (const auto byteValue : bytes)
            result.push_back(static_cast<unsigned char>(byteValue));
        return result;
    }

    [[nodiscard]] inline std::vector<unsigned char> ReadByteValues(File& file, const std::size_t size)
    {
        const auto bytes = ReadBytes(file, size);
        std::vector<unsigned char> result;
        result.reserve(bytes.size());
        for (const auto byteValue : bytes)
            result.push_back(static_cast<unsigned char>(byteValue));
        return result;
    }

    [[nodiscard]] inline std::size_t WriteByteValues(File& file, std::span<const unsigned char> bytes)
    {
        if (bytes.empty())
            return 0;

        auto rawBytes = std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(bytes.data()),
            bytes.size()
        );
        return Write(file, rawBytes);
    }

    [[nodiscard]] inline std::size_t WriteByteValues(File& file, const std::vector<unsigned char>& bytes)
    {
        return WriteByteValues(file, std::span<const unsigned char>(bytes.data(), bytes.size()));
    }
}
