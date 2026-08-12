#include "std_process.h"

#include <bit>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cwchar>
#include <cwctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(_WIN32)
    #include <windows.h>
    #ifdef SetCurrentDirectory
        #undef SetCurrentDirectory
    #endif
#elif defined(__APPLE__)
    #include <fcntl.h>
    #include <mach-o/dyld.h>
    #include <pthread.h>
    #include <signal.h>
    #include <sys/select.h>
    #include <sys/wait.h>
    #include <unistd.h>
#else
    #include <fcntl.h>
    #include <pthread.h>
    #include <signal.h>
    #include <sys/select.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

namespace wio::runtime::std_process
{
    namespace
    {
        std::atomic<std::uint64_t> liveProcessCount{0};

        struct ProcessHandle final
        {
            ProcessHandle() { liveProcessCount.fetch_add(1, std::memory_order_relaxed); }
            ~ProcessHandle() { liveProcessCount.fetch_sub(1, std::memory_order_relaxed); }

            std::atomic<std::size_t> references{1};
            std::mutex lifecycleMutex;
            std::mutex stdinMutex;
            std::mutex stdoutMutex;
            std::mutex stderrMutex;
            std::mutex waitMutex;
            bool closed = false;
            std::atomic_bool waited{false};
            std::atomic<int> exitCode{-1};
#if defined(_WIN32)
            HANDLE process = nullptr;
            HANDLE stdinWrite = nullptr;
            HANDLE stdoutRead = nullptr;
            HANDLE stderrRead = nullptr;
#else
            pid_t process = -1;
            int stdinWrite = -1;
            int stdoutRead = -1;
            int stderrRead = -1;
#endif
        };

        ProcessHandle* asProcess(void* handle) noexcept
        {
            return static_cast<ProcessHandle*>(handle);
        }

#if defined(_WIN32)
        void closePipe(HANDLE& handle) noexcept
        {
            if (handle)
            {
                CloseHandle(handle);
                handle = nullptr;
            }
        }
#else
        void closePipe(int& handle) noexcept
        {
            if (handle >= 0)
            {
                ::close(handle);
                handle = -1;
            }
        }

        class ScopedSigpipeBlock final
        {
        public:
            ScopedSigpipeBlock() noexcept
            {
                sigemptyset(&blocked_);
                sigaddset(&blocked_, SIGPIPE);
                active_ = pthread_sigmask(SIG_BLOCK, &blocked_, &previous_) == 0;
                previouslyBlocked_ = active_ && sigismember(&previous_, SIGPIPE) == 1;
            }

            ~ScopedSigpipeBlock()
            {
                if (!active_) return;
                if (!previouslyBlocked_)
                {
                    sigset_t pending{};
                    if (sigpending(&pending) == 0 && sigismember(&pending, SIGPIPE) == 1)
                    {
                        int consumedSignal = 0;
                        static_cast<void>(sigwait(&blocked_, &consumedSignal));
                    }
                }
                static_cast<void>(pthread_sigmask(SIG_SETMASK, &previous_, nullptr));
            }

        private:
            sigset_t blocked_{};
            sigset_t previous_{};
            bool active_ = false;
            bool previouslyBlocked_ = false;
        };
#endif

        void setProcessError(
            ProcessError& error, int& nativeError, std::string& message,
            const ProcessError value, const int native, std::string text)
        {
            error = value;
            nativeError = native;
            message = std::move(text);
        }

        bool retainProcess(void* handle, ProcessError& error, int& nativeError, std::string& message) noexcept
        {
            if (!handle)
            {
                setProcessError(error, nativeError, message, ProcessError::process_closed, 0, "process handle is null");
                return false;
            }
            auto* state = asProcess(handle);
            std::lock_guard lock(state->lifecycleMutex);
            if (state->closed)
            {
                setProcessError(error, nativeError, message, ProcessError::process_closed, 0, "process is closed");
                return false;
            }
            state->references.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        class ProcessLease final
        {
        public:
            explicit ProcessLease(void* handle) noexcept : handle_(handle) {}
            ProcessLease(const ProcessLease&) = delete;
            ProcessLease& operator=(const ProcessLease&) = delete;
            ~ProcessLease() { if (handle_) ProcessRelease(handle_); }
        private:
            void* handle_ = nullptr;
        };

        bool waitProcessState(ProcessHandle* state, int& exitCode, ProcessError& error,
                              int& nativeError, std::string& message) noexcept
        {
            std::lock_guard waitLock(state->waitMutex);
            if (state->waited.load(std::memory_order_acquire))
            {
                exitCode = state->exitCode.load(std::memory_order_acquire);
                return true;
            }
#if defined(_WIN32)
            const DWORD waitResult = WaitForSingleObject(state->process, INFINITE);
            if (waitResult != WAIT_OBJECT_0)
            {
                setProcessError(error, nativeError, message, ProcessError::wait_failed,
                    static_cast<int>(GetLastError()), "waiting for process failed");
                return false;
            }
            DWORD code = 0;
            if (!GetExitCodeProcess(state->process, &code))
            {
                setProcessError(error, nativeError, message, ProcessError::wait_failed,
                    static_cast<int>(GetLastError()), "reading process exit code failed");
                return false;
            }
            state->exitCode.store(static_cast<int>(code), std::memory_order_release);
#else
            int status = 0;
            pid_t waited = -1;
            do { waited = ::waitpid(state->process, &status, 0); }
            while (waited < 0 && errno == EINTR);
            if (waited < 0)
            {
                setProcessError(error, nativeError, message, ProcessError::wait_failed,
                    errno, "waiting for process failed");
                return false;
            }
            state->exitCode.store(WIFEXITED(status)
                ? WEXITSTATUS(status)
                : WIFSIGNALED(status) ? 128 + WTERMSIG(status) : status,
                std::memory_order_release);
#endif
            state->waited.store(true, std::memory_order_release);
            exitCode = state->exitCode.load(std::memory_order_acquire);
            return true;
        }

        bool processRunningState(ProcessHandle* state, bool& running, ProcessError& error,
                                 int& nativeError, std::string& message) noexcept
        {
            std::lock_guard waitLock(state->waitMutex);
            if (state->waited.load(std::memory_order_acquire))
            {
                running = false;
                return true;
            }
#if defined(_WIN32)
            DWORD code = 0;
            if (!GetExitCodeProcess(state->process, &code))
            {
                setProcessError(error, nativeError, message, ProcessError::wait_failed,
                    static_cast<int>(GetLastError()), "reading process status failed");
                return false;
            }
            running = code == STILL_ACTIVE;
            if (!running)
            {
                state->exitCode.store(static_cast<int>(code), std::memory_order_release);
                state->waited.store(true, std::memory_order_release);
            }
#else
            int status = 0;
            const pid_t result = ::waitpid(state->process, &status, WNOHANG);
            if (result < 0)
            {
                setProcessError(error, nativeError, message, ProcessError::wait_failed,
                    errno, "reading process status failed");
                return false;
            }
            running = result == 0;
            if (!running)
            {
                state->exitCode.store(WIFEXITED(status)
                    ? WEXITSTATUS(status)
                    : WIFSIGNALED(status) ? 128 + WTERMSIG(status) : status,
                    std::memory_order_release);
                state->waited.store(true, std::memory_order_release);
            }
#endif
            return true;
        }

        bool terminateProcessState(ProcessHandle* state, ProcessError& error,
                                   int& nativeError, std::string& message) noexcept
        {
            if (state->waited.load(std::memory_order_acquire))
                return true;
#if defined(_WIN32)
            DWORD code = 0;
            if (!GetExitCodeProcess(state->process, &code))
            {
                setProcessError(error, nativeError, message, ProcessError::terminate_failed,
                    static_cast<int>(GetLastError()), "reading process status before terminate failed");
                return false;
            }
            if (code != STILL_ACTIVE)
                return true;
            if (!TerminateProcess(state->process, 1))
            {
                setProcessError(error, nativeError, message, ProcessError::terminate_failed,
                    static_cast<int>(GetLastError()), "terminating process failed");
                return false;
            }
#else
            if (::kill(state->process, SIGKILL) != 0 && errno != ESRCH)
            {
                setProcessError(error, nativeError, message, ProcessError::terminate_failed,
                    errno, "terminating process failed");
                return false;
            }
#endif
            return true;
        }

#if defined(_WIN32)
        std::wstring widenProcessText(const std::string_view value)
        {
            if (value.empty()) return {};
            const int size = MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
            if (size <= 0) return {};
            std::wstring result(static_cast<std::size_t>(size), L'\0');
            if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                    static_cast<int>(value.size()), result.data(), size) <= 0)
                return {};
            return result;
        }

