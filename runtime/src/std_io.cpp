#include "std_io.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif

namespace wio::runtime::std_io
{
    namespace
    {
#if defined(_WIN32)
        [[nodiscard]] HANDLE ToHandle(const NativeFileHandle handle) noexcept
        {
            return static_cast<HANDLE>(handle);
        }

        [[nodiscard]] NativeFileHandle ToNativeFileHandle(const HANDLE handle) noexcept
        {
            return static_cast<NativeFileHandle>(handle);
        }

        [[nodiscard]] NativeErrorCode LastNativeError() noexcept
        {
            return static_cast<NativeErrorCode>(::GetLastError());
        }
#else
        [[nodiscard]] NativeErrorCode LastNativeError() noexcept
        {
            return static_cast<NativeErrorCode>(errno);
        }
#endif

        [[nodiscard]] std::string BuildFileMessage(
            const FileError error,
            const NativeErrorCode nativeError,
            const std::string& context
        )
        {
            std::string message = "std_io file error: ";
            message += ToString(error);

            if (!context.empty())
            {
                message += " | context: ";
                message += context;
            }

            if (nativeError != 0)
            {
                message += " | native_error: ";
                message += std::to_string(nativeError);
            }

            return message;
        }

        [[nodiscard]] std::string BuildReadMessage(
            const io::ReadError error,
            const std::size_t bytesRequested,
            const std::size_t bytesRead,
            const NativeErrorCode nativeError,
            const std::string& context
        )
        {
            std::string message = "std_io read error: ";
            message += ToString(error);
            message += " | requested: ";
            message += std::to_string(bytesRequested);
            message += " | read: ";
            message += std::to_string(bytesRead);

            if (!context.empty())
            {
                message += " | context: ";
                message += context;
            }

            if (nativeError != 0)
            {
                message += " | native_error: ";
                message += std::to_string(nativeError);
            }

            return message;
        }

        [[nodiscard]] std::string BuildWriteMessage(
            const io::WriteError error,
            const std::size_t bytesRequested,
            const std::size_t bytesWritten,
            const NativeErrorCode nativeError,
            const std::string& context
        )
        {
            std::string message = "std_io write error: ";
            message += ToString(error);
            message += " | requested: ";
            message += std::to_string(bytesRequested);
            message += " | written: ";
            message += std::to_string(bytesWritten);

            if (!context.empty())
            {
                message += " | context: ";
                message += context;
            }

            if (nativeError != 0)
            {
                message += " | native_error: ";
                message += std::to_string(nativeError);
            }

            return message;
        }

        [[noreturn]] void ThrowFile(
            const FileError error,
            const NativeErrorCode nativeError = 0,
            std::string context = {}
        )
        {
            throw FileException(error, nativeError, std::move(context));
        }

        [[noreturn]] void ThrowRead(
            const io::ReadError error,
            const std::size_t bytesRequested,
            const std::size_t bytesRead,
            const NativeErrorCode nativeError = 0,
            std::string context = {}
        )
        {
            throw FileReadException(error, bytesRequested, bytesRead, nativeError, std::move(context));
        }

        [[noreturn]] void ThrowWrite(
            const io::WriteError error,
            const std::size_t bytesRequested,
            const std::size_t bytesWritten,
            const NativeErrorCode nativeError = 0,
            std::string context = {}
        )
        {
            throw FileWriteException(error, bytesRequested, bytesWritten, nativeError, std::move(context));
        }

        [[nodiscard]] bool IsSpace(const char ch) noexcept
        {
            return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
        }

        [[nodiscard]] bool IsModeValid(const OpenMode mode) noexcept
        {
            const bool read = HasFlag(mode, OpenMode::read);
            const bool write = HasFlag(mode, OpenMode::write);
            const bool append = HasFlag(mode, OpenMode::append);
            const bool create = HasFlag(mode, OpenMode::create);
            const bool createNew = HasFlag(mode, OpenMode::create_new);
            const bool truncate = HasFlag(mode, OpenMode::truncate);
            const bool binary = HasFlag(mode, OpenMode::binary);
            const bool text = HasFlag(mode, OpenMode::text);
            const bool sequential = HasFlag(mode, OpenMode::sequential);
            const bool randomAccess = HasFlag(mode, OpenMode::random_access);

            if (!read && !write)
                return false;

            if (append && !write)
                return false;

            if (truncate && !write)
                return false;

            if (createNew && !write)
                return false;

            if (createNew && create)
                return false;

            if (createNew && truncate)
                return false;

            if (binary && text)
                return false;

            if (sequential && randomAccess)
                return false;

            (void)create;
            return true;
        }

