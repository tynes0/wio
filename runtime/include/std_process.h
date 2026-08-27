#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wio::runtime::std_process
{
    enum class ProcessError : std::uint8_t
    {
        none = 0,
        empty_program = 1,
        invalid_working_directory = 2,
        launch_failed = 3,
        pipe_failed = 4,
        process_closed = 5,
        wait_failed = 6,
        terminate_failed = 7,
        cancelled = 8
    };

    [[nodiscard]] const char* ToString(ProcessError error) noexcept;
    [[nodiscard]] inline int ProcessErrorValue(const ProcessError error) noexcept
    {
        return static_cast<int>(error);
    }

    [[nodiscard]] std::string ExecutableSuffix();
    [[nodiscard]] std::string CurrentExecutablePath();
    [[nodiscard]] std::string SharedLibrarySuffix();
    [[nodiscard]] std::string StaticLibrarySuffix();
    [[nodiscard]] std::string WhichExecutable(std::string_view name);

    [[nodiscard]] bool TryRunResult(
        std::string_view program,
        const std::vector<std::string>& args,
        std::string_view workingDirectory,
        int& exitCode,
        ProcessError& error,
        int& nativeError,
        std::string& message) noexcept;
    [[nodiscard]] bool TryRunCapture(
        std::string_view program,
        const std::vector<std::string>& args,
        std::string_view workingDirectory,
        int& exitCode,
        std::string& output,
        ProcessError& error,
        int& nativeError,
        std::string& message) noexcept;

    [[nodiscard]] bool Spawn(
        std::string_view program,
        const std::vector<std::string>& args,
        std::string_view workingDirectory,
        void*& handle,
        ProcessError& error,
        int& nativeError,
        std::string& message) noexcept;
    [[nodiscard]] bool ProcessReadStdout(
        void* handle, std::size_t maximumBytes, std::string& bytes, bool& eof,
        ProcessError& error, int& nativeError, std::string& message) noexcept;
    [[nodiscard]] bool ProcessReadStderr(
        void* handle, std::size_t maximumBytes, std::string& bytes, bool& eof,
        ProcessError& error, int& nativeError, std::string& message) noexcept;
    [[nodiscard]] bool ProcessTryReadStdout(
        void* handle, std::size_t maximumBytes, std::string& bytes, bool& eof,
        ProcessError& error, int& nativeError, std::string& message) noexcept;
    [[nodiscard]] bool ProcessTryReadStderr(
        void* handle, std::size_t maximumBytes, std::string& bytes, bool& eof,
        ProcessError& error, int& nativeError, std::string& message) noexcept;
    [[nodiscard]] bool ProcessWriteStdin(
        void* handle, std::string_view bytes, std::size_t& written,
        ProcessError& error, int& nativeError, std::string& message) noexcept;
    [[nodiscard]] bool ProcessCloseStdin(
        void* handle, ProcessError& error, int& nativeError, std::string& message) noexcept;
    [[nodiscard]] bool ProcessWait(
        void* handle, int& exitCode,
        ProcessError& error, int& nativeError, std::string& message) noexcept;
    [[nodiscard]] bool ProcessIsRunning(
        void* handle, bool& running,
        ProcessError& error, int& nativeError, std::string& message) noexcept;
    [[nodiscard]] bool ProcessTerminate(
        void* handle, ProcessError& error, int& nativeError, std::string& message) noexcept;
    [[nodiscard]] bool ProcessRetain(void* handle, std::string& message) noexcept;
    [[nodiscard]] std::uint64_t LiveProcessCount() noexcept;
    void ProcessRelease(void* handle) noexcept;
    void ProcessClose(void* handle) noexcept;
}

namespace wio::runtime::std_environment
{
    [[nodiscard]] bool TryGet(std::string_view name, std::string& value) noexcept;
    [[nodiscard]] bool Set(std::string_view name, std::string_view value) noexcept;
    [[nodiscard]] bool Remove(std::string_view name) noexcept;
    [[nodiscard]] bool TryGetUser(std::string_view name, std::string& value) noexcept;
    [[nodiscard]] bool SetUser(std::string_view name, std::string_view value) noexcept;
    [[nodiscard]] bool RemoveUser(std::string_view name) noexcept;
    [[nodiscard]] bool UserPathContains(std::string_view entry) noexcept;
    [[nodiscard]] bool AddUserPath(std::string_view entry) noexcept;
    [[nodiscard]] bool RemoveUserPath(std::string_view entry) noexcept;
    [[nodiscard]] std::vector<std::string> DuplicateKeys();
    [[nodiscard]] std::string TemporaryDirectory();
    [[nodiscard]] std::string HomeDirectory();
    [[nodiscard]] std::string CacheDirectory();
    [[nodiscard]] std::string ConfigDirectory();
    [[nodiscard]] std::string DataDirectory();
    [[nodiscard]] std::string RuntimeDirectory();
    [[nodiscard]] std::string CurrentDirectory();
    [[nodiscard]] bool SetCurrentDirectory(std::string_view path) noexcept;
}

namespace wio::runtime::std_platform
{
    enum class OperatingSystem : std::uint8_t
    {
        unknown = 0,
        windows = 1,
        linux = 2,
        macos = 3,
        unix_like = 4
    };

    enum class Architecture : std::uint8_t
    {
        unknown = 0,
        x86 = 1,
        x64 = 2,
        arm32 = 3,
        arm64 = 4,
        wasm32 = 5,
        wasm64 = 6
    };

    [[nodiscard]] OperatingSystem CurrentOperatingSystem() noexcept;
    [[nodiscard]] Architecture CurrentArchitecture() noexcept;
    [[nodiscard]] const char* OperatingSystemName(OperatingSystem value) noexcept;
    [[nodiscard]] const char* ArchitectureName(Architecture value) noexcept;
    [[nodiscard]] std::uint32_t PointerBits() noexcept;
    [[nodiscard]] bool IsLittleEndian() noexcept;
    [[nodiscard]] std::uint32_t HardwareThreadCount() noexcept;
    [[nodiscard]] std::string PathListSeparator();
    [[nodiscard]] std::string NativeNewLine();
}