        std::wstring quoteWindowsProcessArgument(const std::string_view value)
        {
            const std::wstring wide = widenProcessText(value);
            if (!value.empty() && wide.empty()) return {};
            const bool needsQuotes = wide.empty() || std::ranges::any_of(wide, [](const wchar_t ch)
            {
                return ch == L' ' || ch == L'\t' || ch == L'\n' || ch == L'\v' || ch == L'"';
            });
            if (!needsQuotes) return wide;

            std::wstring result(1, L'"');
            std::size_t backslashes = 0;
            for (const wchar_t ch : wide)
            {
                if (ch == L'\\')
                {
                    ++backslashes;
                    continue;
                }
                if (ch == L'"')
                {
                    result.append(backslashes * 2 + 1, L'\\');
                    result.push_back(L'"');
                    backslashes = 0;
                    continue;
                }
                result.append(backslashes, L'\\');
                backslashes = 0;
                result.push_back(ch);
            }
            result.append(backslashes * 2, L'\\');
            result.push_back(L'"');
            return result;
        }

        std::wstring joinWindowsProcessCommand(
            const std::string_view program, const std::vector<std::string>& args)
        {
            std::wstring result = quoteWindowsProcessArgument(program);
            for (const auto& argument : args)
            {
                result.push_back(L' ');
                result += quoteWindowsProcessArgument(argument);
            }
            return result;
        }
#endif

        std::string quoteArgument(const std::string_view value)
        {
#if defined(_WIN32)
            if (value.empty())
                return "\"\"";

            bool needsQuotes = false;
            for (const char ch : value)
            {
                if (std::isspace(static_cast<unsigned char>(ch)) != 0 || ch == '"' || ch == '&' || ch == '(' || ch == ')' || ch == ';')
                {
                    needsQuotes = true;
                    break;
                }
            }

            if (!needsQuotes)
                return std::string(value);

            std::string result;
            result.reserve(value.size() + 2);
            result.push_back('"');
            for (const char ch : value)
            {
                if (ch == '"')
                    result += "\\\"";
                else
                    result.push_back(ch);
            }
            result.push_back('"');
            return result;
#else
            if (value.empty())
                return "''";

            std::string result;
            result.reserve(value.size() + 2);
            result.push_back('\'');
            for (const char ch : value)
            {
                if (ch == '\'')
                    result += "'\\''";
                else
                    result.push_back(ch);
            }
            result.push_back('\'');
            return result;
#endif
        }

        std::string joinCommand(const std::string_view program, const std::vector<std::string>& args)
        {
            std::ostringstream stream;
            stream << quoteArgument(program);
            for (const auto& arg : args)
                stream << ' ' << quoteArgument(arg);
            return stream.str();
        }

        class ScopedCurrentPath
        {
        public:
            explicit ScopedCurrentPath(const std::filesystem::path& nextPath, std::error_code& ec)
            {
                previousPath_ = std::filesystem::current_path(ec);
                if (ec)
                    return;

                std::filesystem::current_path(nextPath, ec);
                if (!ec)
                    active_ = true;
            }

            ~ScopedCurrentPath()
            {
                if (!active_)
                    return;

                std::error_code restoreError;
                std::filesystem::current_path(previousPath_, restoreError);
            }