        void ValidateModeOrThrow(const OpenMode mode)
        {
            if (!IsModeValid(mode))
                ThrowFile(FileError::invalid_mode);
        }

        [[nodiscard]] File MakeAttachedFile(const NativeFileHandle handle, const OpenMode mode)
        {
#if defined(_WIN32)
            if (handle == nullptr || handle == InvalidNativeFileHandle())
#else
            if (handle < 0)
#endif
            {
                ThrowFile(FileError::invalid_argument);
            }

            ValidateModeOrThrow(mode);

            return {
                .data = handle,
                .mode = mode
            };
        }

#if defined(_WIN32)
        [[nodiscard]] std::wstring Utf8ToWide(const char* path)
        {
            if (path == nullptr)
                ThrowFile(FileError::null_path);

            if (path[0] == '\0')
                ThrowFile(FileError::empty_path);

            const int required = ::MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                path,
                -1,
                nullptr,
                0
            );

            if (required <= 0)
                ThrowFile(FileError::invalid_utf8, LastNativeError());

            std::wstring result;
            result.resize(static_cast<std::size_t>(required));

            const int written = ::MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                path,
                -1,
                result.data(),
                required
            );

            if (written <= 0)
                ThrowFile(FileError::invalid_utf8, LastNativeError());

            result.resize(static_cast<std::size_t>(written - 1));
            return result;
        }

        [[nodiscard]] std::wstring Utf8ViewToWide(const std::string_view path)
        {
            if (path.empty())
                ThrowFile(FileError::empty_path);

            const std::string ownedPath(path);
            return Utf8ToWide(ownedPath.c_str());
        }

        void ValidateWidePath(const wchar_t* path)
        {
            if (path == nullptr)
                ThrowFile(FileError::null_path);

            if (path[0] == L'\0')
                ThrowFile(FileError::empty_path);
        }

        [[nodiscard]] DWORD MakeDesiredAccess(const OpenMode mode) noexcept
        {
            DWORD access = 0;

            if (HasFlag(mode, OpenMode::read))
                access |= GENERIC_READ;

            if (HasFlag(mode, OpenMode::append))
            {
                access |= FILE_APPEND_DATA;
            }
            else if (HasFlag(mode, OpenMode::write))
            {
                access |= GENERIC_WRITE;
            }

            return access;
        }

        [[nodiscard]] DWORD MakeShareMode(const OpenMode mode) noexcept
        {
            DWORD shareMode = 0;

            if (HasFlag(mode, OpenMode::share_read))
                shareMode |= FILE_SHARE_READ;

            if (HasFlag(mode, OpenMode::share_write))
                shareMode |= FILE_SHARE_WRITE;

            if (HasFlag(mode, OpenMode::share_delete))
                shareMode |= FILE_SHARE_DELETE;

            return shareMode;
        }

        [[nodiscard]] DWORD MakeCreationDisposition(const OpenMode mode) noexcept
        {
            const bool create = HasFlag(mode, OpenMode::create);
            const bool createNew = HasFlag(mode, OpenMode::create_new);
            const bool truncate = HasFlag(mode, OpenMode::truncate);

            if (createNew)
                return CREATE_NEW;

            if (create && truncate)
                return CREATE_ALWAYS;

            if (create)
                return OPEN_ALWAYS;

            if (truncate)
                return TRUNCATE_EXISTING;

            return OPEN_EXISTING;
        }

        [[nodiscard]] DWORD MakeFlagsAndAttributes(const OpenMode mode) noexcept
        {
            DWORD flags = FILE_ATTRIBUTE_NORMAL;

            if (HasFlag(mode, OpenMode::sequential))
                flags |= FILE_FLAG_SEQUENTIAL_SCAN;

            if (HasFlag(mode, OpenMode::random_access))
                flags |= FILE_FLAG_RANDOM_ACCESS;

            if (HasFlag(mode, OpenMode::write_through))
                flags |= FILE_FLAG_WRITE_THROUGH;

            if (HasFlag(mode, OpenMode::temporary))
                flags |= FILE_ATTRIBUTE_TEMPORARY;

            if (HasFlag(mode, OpenMode::delete_on_close))
                flags |= FILE_FLAG_DELETE_ON_CLOSE;

            return flags;
        }

        [[nodiscard]] File OpenWideImpl(const wchar_t* path, const OpenMode mode)
        {
            ValidateWidePath(path);
            ValidateModeOrThrow(mode);

            SECURITY_ATTRIBUTES securityAttributes{};
            securityAttributes.nLength = sizeof(securityAttributes);
            securityAttributes.lpSecurityDescriptor = nullptr;
            securityAttributes.bInheritHandle = HasFlag(mode, OpenMode::inheritable) ? TRUE : FALSE;

            HANDLE handle = ::CreateFileW(
                path,
                MakeDesiredAccess(mode),
                MakeShareMode(mode),
                &securityAttributes,
                MakeCreationDisposition(mode),
                MakeFlagsAndAttributes(mode),
                nullptr
            );

            if (handle == INVALID_HANDLE_VALUE)
                ThrowFile(FileError::open_failed, LastNativeError());

            return MakeAttachedFile(ToNativeFileHandle(handle), mode);
        }