        private:
            std::filesystem::path previousPath_;
            bool active_ = false;
        };
    }

    const char* ToString(const ProcessError error) noexcept
    {
        switch (error)
        {
        case ProcessError::none:
            return "none";
        case ProcessError::empty_program:
            return "empty_program";
        case ProcessError::invalid_working_directory:
            return "invalid_working_directory";
        case ProcessError::launch_failed:
            return "launch_failed";
        case ProcessError::pipe_failed:
            return "pipe_failed";
        case ProcessError::process_closed:
            return "process_closed";
        case ProcessError::wait_failed:
            return "wait_failed";
        case ProcessError::terminate_failed:
            return "terminate_failed";
        }

        return "launch_failed";
    }

    std::string ExecutableSuffix()
    {
#if defined(_WIN32)
        return ".exe";
#else
        return "";
#endif
    }

    std::string CurrentExecutablePath()
    {
#if defined(_WIN32)
        std::wstring buffer(MAX_PATH, L'\0');
        for (;;)
        {
            const DWORD copied = GetModuleFileNameW(
                nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (copied == 0)
                return {};
            if (copied < buffer.size())
            {
                buffer.resize(copied);
                return std::filesystem::path(buffer).lexically_normal().generic_string();
            }
            buffer.resize(buffer.size() * 2u);
        }
#elif defined(__APPLE__)
        std::uint32_t size = 0;
        if (_NSGetExecutablePath(nullptr, &size) != -1 || size == 0)
            return {};
        std::vector<char> buffer(size, '\0');
        if (_NSGetExecutablePath(buffer.data(), &size) != 0)
            return {};
        std::error_code ec;
        const auto resolved = std::filesystem::weakly_canonical(buffer.data(), ec);
        return ec ? std::filesystem::path(buffer.data()).lexically_normal().generic_string()
                  : resolved.generic_string();
#else
        std::vector<char> buffer(1024, '\0');
        for (;;)
        {
            const ssize_t copied = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1u);
            if (copied < 0)
                return {};
            if (static_cast<std::size_t>(copied) < buffer.size() - 1u)
            {
                buffer[static_cast<std::size_t>(copied)] = '\0';
                return std::filesystem::path(buffer.data()).lexically_normal().generic_string();
            }
            buffer.resize(buffer.size() * 2u);
        }
#endif
    }

    std::string SharedLibrarySuffix()
    {
#if defined(_WIN32)
        return ".dll";
#elif defined(__APPLE__)
        return ".dylib";
#else
        return ".so";
#endif
    }

    std::string StaticLibrarySuffix()
    {
        return ".a";
    }

    std::string WhichExecutable(const std::string_view name)
    {
        if (name.empty())
            return {};
        const std::filesystem::path requested{ std::string(name) };
        std::error_code ec;
        if (requested.has_parent_path() && std::filesystem::is_regular_file(requested, ec))
            return std::filesystem::absolute(requested, ec).lexically_normal().generic_string();

        const char* pathValue = std::getenv(
#if defined(_WIN32)
            "Path"
#else
            "PATH"
#endif
        );
        if (pathValue == nullptr)
            return {};

#if defined(_WIN32)
        constexpr char separator = ';';
        const std::vector<std::string> suffixes = requested.has_extension()
            ? std::vector<std::string>{ "" }
            : std::vector<std::string>{ "", ".exe", ".cmd", ".bat" };
#else
        constexpr char separator = ':';
        const std::vector<std::string> suffixes{ "" };
#endif
        std::stringstream paths(pathValue);
        std::string directory;
        while (std::getline(paths, directory, separator))
        {
            if (directory.empty())
                continue;
            for (const auto& suffix : suffixes)
            {
                ec.clear();
                const std::filesystem::path candidate =
                    std::filesystem::path(directory) / (std::string(name) + suffix);
                if (std::filesystem::is_regular_file(candidate, ec) && !ec)
                    return std::filesystem::absolute(candidate, ec).lexically_normal().generic_string();
            }
        }
        return {};
    }

    bool TryRunResult(
        const std::string_view program,
        const std::vector<std::string>& args,
        const std::string_view workingDirectory,
        int& exitCode,
        ProcessError& error,
        int& nativeError,
        std::string& message) noexcept
    {
        exitCode = -1;
        error = ProcessError::none;
        nativeError = 0;
        message.clear();

        if (program.empty())
        {
            error = ProcessError::empty_program;
            message = "process program cannot be empty";
            return false;
        }

        std::error_code pathError;
        std::optional<ScopedCurrentPath> pathGuard;
        if (!workingDirectory.empty())
        {
            const std::filesystem::path directoryPath = std::filesystem::path(std::string(workingDirectory));
            if (!std::filesystem::exists(directoryPath, pathError) ||
                !std::filesystem::is_directory(directoryPath, pathError))
            {
                error = ProcessError::invalid_working_directory;
                nativeError = static_cast<int>(pathError.value());
                message = "working directory does not exist: " + std::string(workingDirectory);
                return false;
            }

            pathGuard.emplace(directoryPath, pathError);
            if (pathError)
            {
                error = ProcessError::invalid_working_directory;
                nativeError = static_cast<int>(pathError.value());
                message = "could not switch to working directory: " + std::string(workingDirectory);
                return false;
            }
        }

        const std::string command = joinCommand(program, args);
        errno = 0;
        const int rawExitCode = std::system(command.c_str());
        if (rawExitCode == -1)
        {
            error = ProcessError::launch_failed;
            nativeError = errno;
            message = "process launch failed for: " + std::string(program);
            return false;
        }

#if defined(_WIN32)
        exitCode = rawExitCode;
#else
        if (WIFEXITED(rawExitCode))
            exitCode = WEXITSTATUS(rawExitCode);
        else
            exitCode = rawExitCode;
#endif

        return true;
    }

    bool TryRunCapture(
        const std::string_view program,
        const std::vector<std::string>& args,
        const std::string_view workingDirectory,
        int& exitCode,
        std::string& output,
        ProcessError& error,
        int& nativeError,
        std::string& message) noexcept
    {
        exitCode = -1; output.clear(); error = ProcessError::none; nativeError = 0; message.clear();
        if (program.empty()) { error = ProcessError::empty_program; message = "process program cannot be empty"; return false; }
        std::error_code pathError;
        std::optional<ScopedCurrentPath> pathGuard;
        if (!workingDirectory.empty())
        {
            const std::filesystem::path directoryPath{ std::string(workingDirectory) };
            if (!std::filesystem::is_directory(directoryPath, pathError))
            { error = ProcessError::invalid_working_directory; nativeError = pathError.value(); message = "working directory does not exist"; return false; }
            pathGuard.emplace(directoryPath, pathError);
            if (pathError) { error = ProcessError::invalid_working_directory; nativeError = pathError.value(); message = "could not switch working directory"; return false; }
        }
        const std::string command = joinCommand(program, args) + " 2>&1";
#if defined(_WIN32)
        FILE* pipe = _popen(command.c_str(), "r");
#else
        FILE* pipe = popen(command.c_str(), "r");
#endif
        if (!pipe) { error = ProcessError::launch_failed; nativeError = errno; message = "process capture launch failed"; return false; }
        char buffer[4096];
        while (std::fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != nullptr) output += buffer;
#if defined(_WIN32)
        exitCode = _pclose(pipe);
#else
        const int rawExitCode = pclose(pipe);
        exitCode = WIFEXITED(rawExitCode) ? WEXITSTATUS(rawExitCode) : rawExitCode;
#endif
        return true;
    }

    bool Spawn(
        const std::string_view program,
        const std::vector<std::string>& args,
        const std::string_view workingDirectory,
        void*& handle,
        ProcessError& error,
        int& nativeError,
        std::string& message) noexcept
    {
        handle = nullptr;
        error = ProcessError::none;
        nativeError = 0;
        message.clear();
        if (program.empty())
        {
            error = ProcessError::empty_program;
            message = "process program cannot be empty";
            return false;
        }
        if (!workingDirectory.empty())
        {
            std::error_code pathError;
            if (!std::filesystem::is_directory(std::filesystem::path(std::string(workingDirectory)), pathError))
            {
                setProcessError(error, nativeError, message, ProcessError::invalid_working_directory,
                    pathError.value(), "working directory does not exist: " + std::string(workingDirectory));
                return false;
            }
        }

#if defined(_WIN32)
        SECURITY_ATTRIBUTES attributes{};
        attributes.nLength = sizeof(attributes);
        attributes.bInheritHandle = TRUE;
        HANDLE childStdinRead = nullptr;
        HANDLE parentStdinWrite = nullptr;
        HANDLE parentStdoutRead = nullptr;
        HANDLE childStdoutWrite = nullptr;
        HANDLE parentStderrRead = nullptr;
        HANDLE childStderrWrite = nullptr;
        auto closeAll = [&]()
        {
            closePipe(childStdinRead); closePipe(parentStdinWrite);
            closePipe(parentStdoutRead); closePipe(childStdoutWrite);
            closePipe(parentStderrRead); closePipe(childStderrWrite);
        };
        if (!CreatePipe(&childStdinRead, &parentStdinWrite, &attributes, 0) ||
            !CreatePipe(&parentStdoutRead, &childStdoutWrite, &attributes, 0) ||
            !CreatePipe(&parentStderrRead, &childStderrWrite, &attributes, 0) ||
            !SetHandleInformation(parentStdinWrite, HANDLE_FLAG_INHERIT, 0) ||
            !SetHandleInformation(parentStdoutRead, HANDLE_FLAG_INHERIT, 0) ||
            !SetHandleInformation(parentStderrRead, HANDLE_FLAG_INHERIT, 0))
        {
            const int code = static_cast<int>(GetLastError());
            closeAll();
            setProcessError(error, nativeError, message, ProcessError::pipe_failed, code,
                "creating process pipes failed");
            return false;
        }

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = childStdinRead;
        startup.hStdOutput = childStdoutWrite;
        startup.hStdError = childStderrWrite;
        PROCESS_INFORMATION information{};
        const std::wstring application = widenProcessText(program);
        std::wstring commandLine = joinWindowsProcessCommand(program, args);
        const std::wstring directory = widenProcessText(workingDirectory);
        if (application.empty() || commandLine.empty() ||
            (!workingDirectory.empty() && directory.empty()))
        {
            closeAll();
            setProcessError(error, nativeError, message, ProcessError::launch_failed,
                ERROR_NO_UNICODE_TRANSLATION, "process path or argument is not valid UTF-8");
            return false;
        }
        const BOOL created = CreateProcessW(
            application.c_str(), commandLine.data(), nullptr, nullptr, TRUE, 0, nullptr,
            directory.empty() ? nullptr : directory.c_str(), &startup, &information);
        closePipe(childStdinRead);
        closePipe(childStdoutWrite);
        closePipe(childStderrWrite);
        if (!created)
        {
            const int code = static_cast<int>(GetLastError());
            closePipe(parentStdinWrite);
            closePipe(parentStdoutRead);
            closePipe(parentStderrRead);
            setProcessError(error, nativeError, message, ProcessError::launch_failed, code,
                "process launch failed for: " + std::string(program));
            return false;
        }
        CloseHandle(information.hThread);
        auto state = std::make_unique<ProcessHandle>();
        state->process = information.hProcess;
        state->stdinWrite = parentStdinWrite;
        state->stdoutRead = parentStdoutRead;
        state->stderrRead = parentStderrRead;
#else
        std::string programText(program);
        std::string workingDirectoryText(workingDirectory);
        std::vector<std::string> argumentStorage;
        argumentStorage.reserve(args.size() + 1);
        argumentStorage.push_back(programText);
        argumentStorage.insert(argumentStorage.end(), args.begin(), args.end());
        std::vector<char*> argumentPointers;
        argumentPointers.reserve(argumentStorage.size() + 1);
        for (auto& argument : argumentStorage) argumentPointers.push_back(argument.data());
        argumentPointers.push_back(nullptr);

        int stdinPipe[2]{-1, -1};
        int stdoutPipe[2]{-1, -1};
        int stderrPipe[2]{-1, -1};
        int launchPipe[2]{-1, -1};
        auto closePair = [](int (&pair)[2]) { closePipe(pair[0]); closePipe(pair[1]); };
        if (::pipe(stdinPipe) != 0 || ::pipe(stdoutPipe) != 0 ||
            ::pipe(stderrPipe) != 0 || ::pipe(launchPipe) != 0)
        {
            const int code = errno;
            closePair(stdinPipe); closePair(stdoutPipe); closePair(stderrPipe); closePair(launchPipe);
            setProcessError(error, nativeError, message, ProcessError::pipe_failed, code,
                "creating process pipes failed");
            return false;
        }
        if (::fcntl(launchPipe[1], F_SETFD, FD_CLOEXEC) != 0)
        {
            const int code = errno;
            closePair(stdinPipe); closePair(stdoutPipe); closePair(stderrPipe); closePair(launchPipe);
            setProcessError(error, nativeError, message, ProcessError::pipe_failed, code,
                "configuring process launch pipe failed");
            return false;
        }
        const pid_t child = ::fork();
        if (child < 0)
        {
            const int code = errno;
            closePair(stdinPipe); closePair(stdoutPipe); closePair(stderrPipe); closePair(launchPipe);
            setProcessError(error, nativeError, message, ProcessError::launch_failed, code,
                "fork failed for process: " + std::string(program));
            return false;
        }
        if (child == 0)
        {
            closePipe(stdinPipe[1]); closePipe(stdoutPipe[0]); closePipe(stderrPipe[0]); closePipe(launchPipe[0]);
            if (::dup2(stdinPipe[0], STDIN_FILENO) < 0 ||
                ::dup2(stdoutPipe[1], STDOUT_FILENO) < 0 ||
                ::dup2(stderrPipe[1], STDERR_FILENO) < 0 ||
                (!workingDirectoryText.empty() && ::chdir(workingDirectoryText.c_str()) != 0))
            {
                const int code = errno;
                static_cast<void>(::write(launchPipe[1], &code, sizeof(code)));
                _exit(127);
            }
            closePipe(stdinPipe[0]); closePipe(stdoutPipe[1]); closePipe(stderrPipe[1]);
            ::execvp(programText.c_str(), argumentPointers.data());
            const int code = errno;
            static_cast<void>(::write(launchPipe[1], &code, sizeof(code)));
            _exit(127);
        }
        closePipe(stdinPipe[0]); closePipe(stdoutPipe[1]); closePipe(stderrPipe[1]); closePipe(launchPipe[1]);
        int launchError = 0;
        ssize_t launchRead = -1;
        do { launchRead = ::read(launchPipe[0], &launchError, sizeof(launchError)); }
        while (launchRead < 0 && errno == EINTR);
        const int launchReadError = errno;
        closePipe(launchPipe[0]);
        auto abortSpawnedChild = [&]()
        {
            static_cast<void>(::kill(child, SIGKILL));
            int ignored = 0;
            pid_t waited = -1;
            do { waited = ::waitpid(child, &ignored, 0); }
            while (waited < 0 && errno == EINTR);
            closePipe(stdinPipe[1]); closePipe(stdoutPipe[0]); closePipe(stderrPipe[0]);
        };
        if (launchRead < 0)
        {
            abortSpawnedChild();
            setProcessError(error, nativeError, message, ProcessError::launch_failed,
                launchReadError, "reading process launch status failed");
            return false;
        }
        if (launchRead > 0)
        {
            abortSpawnedChild();
            setProcessError(error, nativeError, message, ProcessError::launch_failed, launchError,
                "process exec failed for: " + std::string(program));
            return false;
        }
        const int stdoutFlags = ::fcntl(stdoutPipe[0], F_GETFL);
        const int stderrFlags = ::fcntl(stderrPipe[0], F_GETFL);
        if (stdoutFlags < 0 || stderrFlags < 0 ||
            ::fcntl(stdoutPipe[0], F_SETFL, stdoutFlags | O_NONBLOCK) != 0 ||
            ::fcntl(stderrPipe[0], F_SETFL, stderrFlags | O_NONBLOCK) != 0)
        {
            const int code = errno;
            abortSpawnedChild();
            setProcessError(error, nativeError, message, ProcessError::pipe_failed, code,
                "configuring non-blocking process pipes failed");
            return false;
        }
        auto state = std::make_unique<ProcessHandle>();
        state->process = child;
        state->stdinWrite = stdinPipe[1];
        state->stdoutRead = stdoutPipe[0];
        state->stderrRead = stderrPipe[0];
#endif
        handle = state.release();
        return true;
    }

    namespace
    {
        bool readProcessPipe(void* handle, const bool standardError, const bool waitForData,
                             const std::size_t maximumBytes,
                             std::string& bytes, bool& eof, ProcessError& error,
                             int& nativeError, std::string& message) noexcept
        {
            bytes.clear(); eof = false; error = ProcessError::none; nativeError = 0; message.clear();
            if (!retainProcess(handle, error, nativeError, message)) return false;
            ProcessLease lease(handle);
            auto* state = asProcess(handle);
            std::unique_lock pipeLock(standardError ? state->stderrMutex : state->stdoutMutex);
            const std::size_t requested = std::max<std::size_t>(
                1, std::min<std::size_t>(maximumBytes, 1u << 20u));
            while (true)
            {
                {
                    std::lock_guard lifecycleLock(state->lifecycleMutex);
                    if (state->closed)
                    {
                        setProcessError(error, nativeError, message, ProcessError::process_closed, 0, "process is closed");
                        return false;
                    }
                }
#if defined(_WIN32)
                HANDLE pipe = standardError ? state->stderrRead : state->stdoutRead;
                DWORD available = 0;
                if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr))
                {
                    const DWORD code = GetLastError();
                    if (code == ERROR_BROKEN_PIPE)
                    {
                        eof = true;
                        return true;
                    }
                    setProcessError(error, nativeError, message, ProcessError::pipe_failed,
                        static_cast<int>(code), "reading process pipe status failed");
                    return false;
                }
                if (available == 0)
                {
                    bool running = false;
                    if (!processRunningState(state, running, error, nativeError, message)) return false;
                    if (!running)
                    {
                        // The process handle may signal just before the final
                        // pipe bytes become visible to PeekNamedPipe. A final
                        // read after exit drains those bytes or observes the
                        // broken-pipe EOF without blocking on a live writer.
                        std::vector<char> finalBuffer(requested);
                        DWORD finalRead = 0;
                        if (ReadFile(pipe, finalBuffer.data(), static_cast<DWORD>(finalBuffer.size()), &finalRead, nullptr))
                        {
                            if (finalRead == 0) { eof = true; return true; }
                            bytes.assign(finalBuffer.data(), finalRead);
                            return true;
                        }
                        const DWORD finalError = GetLastError();
                        if (finalError == ERROR_BROKEN_PIPE) { eof = true; return true; }
                        setProcessError(error, nativeError, message, ProcessError::pipe_failed,
                            static_cast<int>(finalError), "draining process pipe failed");
                        return false;
                    }
                    if (!waitForData) return true;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                std::vector<char> buffer(std::min<std::size_t>(requested, available));
                DWORD read = 0;
                if (!ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr))
                {
                    const DWORD code = GetLastError();
                    if (code == ERROR_BROKEN_PIPE) { eof = true; return true; }
                    setProcessError(error, nativeError, message, ProcessError::pipe_failed,
                        static_cast<int>(code), "reading process pipe failed");
                    return false;
                }
                bytes.assign(buffer.data(), read);
                return true;
#else
                const int pipe = standardError ? state->stderrRead : state->stdoutRead;
                fd_set readable; FD_ZERO(&readable); FD_SET(pipe, &readable);
                timeval timeout{0, waitForData ? 50'000 : 0};
                const int ready = ::select(pipe + 1, &readable, nullptr, nullptr, &timeout);
                if (ready < 0 && errno != EINTR)
                {
                    setProcessError(error, nativeError, message, ProcessError::pipe_failed, errno,
                        "waiting for process pipe failed");
                    return false;
                }
                if (ready == 0 && !waitForData) return true;
                if (ready <= 0) continue;
                std::vector<char> buffer(requested);
                const ssize_t read = ::read(pipe, buffer.data(), buffer.size());
                if (read > 0) { bytes.assign(buffer.data(), static_cast<std::size_t>(read)); return true; }
                if (read == 0) { eof = true; return true; }
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
                setProcessError(error, nativeError, message, ProcessError::pipe_failed, errno,
                    "reading process pipe failed");
                return false;
#endif
            }
        }
    }

    bool ProcessReadStdout(void* handle, const std::size_t maximumBytes, std::string& bytes, bool& eof,
                           ProcessError& error, int& nativeError, std::string& message) noexcept
    { return readProcessPipe(handle, false, true, maximumBytes, bytes, eof, error, nativeError, message); }

    bool ProcessReadStderr(void* handle, const std::size_t maximumBytes, std::string& bytes, bool& eof,
                           ProcessError& error, int& nativeError, std::string& message) noexcept
    { return readProcessPipe(handle, true, true, maximumBytes, bytes, eof, error, nativeError, message); }

    bool ProcessTryReadStdout(void* handle, const std::size_t maximumBytes, std::string& bytes, bool& eof,
                              ProcessError& error, int& nativeError, std::string& message) noexcept
    { return readProcessPipe(handle, false, false, maximumBytes, bytes, eof, error, nativeError, message); }

    bool ProcessTryReadStderr(void* handle, const std::size_t maximumBytes, std::string& bytes, bool& eof,
                              ProcessError& error, int& nativeError, std::string& message) noexcept
    { return readProcessPipe(handle, true, false, maximumBytes, bytes, eof, error, nativeError, message); }

    bool ProcessWriteStdin(void* handle, const std::string_view bytes, std::size_t& written,
                           ProcessError& error, int& nativeError, std::string& message) noexcept
    {
        written = 0; error = ProcessError::none; nativeError = 0; message.clear();
        if (!retainProcess(handle, error, nativeError, message)) return false;
        ProcessLease lease(handle);
        auto* state = asProcess(handle);
        std::lock_guard writeLock(state->stdinMutex);
#if defined(_WIN32)
        if (!state->stdinWrite)
#else
        if (state->stdinWrite < 0)
#endif
        {
            setProcessError(error, nativeError, message, ProcessError::pipe_failed, 0, "process stdin is closed");
            return false;
        }
        while (written < bytes.size())
        {
#if defined(_WIN32)
            DWORD count = 0;
            if (!WriteFile(state->stdinWrite, bytes.data() + written,
                    static_cast<DWORD>(std::min<std::size_t>(bytes.size() - written, 1u << 20u)), &count, nullptr))
            {
                setProcessError(error, nativeError, message, ProcessError::pipe_failed,
                    static_cast<int>(GetLastError()), "writing process stdin failed");
                return false;
            }
#else
            ScopedSigpipeBlock sigpipeGuard;
            const ssize_t count = ::write(state->stdinWrite, bytes.data() + written, bytes.size() - written);
            if (count < 0)
            {
                if (errno == EINTR) continue;
                setProcessError(error, nativeError, message, ProcessError::pipe_failed, errno,
                    "writing process stdin failed");
                return false;
            }
#endif
            written += static_cast<std::size_t>(count);
        }
        return true;
    }

    bool ProcessCloseStdin(void* handle, ProcessError& error, int& nativeError, std::string& message) noexcept
    {
        error = ProcessError::none; nativeError = 0; message.clear();
        if (!retainProcess(handle, error, nativeError, message)) return false;
        ProcessLease lease(handle);
        auto* state = asProcess(handle);
        std::lock_guard writeLock(state->stdinMutex);
        closePipe(state->stdinWrite);
        return true;
    }

    bool ProcessWait(void* handle, int& exitCode, ProcessError& error, int& nativeError, std::string& message) noexcept
    {
        exitCode = -1; error = ProcessError::none; nativeError = 0; message.clear();
        if (!retainProcess(handle, error, nativeError, message)) return false;
        ProcessLease lease(handle);
        return waitProcessState(asProcess(handle), exitCode, error, nativeError, message);
    }

    bool ProcessIsRunning(void* handle, bool& running, ProcessError& error, int& nativeError, std::string& message) noexcept
    {
        running = false; error = ProcessError::none; nativeError = 0; message.clear();
        if (!retainProcess(handle, error, nativeError, message)) return false;
        ProcessLease lease(handle);
        return processRunningState(asProcess(handle), running, error, nativeError, message);
    }

    bool ProcessTerminate(void* handle, ProcessError& error, int& nativeError, std::string& message) noexcept
    {
        error = ProcessError::none; nativeError = 0; message.clear();
        if (!retainProcess(handle, error, nativeError, message)) return false;
        ProcessLease lease(handle);
        return terminateProcessState(asProcess(handle), error, nativeError, message);
    }

    bool ProcessRetain(void* handle, std::string& message) noexcept
    {
        ProcessError error = ProcessError::none;
        int nativeError = 0;
        return retainProcess(handle, error, nativeError, message);
    }

    std::uint64_t LiveProcessCount() noexcept
    {
        return liveProcessCount.load(std::memory_order_acquire);
    }

    void ProcessRelease(void* handle) noexcept
    {
        if (!handle) return;
        auto* state = asProcess(handle);
        if (state->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
            delete state;
    }

    void ProcessClose(void* handle) noexcept
    {
        if (!handle) return;
        auto* state = asProcess(handle);
        {
            std::lock_guard lifecycleLock(state->lifecycleMutex);
            if (state->closed) return;
            state->closed = true;
        }
        ProcessError error = ProcessError::none;
        int nativeError = 0;
        std::string message;
        static_cast<void>(terminateProcessState(state, error, nativeError, message));
        int exitCode = -1;
        error = ProcessError::none; nativeError = 0; message.clear();
        static_cast<void>(waitProcessState(state, exitCode, error, nativeError, message));
        {
            std::lock_guard lock(state->stdinMutex); closePipe(state->stdinWrite);
        }
        {
            std::lock_guard lock(state->stdoutMutex); closePipe(state->stdoutRead);
        }
        {
            std::lock_guard lock(state->stderrMutex); closePipe(state->stderrRead);
        }
#if defined(_WIN32)
        if (state->process) { CloseHandle(state->process); state->process = nullptr; }
#endif
        ProcessRelease(state);
    }
}

namespace wio::runtime::std_platform
{
    OperatingSystem CurrentOperatingSystem() noexcept
    {
#if defined(_WIN32)
        return OperatingSystem::windows;
#elif defined(__APPLE__) && defined(__MACH__)
        return OperatingSystem::macos;
#elif defined(__linux__)
        return OperatingSystem::linux;
#elif defined(__unix__)
        return OperatingSystem::unix_like;
#else
        return OperatingSystem::unknown;
#endif
    }

    Architecture CurrentArchitecture() noexcept
    {
#if defined(__wasm64__)
        return Architecture::wasm64;
#elif defined(__wasm32__)
        return Architecture::wasm32;
#elif defined(_M_ARM64) || defined(__aarch64__)
        return Architecture::arm64;
#elif defined(_M_ARM) || defined(__arm__)
        return Architecture::arm32;
#elif defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
        return Architecture::x64;
#elif defined(_M_IX86) || defined(__i386__)
        return Architecture::x86;
#else
        return Architecture::unknown;
#endif
    }

    const char* OperatingSystemName(const OperatingSystem value) noexcept
    {
        switch (value)
        {
        case OperatingSystem::windows: return "windows";
        case OperatingSystem::linux: return "linux";
        case OperatingSystem::macos: return "macos";
        case OperatingSystem::unix_like: return "unix";
        case OperatingSystem::unknown: break;
        }
        return "unknown";
    }