#else
        [[nodiscard]] int MakePosixFlags(const OpenMode mode) noexcept
        {
            int flags = 0;

            const bool read = HasFlag(mode, OpenMode::read);
            const bool write = HasFlag(mode, OpenMode::write);

            if (read && write)
                flags |= O_RDWR;
            else if (write)
                flags |= O_WRONLY;
            else
                flags |= O_RDONLY;

            if (HasFlag(mode, OpenMode::append))
                flags |= O_APPEND;

            if (HasFlag(mode, OpenMode::create))
                flags |= O_CREAT;

            if (HasFlag(mode, OpenMode::create_new))
                flags |= O_CREAT | O_EXCL;

            if (HasFlag(mode, OpenMode::truncate))
                flags |= O_TRUNC;

#if defined(O_CLOEXEC)
            if (!HasFlag(mode, OpenMode::inheritable))
                flags |= O_CLOEXEC;
#endif

#if defined(O_SYNC)
            if (HasFlag(mode, OpenMode::write_through))
                flags |= O_SYNC;
#endif

            return flags;
        }

        void SetCloseOnExecIfNeeded(const int fd, const OpenMode mode)
        {
#if !defined(O_CLOEXEC) && defined(FD_CLOEXEC)
            if (HasFlag(mode, OpenMode::inheritable))
                return;

            int flags = ::fcntl(fd, F_GETFD);

            if (flags < 0)
                ThrowFile(FileError::open_failed, LastNativeError(), "fcntl(F_GETFD)");

            flags |= FD_CLOEXEC;

            if (::fcntl(fd, F_SETFD, flags) != 0)
                ThrowFile(FileError::open_failed, LastNativeError(), "fcntl(F_SETFD)");
#else
            (void)fd;
            (void)mode;
#endif
        }

        void ApplyAccessPatternHints(const int fd, const OpenMode mode) noexcept
        {
#if defined(POSIX_FADV_SEQUENTIAL) && defined(POSIX_FADV_RANDOM)
            if (HasFlag(mode, OpenMode::sequential))
            {
                (void)::posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
                return;
            }

            if (HasFlag(mode, OpenMode::random_access))
            {
                (void)::posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM);
                return;
            }
#else
            (void)fd;
            (void)mode;
#endif
        }

        [[nodiscard]] File OpenImpl(const char* path, const OpenMode mode)
        {
            if (path == nullptr)
                ThrowFile(FileError::null_path);

            if (path[0] == '\0')
                ThrowFile(FileError::empty_path);

            ValidateModeOrThrow(mode);

            const int flags = MakePosixFlags(mode);

            int fd = -1;
            do
            {
                errno = 0;
                fd = ::open(path, flags, static_cast<mode_t>(0666));
            }
            while (fd < 0 && errno == EINTR);

            if (fd < 0)
                ThrowFile(FileError::open_failed, LastNativeError());

            try
            {
                SetCloseOnExecIfNeeded(fd, mode);

                if (HasFlag(mode, OpenMode::delete_on_close))
                {
                    if (::unlink(path) != 0)
                        ThrowFile(FileError::open_failed, LastNativeError(), "unlink(delete_on_close)");
                }
            }
            catch (...)
            {
                (void)::close(fd);
                throw;
            }

            ApplyAccessPatternHints(fd, mode);
            return MakeAttachedFile(fd, mode);
        }