    const char* ArchitectureName(const Architecture value) noexcept
    {
        switch (value)
        {
        case Architecture::x86: return "x86";
        case Architecture::x64: return "x64";
        case Architecture::arm32: return "arm32";
        case Architecture::arm64: return "arm64";
        case Architecture::wasm32: return "wasm32";
        case Architecture::wasm64: return "wasm64";
        case Architecture::unknown: break;
        }
        return "unknown";
    }

    std::uint32_t PointerBits() noexcept
    {
        return static_cast<std::uint32_t>(sizeof(void*) * 8u);
    }

    bool IsLittleEndian() noexcept
    {
        return std::endian::native == std::endian::little;
    }

    std::uint32_t HardwareThreadCount() noexcept
    {
        return std::thread::hardware_concurrency();
    }

    std::string PathListSeparator()
    {
#if defined(_WIN32)
        return ";";
#else
        return ":";
#endif
    }

    std::string NativeNewLine()
    {
#if defined(_WIN32)
        return "\r\n";
#else
        return "\n";
#endif
    }
}

namespace wio::runtime::std_environment
{
    namespace
    {
#if defined(_WIN32)
        std::wstring widen(const std::string_view value)
        {
            if (value.empty())
                return {};
            const int size = MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
            if (size <= 0)
                return {};
            std::wstring result(static_cast<std::size_t>(size), L'\0');
            if (MultiByteToWideChar(
                    CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                    result.data(), size) <= 0)
            {
                return {};
            }
            return result;
        }

        std::string narrow(const std::wstring_view value)
        {
            if (value.empty())
                return {};
            const int size = WideCharToMultiByte(
                CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                nullptr, 0, nullptr, nullptr);
            if (size <= 0)
                return {};
            std::string result(static_cast<std::size_t>(size), '\0');
            if (WideCharToMultiByte(
                    CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                    result.data(), size, nullptr, nullptr) <= 0)
            {
                return {};
            }
            return result;
        }

        bool openUserEnvironment(HKEY& key, const REGSAM access, const bool create) noexcept
        {
            key = nullptr;
            if (create)
            {
                DWORD disposition = 0;
                return RegCreateKeyExW(
                    HKEY_CURRENT_USER, L"Environment", 0, nullptr, 0, access, nullptr,
                    &key, &disposition) == ERROR_SUCCESS;
            }
            return RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, access, &key) == ERROR_SUCCESS;
        }

        bool readRegistryValue(HKEY key, const std::wstring& name, std::wstring& value) noexcept
        {
            value.clear();
            DWORD type = 0;
            DWORD bytes = 0;
            LONG status = RegQueryValueExW(key, name.c_str(), nullptr, &type, nullptr, &bytes);
            if (status != ERROR_SUCCESS || bytes == 0 ||
                (type != REG_SZ && type != REG_EXPAND_SZ))
            {
                return false;
            }
            std::wstring buffer(bytes / sizeof(wchar_t), L'\0');
            status = RegQueryValueExW(
                key, name.c_str(), nullptr, &type,
                reinterpret_cast<BYTE*>(buffer.data()), &bytes);
            if (status != ERROR_SUCCESS)
                return false;
            while (!buffer.empty() && buffer.back() == L'\0')
                buffer.pop_back();
            value = std::move(buffer);
            return true;
        }

        bool writeRegistryValue(
            HKEY key, const std::wstring& name, const std::wstring& value) noexcept
        {
            const DWORD bytes = static_cast<DWORD>((value.size() + 1u) * sizeof(wchar_t));
            return RegSetValueExW(
                key, name.c_str(), 0, REG_EXPAND_SZ,
                reinterpret_cast<const BYTE*>(value.c_str()), bytes) == ERROR_SUCCESS;
        }

        std::wstring normalizePath(std::wstring value)
        {
            std::replace(value.begin(), value.end(), L'/', L'\\');
            while (!value.empty() && (value.back() == L'\\' || value.back() == L'/'))
                value.pop_back();
            std::transform(value.begin(), value.end(), value.begin(), [](const wchar_t ch)
            {
                return static_cast<wchar_t>(std::towlower(ch));
            });
            return value;
        }

        std::vector<std::wstring> splitUserPath(const std::wstring& value)
        {
            std::vector<std::wstring> entries;
            std::wstringstream stream(value);
            std::wstring entry;
            while (std::getline(stream, entry, L';'))
            {
                const auto first = entry.find_first_not_of(L" \t");
                const auto last = entry.find_last_not_of(L" \t");
                if (first != std::wstring::npos)
                    entries.push_back(entry.substr(first, last - first + 1u));
            }
            return entries;
        }

        void broadcastEnvironmentChange() noexcept
        {
            SendMessageTimeoutW(
                HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                reinterpret_cast<LPARAM>(L"Environment"),
                SMTO_ABORTIFHUNG, 5000, nullptr);
        }
#else
        constexpr std::string_view ProfileMarkerBegin = "# >>> wio environment >>>";
        constexpr std::string_view ProfileMarkerEnd = "# <<< wio environment <<<";

        bool validVariableName(const std::string_view name) noexcept
        {
            if (name.empty() ||
                !(std::isalpha(static_cast<unsigned char>(name.front())) != 0 ||
                  name.front() == '_'))
            {
                return false;
            }
            return std::all_of(name.begin() + 1, name.end(), [](const char ch)
            {
                return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
            });
        }

        std::filesystem::path profilePath()
        {
            const char* home = std::getenv("HOME");
            return home == nullptr || *home == '\0'
                ? std::filesystem::path{}
                : std::filesystem::path(home) / ".profile";
        }

        std::string readFile(const std::filesystem::path& path)
        {
            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
                return {};
            std::ostringstream stream;
            stream << input.rdbuf();
            return stream.str();
        }

        std::string shellQuote(const std::string_view value)
        {
            std::string result = "'";
            for (const char ch : value)
            {
                if (ch == '\'')
                    result += "'\\''";
                else
                    result.push_back(ch);
            }
            result.push_back('\'');
            return result;
        }