#endif

        [[nodiscard]] std::size_t MaxNativeChunk() noexcept
        {
#if defined(_WIN32)
            return static_cast<std::size_t>(std::numeric_limits<DWORD>::max());
#else
            return static_cast<std::size_t>(std::numeric_limits<ssize_t>::max());
#endif
        }

        [[nodiscard]] std::size_t ReadSomeInto(File& file, void* outData, const std::size_t size)
        {
            if (!IsOpen(file))
                ThrowRead(io::ReadError::null_file, size, 0);

            if (size == 0)
                return 0;

            if (outData == nullptr)
                ThrowRead(io::ReadError::invalid_argument, size, 0);

            const std::size_t chunkSize = std::min(size, MaxNativeChunk());

#if defined(_WIN32)
            DWORD bytesRead = 0;
            const BOOL ok = ::ReadFile(
                ToHandle(file.data),
                outData,
                static_cast<DWORD>(chunkSize),
                &bytesRead,
                nullptr
            );

            if (!ok)
                ThrowRead(io::ReadError::io_error, size, bytesRead, LastNativeError());

            return static_cast<std::size_t>(bytesRead);
#else
            ssize_t result = 0;

            do
            {
                errno = 0;
                result = ::read(file.data, outData, chunkSize);
            }
            while (result < 0 && errno == EINTR);

            if (result < 0)
                ThrowRead(io::ReadError::io_error, size, 0, LastNativeError());

            return static_cast<std::size_t>(result);
#endif
        }

        [[nodiscard]] std::size_t ReadExactInto(File& file, void* outData, const std::size_t size)
        {
            if (!IsOpen(file))
                ThrowRead(io::ReadError::null_file, size, 0);

            if (size == 0)
                return 0;

            if (outData == nullptr)
                ThrowRead(io::ReadError::invalid_argument, size, 0);

            std::byte* cursor = static_cast<std::byte*>(outData);
            std::size_t totalRead = 0;

            while (totalRead < size)
            {
                std::size_t bytesRead = 0;

                try
                {
                    bytesRead = ReadSomeInto(file, cursor + totalRead, size - totalRead);
                }
                catch (const FileReadException& exception)
                {
                    ThrowRead(
                        exception.Error(),
                        size,
                        totalRead + exception.BytesRead(),
                        exception.NativeError()
                    );
                }

                if (bytesRead == 0)
                {
                    ThrowRead(
                        totalRead == 0 ? io::ReadError::eof : io::ReadError::partial_read,
                        size,
                        totalRead
                    );
                }

                totalRead += bytesRead;
            }

            return totalRead;
        }

        [[nodiscard]] std::size_t WriteSomeImpl(File& file, const void* data, const std::size_t size)
        {
            if (!IsOpen(file))
                ThrowWrite(io::WriteError::null_file, size, 0);

            if (size == 0)
                return 0;

            if (data == nullptr)
                ThrowWrite(io::WriteError::null_data, size, 0);

            const std::size_t chunkSize = std::min(size, MaxNativeChunk());

#if defined(_WIN32)
            DWORD bytesWritten = 0;
            const BOOL ok = ::WriteFile(
                ToHandle(file.data),
                data,
                static_cast<DWORD>(chunkSize),
                &bytesWritten,
                nullptr
            );

            if (!ok)
                ThrowWrite(io::WriteError::io_error, size, bytesWritten, LastNativeError());

            if (bytesWritten == 0)
                ThrowWrite(io::WriteError::partial_write, size, 0);

            return static_cast<std::size_t>(bytesWritten);
#else
            ssize_t result = 0;

            do
            {
                errno = 0;
                result = ::write(file.data, data, chunkSize);
            }
            while (result < 0 && errno == EINTR);

            if (result < 0)
                ThrowWrite(io::WriteError::io_error, size, 0, LastNativeError());

            if (result == 0)
                ThrowWrite(io::WriteError::partial_write, size, 0);

            return static_cast<std::size_t>(result);
#endif
        }
    }

    const char* ToString(const FileError error) noexcept
    {
        switch (error)
        {
        case FileError::none:
            return "none";
        case FileError::null_path:
            return "null_path";
        case FileError::empty_path:
            return "empty_path";
        case FileError::invalid_path:
            return "invalid_path";
        case FileError::invalid_utf8:
            return "invalid_utf8";
        case FileError::allocation_failed:
            return "allocation_failed";
        case FileError::invalid_argument:
            return "invalid_argument";
        case FileError::invalid_mode:
            return "invalid_mode";
        case FileError::unsupported_mode:
            return "unsupported_mode";
        case FileError::already_open:
            return "already_open";
        case FileError::not_open:
            return "not_open";
        case FileError::open_failed:
            return "open_failed";
        case FileError::close_failed:
            return "close_failed";
        case FileError::sync_failed:
            return "sync_failed";
        case FileError::seek_failed:
            return "seek_failed";
        case FileError::tell_failed:
            return "tell_failed";
        case FileError::size_failed:
            return "size_failed";
        }

        return "unknown_file_error";
    }

    const char* ToString(const io::ReadError error) noexcept
    {
        switch (error)
        {
        case io::ReadError::none:
            return "none";
        case io::ReadError::null_file:
            return "null_file";
        case io::ReadError::invalid_argument:
            return "invalid_argument";
        case io::ReadError::eof:
            return "eof";
        case io::ReadError::partial_read:
            return "partial_read";
        case io::ReadError::io_error:
            return "io_error";
        }

        return "unknown_read_error";
    }

    const char* ToString(const io::WriteError error) noexcept
    {
        switch (error)
        {
        case io::WriteError::none:
            return "none";
        case io::WriteError::null_file:
            return "null_file";
        case io::WriteError::null_data:
            return "null_data";
        case io::WriteError::partial_write:
            return "partial_write";
        case io::WriteError::io_error:
            return "io_error";
        }

        return "unknown_write_error";
    }

    FileException::FileException(
        const FileError error,
        const NativeErrorCode nativeError,
        std::string context
    )
        : RuntimeException(BuildFileMessage(error, nativeError, context)),
          error_(error),
          nativeError_(nativeError)
    {
    }

    FileError FileException::Error() const noexcept
    {
        return error_;
    }

    NativeErrorCode FileException::NativeError() const noexcept
    {
        return nativeError_;
    }

    FileReadException::FileReadException(
        const io::ReadError error,
        const std::size_t bytesRequested,
        const std::size_t bytesRead,
        const NativeErrorCode nativeError,
        std::string context
    )
        : RuntimeException(BuildReadMessage(error, bytesRequested, bytesRead, nativeError, context)),
          error_(error),
          bytesRequested_(bytesRequested),
          bytesRead_(bytesRead),
          nativeError_(nativeError)
    {
    }

    io::ReadError FileReadException::Error() const noexcept
    {
        return error_;
    }

    std::size_t FileReadException::BytesRequested() const noexcept
    {
        return bytesRequested_;
    }

    std::size_t FileReadException::BytesRead() const noexcept
    {
        return bytesRead_;
    }

    NativeErrorCode FileReadException::NativeError() const noexcept
    {
        return nativeError_;
    }

    FileWriteException::FileWriteException(
        const io::WriteError error,
        const std::size_t bytesRequested,
        const std::size_t bytesWritten,
        const NativeErrorCode nativeError,
        std::string context
    )
        : RuntimeException(BuildWriteMessage(error, bytesRequested, bytesWritten, nativeError, context)),
          error_(error),
          bytesRequested_(bytesRequested),
          bytesWritten_(bytesWritten),
          nativeError_(nativeError)
    {
    }

    io::WriteError FileWriteException::Error() const noexcept
    {
        return error_;
    }

    std::size_t FileWriteException::BytesRequested() const noexcept
    {
        return bytesRequested_;
    }

    std::size_t FileWriteException::BytesWritten() const noexcept
    {
        return bytesWritten_;
    }

    NativeErrorCode FileWriteException::NativeError() const noexcept
    {
        return nativeError_;
    }

    NativeFileHandle InvalidNativeFileHandle() noexcept
    {
#if defined(_WIN32)
        return reinterpret_cast<NativeFileHandle>(static_cast<std::intptr_t>(-1));
#else
        return -1;
#endif
    }

    bool IsOpen(const File& file) noexcept
    {
#if defined(_WIN32)
        return file.data != nullptr && file.data != InvalidNativeFileHandle();
#else
        return file.data >= 0;
#endif
    }

    bool IsReadable(const File& file) noexcept
    {
        return IsOpen(file) && HasFlag(file.mode, OpenMode::read);
    }

    bool IsWritable(const File& file) noexcept
    {
        return IsOpen(file) && HasFlag(file.mode, OpenMode::write);
    }

    bool IsAppendOnly(const File& file) noexcept
    {
        return IsOpen(file) && HasFlag(file.mode, OpenMode::append);
    }

    File Open(const char* path, const OpenMode mode)
    {
#if defined(_WIN32)
        const std::wstring widePath = Utf8ToWide(path);
        return OpenWideImpl(widePath.c_str(), mode);
#else
        return OpenImpl(path, mode);
#endif
    }

    File Open(const std::string_view path, const OpenMode mode)
    {
        if (path.empty())
            ThrowFile(FileError::empty_path);

#if defined(_WIN32)
        const std::wstring widePath = Utf8ViewToWide(path);
        return OpenWideImpl(widePath.c_str(), mode);
#else
        const std::string ownedPath(path);
        return OpenImpl(ownedPath.c_str(), mode);
#endif
    }

#if defined(_WIN32)
    File Open(const wchar_t* path, const OpenMode mode)
    {
        return OpenWideImpl(path, mode);
    }

    File Open(const std::wstring_view path, const OpenMode mode)
    {
        if (path.empty())
            ThrowFile(FileError::empty_path);

        const std::wstring ownedPath(path);
        return OpenWideImpl(ownedPath.c_str(), mode);
    }
#endif

    File Attach(const NativeFileHandle handle, const OpenMode mode)
    {
        return MakeAttachedFile(handle, mode);
    }

    void Reset(File& file, const NativeFileHandle handle, const OpenMode mode)
    {
        Close(file);
        file = Attach(handle, mode);
    }

    NativeFileHandle Release(File& file) noexcept
    {
        NativeFileHandle oldHandle = file.data;
        file.data = InvalidNativeFileHandle();
        file.mode = OpenMode::none;
        return oldHandle;
    }

    void Close(File& file)
    {
        if (!IsOpen(file))
            return;

        const NativeFileHandle handle = Release(file);

#if defined(_WIN32)
        if (::CloseHandle(ToHandle(handle)) == FALSE)
            ThrowFile(FileError::close_failed, LastNativeError());
#else
        errno = 0;

        if (::close(handle) != 0)
            ThrowFile(FileError::close_failed, LastNativeError());
#endif
    }

    void Sync(File& file)
    {
        if (!IsOpen(file))
            ThrowFile(FileError::not_open);

#if defined(_WIN32)
        if (::FlushFileBuffers(ToHandle(file.data)) == FALSE)
            ThrowFile(FileError::sync_failed, LastNativeError());
#else
        int result = 0;

        do
        {
            errno = 0;
            result = ::fsync(file.data);
        }
        while (result != 0 && errno == EINTR);

        if (result != 0)
            ThrowFile(FileError::sync_failed, LastNativeError());
#endif
    }

    void Seek(File& file, const std::int64_t offset, const SeekOrigin origin)
    {
        if (!IsOpen(file))
            ThrowFile(FileError::not_open);

#if defined(_WIN32)
        LARGE_INTEGER distance{};
        distance.QuadPart = static_cast<LONGLONG>(offset);

        DWORD moveMethod = FILE_BEGIN;

        switch (origin)
        {
        case SeekOrigin::begin:
            moveMethod = FILE_BEGIN;
            break;
        case SeekOrigin::current:
            moveMethod = FILE_CURRENT;
            break;
        case SeekOrigin::end:
            moveMethod = FILE_END;
            break;
        }

        if (::SetFilePointerEx(ToHandle(file.data), distance, nullptr, moveMethod) == FALSE)
            ThrowFile(FileError::seek_failed, LastNativeError());
#else
        int whence = SEEK_SET;

        switch (origin)
        {
        case SeekOrigin::begin:
            whence = SEEK_SET;
            break;
        case SeekOrigin::current:
            whence = SEEK_CUR;
            break;
        case SeekOrigin::end:
            whence = SEEK_END;
            break;
        }

        errno = 0;
        const off_t result = ::lseek(file.data, static_cast<off_t>(offset), whence);

        if (result == static_cast<off_t>(-1))
            ThrowFile(FileError::seek_failed, LastNativeError());
#endif
    }

    std::int64_t Tell(const File& file)
    {
        if (!IsOpen(file))
            ThrowFile(FileError::not_open);

#if defined(_WIN32)
        LARGE_INTEGER zero{};
        LARGE_INTEGER position{};

        if (::SetFilePointerEx(ToHandle(file.data), zero, &position, FILE_CURRENT) == FALSE)
            ThrowFile(FileError::tell_failed, LastNativeError());

        return static_cast<std::int64_t>(position.QuadPart);
#else
        errno = 0;
        const off_t position = ::lseek(file.data, 0, SEEK_CUR);

        if (position == static_cast<off_t>(-1))
            ThrowFile(FileError::tell_failed, LastNativeError());

        return static_cast<std::int64_t>(position);
#endif
    }

    std::uint64_t Size(const File& file)
    {
        if (!IsOpen(file))
            ThrowFile(FileError::not_open);

#if defined(_WIN32)
        LARGE_INTEGER size{};

        if (::GetFileSizeEx(ToHandle(file.data), &size) == FALSE)
            ThrowFile(FileError::size_failed, LastNativeError());

        return static_cast<std::uint64_t>(size.QuadPart);
#else
        struct stat statBuffer{};

        if (::fstat(file.data, &statBuffer) != 0)
            ThrowFile(FileError::size_failed, LastNativeError());

        return static_cast<std::uint64_t>(statBuffer.st_size);
#endif
    }

    std::string ReadSome(File& file, const std::size_t maxSize)
    {
        std::string result;

        if (maxSize == 0)
            return result;

        result.resize(maxSize);
        const std::size_t bytesRead = ReadSomeInto(file, result.data(), maxSize);
        result.resize(bytesRead);
        return result;
    }

    std::vector<std::byte> ReadSomeBytes(File& file, const std::size_t maxSize)
    {
        std::vector<std::byte> result;

        if (maxSize == 0)
            return result;

        result.resize(maxSize);
        const std::size_t bytesRead = ReadSomeInto(file, result.data(), maxSize);
        result.resize(bytesRead);
        return result;
    }

    std::string Read(File& file, const std::size_t size)
    {
        std::string result;

        if (size == 0)
            return result;

        result.resize(size);

        try
        {
            (void)ReadExactInto(file, result.data(), size);
        }
        catch (const FileReadException& exception)
        {
            result.resize(exception.BytesRead());
            throw;
        }

        return result;
    }

    std::vector<std::byte> ReadBytes(File& file, const std::size_t size)
    {
        std::vector<std::byte> result;

        if (size == 0)
            return result;

        result.resize(size);

        try
        {
            (void)ReadExactInto(file, result.data(), size);
        }
        catch (const FileReadException& exception)
        {
            result.resize(exception.BytesRead());
            throw;
        }

        return result;
    }

    char ReadChar(File& file)
    {
        char value = '\0';
        (void)ReadExactInto(file, &value, 1);
        return value;
    }

    std::string ReadCount(File& file, const std::size_t count)
    {
        return Read(file, count);
    }

    std::string ReadAll(File& file)
    {
        if (!IsOpen(file))
            ThrowRead(io::ReadError::null_file, 0, 0);

        std::string result;

        try
        {
            const std::int64_t position = Tell(file);
            const std::uint64_t fileSize = Size(file);

            if (position >= 0 && fileSize > static_cast<std::uint64_t>(position))
            {
                const std::uint64_t remaining = fileSize - static_cast<std::uint64_t>(position);

                if (remaining <= static_cast<std::uint64_t>(result.max_size()))
                    result.reserve(static_cast<std::size_t>(remaining));
            }
        }
        catch (const FileException&)
        {
            // Some handles are not seekable. Reserve is only an optimization, so ignore it.
        }

        char buffer[64 * 1024];

        while (true)
        {
            const std::size_t bytesRead = ReadSomeInto(file, buffer, sizeof(buffer));

            if (bytesRead == 0)
                return result;

            result.append(buffer, bytesRead);
        }
    }

    std::string ReadUntil(
        File& file,
        const char delimiter,
        const bool includeDelimiter,
        const bool stopOnEof
    )
    {
        if (!IsOpen(file))
            ThrowRead(io::ReadError::null_file, 0, 0);

        std::string result;
        char buffer[4096];
        std::size_t consumed = 0;

        while (true)
        {
            const std::size_t bytesRead = ReadSomeInto(file, buffer, sizeof(buffer));

            if (bytesRead == 0)
            {
                if (stopOnEof)
                    return result;

                ThrowRead(
                    result.empty() ? io::ReadError::eof : io::ReadError::partial_read,
                    0,
                    consumed
                );
            }

            const void* found = std::memchr(buffer, delimiter, bytesRead);

            if (found != nullptr)
            {
                const auto* foundChar = static_cast<const char*>(found);
                const std::size_t delimiterIndex = static_cast<std::size_t>(foundChar - buffer);
                const std::size_t appendCount = delimiterIndex + (includeDelimiter ? 1u : 0u);
                const std::size_t consumedFromChunk = delimiterIndex + 1u;

                result.append(buffer, appendCount);
                consumed += consumedFromChunk;

                const std::size_t unread = bytesRead - consumedFromChunk;

                if (unread > 0)
                {
                    try
                    {
                        Seek(file, -static_cast<std::int64_t>(unread), SeekOrigin::current);
                    }
                    catch (const FileException& exception)
                    {
                        ThrowRead(io::ReadError::io_error, 0, consumed, exception.NativeError());
                    }
                }

                return result;
            }

            result.append(buffer, bytesRead);
            consumed += bytesRead;
        }
    }

    std::string ReadLine(File& file, const bool trimCarriageReturn)
    {
        std::string result = ReadUntil(file, '\n', false, true);

        if (trimCarriageReturn && !result.empty() && result.back() == '\r')
            result.pop_back();

        return result;
    }

    std::string ReadWord(File& file)
    {
        if (!IsOpen(file))
            ThrowRead(io::ReadError::null_file, 0, 0);

        std::string result;
        char buffer[4096];
        bool foundWordStart = false;
        std::size_t consumed = 0;

        while (true)
        {
            const std::size_t bytesRead = ReadSomeInto(file, buffer, sizeof(buffer));

            if (bytesRead == 0)
                return result;

            std::size_t index = 0;

            if (!foundWordStart)
            {
                while (index < bytesRead && IsSpace(buffer[index]))
                    ++index;

                consumed += index;

                if (index < bytesRead)
                {
                    foundWordStart = true;
                }
                else
                {
                    continue;
                }
            }

            const std::size_t wordStart = index;

            while (index < bytesRead && !IsSpace(buffer[index]))
                ++index;

            result.append(buffer + wordStart, index - wordStart);
            consumed += index - wordStart;

            if (index < bytesRead && IsSpace(buffer[index]))
            {
                ++consumed;
                const std::size_t consumedFromChunk = index + 1u;
                const std::size_t unread = bytesRead - consumedFromChunk;

                if (unread > 0)
                {
                    try
                    {
                        Seek(file, -static_cast<std::int64_t>(unread), SeekOrigin::current);
                    }
                    catch (const FileException& exception)
                    {
                        ThrowRead(io::ReadError::io_error, 0, consumed, exception.NativeError());
                    }
                }

                return result;
            }
        }
    }

    std::size_t Write(File& file, const void* data, const std::size_t size)
    {
        if (!IsOpen(file))
            ThrowWrite(io::WriteError::null_file, size, 0);

        if (size == 0)
            return 0;

        if (data == nullptr)
            ThrowWrite(io::WriteError::null_data, size, 0);

        const auto* cursor = static_cast<const std::byte*>(data);
        std::size_t totalWritten = 0;

        while (totalWritten < size)
        {
            std::size_t bytesWritten = 0;

            try
            {
                bytesWritten = WriteSomeImpl(file, cursor + totalWritten, size - totalWritten);
            }
            catch (const FileWriteException& exception)
            {
                ThrowWrite(
                    exception.Error(),
                    size,
                    totalWritten + exception.BytesWritten(),
                    exception.NativeError()
                );
            }

            if (bytesWritten == 0)
                ThrowWrite(io::WriteError::partial_write, size, totalWritten);

            totalWritten += bytesWritten;
        }

        return totalWritten;
    }

    std::size_t Write(File& file, const std::span<const std::byte> bytes)
    {
        return Write(file, bytes.data(), bytes.size());
    }

    std::size_t Write(File& file, const std::string_view value)
    {
        return Write(file, value.data(), value.size());
    }

    std::size_t Write(File& file, const std::string& value)
    {
        return Write(file, value.data(), value.size());
    }

    std::size_t Write(File& file, const char* value)
    {
        if (value == nullptr)
            ThrowWrite(io::WriteError::null_data, 0, 0);

        return Write(file, value, std::strlen(value));
    }

    std::size_t WriteLine(File& file)
    {
#if defined(_WIN32)
        if (HasFlag(file.mode, OpenMode::text))
            return Write(file, "\r\n", 2);
#endif

        return Write(file, "\n", 1);
    }

    std::size_t WriteLine(File& file, const std::string_view value)
    {
        const std::size_t valueBytes = Write(file, value);
        const std::size_t lineBytes = WriteLine(file);
        return valueBytes + lineBytes;
    }
}