        bool shellUnquote(const std::string_view value, std::string& result)
        {
            result.clear();
            if (value.size() < 2u || value.front() != '\'' || value.back() != '\'')
                return false;
            for (std::size_t index = 1u; index + 1u < value.size();)
            {
                if (index + 3u < value.size() && value.substr(index, 4u) == "'\\''")
                {
                    result.push_back('\'');
                    index += 4u;
                }
                else
                {
                    result.push_back(value[index]);
                    ++index;
                }
            }
            return true;
        }

        struct ProfileParts
        {
            std::string prefix;
            std::vector<std::string> lines;
            std::string suffix;
        };

        ProfileParts parseProfile(const std::string& content)
        {
            ProfileParts parts;
            const std::size_t begin = content.find(ProfileMarkerBegin);
            if (begin == std::string::npos)
            {
                parts.prefix = content;
                return parts;
            }
            parts.prefix = content.substr(0u, begin);
            const std::size_t bodyStart = begin + ProfileMarkerBegin.size();
            const std::size_t end = content.find(ProfileMarkerEnd, bodyStart);
            const std::string body = content.substr(
                bodyStart, end == std::string::npos ? std::string::npos : end - bodyStart);
            std::istringstream lines(body);
            std::string line;
            while (std::getline(lines, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (!line.empty())
                    parts.lines.push_back(line);
            }
            if (end != std::string::npos)
            {
                std::size_t suffixStart = end + ProfileMarkerEnd.size();
                if (suffixStart < content.size() && content[suffixStart] == '\r')
                    ++suffixStart;
                if (suffixStart < content.size() && content[suffixStart] == '\n')
                    ++suffixStart;
                parts.suffix = content.substr(suffixStart);
            }
            return parts;
        }

        bool writeProfile(ProfileParts parts)
        {
            const auto path = profilePath();
            if (path.empty())
                return false;
            std::ostringstream output;
            output << parts.prefix;
            if (!parts.prefix.empty() && parts.prefix.back() != '\n')
                output << '\n';
            if (!parts.lines.empty())
            {
                output << ProfileMarkerBegin << '\n';
                for (const auto& line : parts.lines)
                    output << line << '\n';
                output << ProfileMarkerEnd << '\n';
            }
            output << parts.suffix;
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            const std::string content = output.str();
            file.write(content.data(), static_cast<std::streamsize>(content.size()));
            return file.good();
        }

        std::string normalizedPosixPath(std::string value)
        {
            while (value.size() > 1u && value.back() == '/')
                value.pop_back();
            return value;
        }
#endif
    }

    bool TryGet(const std::string_view name, std::string& value) noexcept
    {
        value.clear();
        if (name.empty())
            return false;

        const std::string key(name);
#if defined(_WIN32)
        char* buffer = nullptr;
        std::size_t length = 0;
        if (_dupenv_s(&buffer, &length, key.c_str()) != 0 || buffer == nullptr)
            return false;
        value.assign(buffer);
        std::free(buffer);
        return true;
#else
        const char* result = std::getenv(key.c_str());
        if (result == nullptr)
            return false;
        value.assign(result);
        return true;
#endif
    }

    bool Set(const std::string_view name, const std::string_view value) noexcept
    {
        if (name.empty())
            return false;
        const std::string key(name);
        const std::string text(value);
#if defined(_WIN32)
        return _putenv_s(key.c_str(), text.c_str()) == 0;
#else
        return setenv(key.c_str(), text.c_str(), 1) == 0;
#endif
    }

    bool Remove(const std::string_view name) noexcept
    {
        if (name.empty())
            return false;
        const std::string key(name);
#if defined(_WIN32)
        return _putenv_s(key.c_str(), "") == 0;
#else
        return unsetenv(key.c_str()) == 0;
#endif
    }

    bool TryGetUser(const std::string_view name, std::string& value) noexcept
    {
        value.clear();
        if (name.empty())
            return false;
#if defined(_WIN32)
        const std::wstring wideName = widen(name);
        if (wideName.empty())
            return false;
        HKEY key = nullptr;
        if (!openUserEnvironment(key, KEY_QUERY_VALUE, false))
            return false;
        std::wstring wideValue;
        const bool found = readRegistryValue(key, wideName, wideValue);
        RegCloseKey(key);
        if (!found)
            return false;
        value = narrow(wideValue);
        return true;
#else
        if (!validVariableName(name))
            return false;
        const ProfileParts parts = parseProfile(readFile(profilePath()));
        const std::string prefix = "export " + std::string(name) + "=";
        for (const auto& line : parts.lines)
        {
            if (line.starts_with(prefix))
                return shellUnquote(std::string_view(line).substr(prefix.size()), value);
        }
        return false;
#endif
    }

    bool SetUser(const std::string_view name, const std::string_view value) noexcept
    {
        if (name.empty())
            return false;
#if defined(_WIN32)
        const std::wstring wideName = widen(name);
        const std::wstring wideValue = widen(value);
        if (wideName.empty() || (!value.empty() && wideValue.empty()))
            return false;
        HKEY key = nullptr;
        if (!openUserEnvironment(key, KEY_SET_VALUE, true))
            return false;
        const bool written = writeRegistryValue(key, wideName, wideValue);
        RegCloseKey(key);
        if (written)
            broadcastEnvironmentChange();
        return written;
#else
        if (!validVariableName(name))
            return false;
        ProfileParts parts = parseProfile(readFile(profilePath()));
        const std::string prefix = "export " + std::string(name) + "=";
        parts.lines.erase(
            std::remove_if(parts.lines.begin(), parts.lines.end(), [&](const std::string& line)
            {
                return line.starts_with(prefix);
            }),
            parts.lines.end());
        parts.lines.push_back(prefix + shellQuote(value));
        return writeProfile(std::move(parts));
#endif
    }

    bool RemoveUser(const std::string_view name) noexcept
    {
        if (name.empty())
            return false;
#if defined(_WIN32)
        const std::wstring wideName = widen(name);
        if (wideName.empty())
            return false;
        HKEY key = nullptr;
        if (!openUserEnvironment(key, KEY_SET_VALUE, false))
            return true;
        const LONG status = RegDeleteValueW(key, wideName.c_str());
        RegCloseKey(key);
        const bool removed = status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
        if (removed)
            broadcastEnvironmentChange();
        return removed;
#else
        if (!validVariableName(name))
            return false;
        ProfileParts parts = parseProfile(readFile(profilePath()));
        const std::string prefix = "export " + std::string(name) + "=";
        parts.lines.erase(
            std::remove_if(parts.lines.begin(), parts.lines.end(), [&](const std::string& line)
            {
                return line.starts_with(prefix);
            }),
            parts.lines.end());
        return writeProfile(std::move(parts));
#endif
    }

    bool UserPathContains(const std::string_view entry) noexcept
    {
        if (entry.empty())
            return false;
#if defined(_WIN32)
        const std::wstring candidate = normalizePath(widen(entry));
        HKEY key = nullptr;
        if (candidate.empty() || !openUserEnvironment(key, KEY_QUERY_VALUE, false))
            return false;
        std::wstring pathValue;
        const bool found = readRegistryValue(key, L"Path", pathValue);
        RegCloseKey(key);
        if (!found)
            return false;
        const auto entries = splitUserPath(pathValue);
        return std::any_of(entries.begin(), entries.end(), [&](const std::wstring& current)
        {
            return normalizePath(current) == candidate;
        });
#else
        const std::string candidate = normalizedPosixPath(std::string(entry));
        const ProfileParts parts = parseProfile(readFile(profilePath()));
        for (const auto& line : parts.lines)
        {
            constexpr std::string_view prefix = "export PATH=";
            constexpr std::string_view suffix = ":$PATH";
            if (!line.starts_with(prefix) || !line.ends_with(suffix))
                continue;
            const std::string_view encoded(line.data() + prefix.size(),
                line.size() - prefix.size() - suffix.size());
            std::string decoded;
            if (shellUnquote(encoded, decoded) && normalizedPosixPath(decoded) == candidate)
                return true;
        }
        return false;
#endif
    }

    bool AddUserPath(const std::string_view entry) noexcept
    {
        if (entry.empty() || UserPathContains(entry))
            return !entry.empty();
#if defined(_WIN32)
        const std::wstring wideEntry = widen(entry);
        if (wideEntry.empty())
            return false;
        HKEY key = nullptr;
        if (!openUserEnvironment(key, KEY_QUERY_VALUE | KEY_SET_VALUE, true))
            return false;
        std::wstring pathValue;
        readRegistryValue(key, L"Path", pathValue);
        const std::wstring updated = pathValue.empty() ? wideEntry : wideEntry + L";" + pathValue;
        const bool written = writeRegistryValue(key, L"Path", updated);
        RegCloseKey(key);
        if (written)
            broadcastEnvironmentChange();
        return written;
#else
        ProfileParts parts = parseProfile(readFile(profilePath()));
        parts.lines.push_back("export PATH=" + shellQuote(entry) + ":$PATH");
        return writeProfile(std::move(parts));
#endif
    }

    bool RemoveUserPath(const std::string_view entry) noexcept
    {
        if (entry.empty())
            return false;
#if defined(_WIN32)
        const std::wstring candidate = normalizePath(widen(entry));
        HKEY key = nullptr;
        if (candidate.empty() || !openUserEnvironment(key, KEY_QUERY_VALUE | KEY_SET_VALUE, false))
            return true;
        std::wstring pathValue;
        if (!readRegistryValue(key, L"Path", pathValue))
        {
            RegCloseKey(key);
            return true;
        }
        const auto entries = splitUserPath(pathValue);
        std::wstring updated;
        for (const auto& current : entries)
        {
            if (normalizePath(current) == candidate)
                continue;
            if (!updated.empty())
                updated.push_back(L';');
            updated += current;
        }
        bool written = false;
        if (updated.empty())
        {
            const LONG status = RegDeleteValueW(key, L"Path");
            written = status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
        }
        else
        {
            written = writeRegistryValue(key, L"Path", updated);
        }
        RegCloseKey(key);
        if (written)
            broadcastEnvironmentChange();
        return written;
#else
        const std::string candidate = normalizedPosixPath(std::string(entry));
        ProfileParts parts = parseProfile(readFile(profilePath()));
        parts.lines.erase(
            std::remove_if(parts.lines.begin(), parts.lines.end(), [&](const std::string& line)
            {
                constexpr std::string_view prefix = "export PATH=";
                constexpr std::string_view suffix = ":$PATH";
                if (!line.starts_with(prefix) || !line.ends_with(suffix))
                    return false;
                const std::string_view encoded(line.data() + prefix.size(),
                    line.size() - prefix.size() - suffix.size());
                std::string decoded;
                return shellUnquote(encoded, decoded) &&
                    normalizedPosixPath(decoded) == candidate;
            }),
            parts.lines.end());
        return writeProfile(std::move(parts));
#endif
    }

    std::vector<std::string> DuplicateKeys()
    {
        std::vector<std::string> result;
#if defined(_WIN32)
        LPWCH environment = GetEnvironmentStringsW();
        if (environment == nullptr)
            return result;
        std::unordered_set<std::wstring> seen;
        std::unordered_set<std::wstring> duplicates;
        for (LPCWCH cursor = environment; *cursor != L'\0'; cursor += std::wcslen(cursor) + 1u)
        {
            const std::wstring_view entry(cursor);
            const std::size_t equals = entry.find(L'=');
            if (equals == std::wstring_view::npos || equals == 0u)
                continue;
            std::wstring key(entry.substr(0u, equals));
            std::transform(key.begin(), key.end(), key.begin(), [](const wchar_t ch)
            {
                return static_cast<wchar_t>(std::towlower(ch));
            });
            if (!seen.insert(key).second)
                duplicates.insert(std::move(key));
        }
        FreeEnvironmentStringsW(environment);
        for (const auto& key : duplicates)
            result.push_back(narrow(key));
        std::sort(result.begin(), result.end());
#endif
        return result;
    }

    std::string TemporaryDirectory()
    {
        std::error_code ec;
        const auto path = std::filesystem::temp_directory_path(ec);
        return ec ? std::string{} : path.lexically_normal().generic_string();
    }

    std::string HomeDirectory()
    {
        std::string value;
#if defined(_WIN32)
        if (TryGet("USERPROFILE", value))
            return std::filesystem::path(value).lexically_normal().generic_string();
        std::string drive;
        std::string homePath;
        if (TryGet("HOMEDRIVE", drive) && TryGet("HOMEPATH", homePath))
            return std::filesystem::path(drive + homePath).lexically_normal().generic_string();
#else
        if (TryGet("HOME", value))
            return std::filesystem::path(value).lexically_normal().generic_string();
#endif
        return {};
    }

    std::string CacheDirectory()
    {
        std::string value;
#if defined(_WIN32)
        if (TryGet("LOCALAPPDATA", value))
            return std::filesystem::path(value).lexically_normal().generic_string();
        const std::string home = HomeDirectory();
        if (!home.empty())
            return (std::filesystem::path(home) / "AppData" / "Local").lexically_normal().generic_string();
#else
        if (TryGet("XDG_CACHE_HOME", value))
            return std::filesystem::path(value).lexically_normal().generic_string();
        const std::string home = HomeDirectory();
        if (!home.empty())
            return (std::filesystem::path(home) / ".cache").lexically_normal().generic_string();
#endif
        return TemporaryDirectory();
    }

    std::string ConfigDirectory()
    {
        std::string value;
#if defined(_WIN32)
        if (TryGet("APPDATA", value)) return std::filesystem::path(value).lexically_normal().generic_string();
#else
        if (TryGet("XDG_CONFIG_HOME", value)) return std::filesystem::path(value).lexically_normal().generic_string();
        const std::string home = HomeDirectory();
        if (!home.empty()) return (std::filesystem::path(home) / ".config").generic_string();
#endif
        return HomeDirectory();
    }

    std::string DataDirectory()
    {
        std::string value;
#if defined(_WIN32)
        if (TryGet("LOCALAPPDATA", value)) return std::filesystem::path(value).lexically_normal().generic_string();
#else
        if (TryGet("XDG_DATA_HOME", value)) return std::filesystem::path(value).lexically_normal().generic_string();
        const std::string home = HomeDirectory();
        if (!home.empty()) return (std::filesystem::path(home) / ".local" / "share").generic_string();
#endif
        return HomeDirectory();
    }

    std::string RuntimeDirectory()
    {
        std::string value;
#if !defined(_WIN32)
        if (TryGet("XDG_RUNTIME_DIR", value)) return std::filesystem::path(value).lexically_normal().generic_string();
#endif
        return TemporaryDirectory();
    }

    std::string CurrentDirectory()
    {
        std::error_code error;
        const auto path = std::filesystem::current_path(error);
        return error ? std::string{} : path.lexically_normal().generic_string();
    }

    bool SetCurrentDirectory(const std::string_view path) noexcept
    {
        std::error_code error;
        std::filesystem::current_path(std::filesystem::path(std::string(path)), error);
        return !error;
    }
}
