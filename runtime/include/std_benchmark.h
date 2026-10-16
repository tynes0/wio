#pragma once

#include "benchmark_memory.h"
#include "std_hash.h"
#include "std_process.h"
#include "std_serialization.h"
#ifdef WIO_BENCHMARK_ALLOCATIONS
#include "wio_benchmark_allocations.h"
#endif
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <stdexcept>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#include <io.h>
#ifdef interface
#undef interface
#endif
#ifdef Yield
#undef Yield
#endif
#ifdef CopyFile
#undef CopyFile
#endif
#ifdef MoveFile
#undef MoveFile
#endif
#ifdef GetCurrentDirectory
#undef GetCurrentDirectory
#endif
#ifdef SetCurrentDirectory
#undef SetCurrentDirectory
#endif
#elif defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <csignal>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sched.h>
#endif
#ifdef __APPLE__
#include <mach/mach.h>
#endif
#endif

namespace wio::runtime::benchmark
{
    inline std::atomic<std::uint64_t> sink{0};
    inline thread_local volatile std::uint64_t inputU64 = 0;
    inline thread_local volatile std::int64_t inputI64 = 0;
    inline thread_local volatile double inputF64 = 0;

    inline std::uint64_t InputU64(std::uint64_t value) noexcept
    {
        inputU64 = value;
        return inputU64;
    }

    inline std::int64_t InputI64(std::int64_t value) noexcept
    {
        inputI64 = value;
        return inputI64;
    }

    inline double InputF64(double value) noexcept
    {
        inputF64 = value;
        return inputF64;
    }

    inline void Consume(std::uint64_t value) noexcept
    {
        sink.fetch_xor(value, std::memory_order_relaxed);
    }

    inline std::int64_t Now() noexcept
    {
        std::atomic_signal_fence(std::memory_order_seq_cst);
        const auto value =
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count();
        std::atomic_signal_fence(std::memory_order_seq_cst);
        return value;
    }

    struct MemorySnapshot
    {
        std::uint64_t residentBytes = 0;
        std::uint64_t peakResidentBytes = 0;
        std::uint64_t privateBytes = 0;
        bool residentAvailable = false;
        bool peakAvailable = false;
        bool privateAvailable = false;
    };

    inline MemorySnapshot Memory() noexcept
    {
        MemorySnapshot result;
#if defined(_WIN32)
        PROCESS_MEMORY_COUNTERS_EX counters{};
        if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                                 sizeof(counters)))
        {
            result = {counters.WorkingSetSize, counters.PeakWorkingSetSize, counters.PrivateUsage, true, true, true};
        }
#elif defined(__linux__)
        if (auto* file = std::fopen("/proc/self/status", "r"))
        {
            char line[256];
            unsigned long long value;
            while (std::fgets(line, sizeof(line), file))
            {
                if (std::sscanf(line, "VmRSS: %llu kB", &value) == 1)
                {
                    result.residentBytes = value * 1024;
                    result.residentAvailable = true;
                }
                if (std::sscanf(line, "VmHWM: %llu kB", &value) == 1)
                {
                    result.peakResidentBytes = value * 1024;
                    result.peakAvailable = true;
                }
            }
            std::fclose(file);
        }
#elif defined(__APPLE__)
        mach_task_basic_info_data_t info{};
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) ==
            KERN_SUCCESS)
        {
            result.residentBytes = info.resident_size;
            result.residentAvailable = true;
        }
        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) == 0)
        {
            result.peakResidentBytes = usage.ru_maxrss;
            result.peakAvailable = true;
        }
#endif
        return result;
    }

    inline std::string Quote(const std::string& text)
    {
        constexpr char hex[] = "0123456789abcdef";
        std::string result = "\"";
        for (unsigned char ch : text)
        {
            if (ch == '"' || ch == '\\')
            {
                result += '\\';
                result += static_cast<char>(ch);
            }
            else if (ch < 32 || ch >= 127)
            {
                // Byte-preserving JSON representation, including arbitrary
                // invalid UTF-8 from foreign tools. stdout is not decoded text.
                result += "\\u00";
                result += hex[ch >> 4];
                result += hex[ch & 15];
            }
            else
                result += static_cast<char>(ch);
        }
        return result + '"';
    }

    inline std::string Fingerprint(const std::string& path)
    {
        if (path.empty() || path.find('\0') != std::string::npos)
            return "{\"ok\":false,\"error\":\"invalid artifact path\"}";
        std::error_code error;
        const auto utf8 = std::u8string(reinterpret_cast<const char8_t*>(path.data()), path.size());
        const auto absolute = std::filesystem::absolute(std::filesystem::path(utf8), error);
        if (error)
            return "{\"ok\":false,\"error\":\"cannot resolve artifact\"}";
        std::ifstream input(absolute, std::ios::binary);
        if (!input)
            return "{\"ok\":false,\"error\":\"cannot open artifact\"}";
        std::string bytes;
        char block[65536];
        while (input.read(block, sizeof(block)) || input.gcount())
        {
            if (bytes.size() + input.gcount() > 512ULL * 1024 * 1024)
                return "{\"ok\":false,\"error\":\"artifact exceeds 512 MiB hash limit\"}";
            bytes.append(block, static_cast<std::size_t>(input.gcount()));
        }
        if (!input.eof())
            return "{\"ok\":false,\"error\":\"artifact read failed\"}";
        const auto canonical = absolute.generic_u8string();
        return "{\"ok\":true,\"path\":" +
               std_serialization::JsonQuote(
                   std::string(reinterpret_cast<const char*>(canonical.data()), canonical.size())) +
               ",\"bytes\":" + std::to_string(bytes.size()) + ",\"sha256\":" + Quote(std_hash::Sha256(bytes)) + "}";
    }

    inline std::string Environment()
    {
        std::string os = "unsupported";
        std::string power = "null";
#if defined(_WIN32)
        os = "windows";
        if (auto library = LoadLibraryW(L"powrprof.dll"))
        {
            using ReadScheme = DWORD(WINAPI*)(HKEY, GUID**);
            const auto read = reinterpret_cast<ReadScheme>(GetProcAddress(library, "PowerGetActiveScheme"));
            GUID* scheme = nullptr;
            if (read && read(nullptr, &scheme) == ERROR_SUCCESS && scheme)
            {
                char text[80];
                std::snprintf(text, sizeof(text), "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                              static_cast<unsigned long>(scheme->Data1), scheme->Data2, scheme->Data3, scheme->Data4[0],
                              scheme->Data4[1], scheme->Data4[2], scheme->Data4[3], scheme->Data4[4], scheme->Data4[5],
                              scheme->Data4[6], scheme->Data4[7]);
                power = Quote(text);
                LocalFree(scheme);
            }
            FreeLibrary(library);
        }
#elif defined(__linux__)
        os = "linux";
        std::ifstream file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
        std::string governor;
        if (std::getline(file, governor))
            power = Quote(governor);
#elif defined(__APPLE__)
        os = "macos";
#endif
        return "{\"os\":" + Quote(os) + ",\"pointerBits\":" + std::to_string(sizeof(void*) * 8) +
               ",\"hardwareThreads\":" + std::to_string(std::thread::hardware_concurrency()) +
               ",\"powerProfile\":" + power + ",\"unixNanoseconds\":" +
               std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count()) +
               "}";
    }

    inline std::string SelfFingerprint()
    {
        return Fingerprint(std_process::CurrentExecutablePath());
    }

    namespace detail
    {
        struct File
        {
            FILE* value = std::tmpfile();
            ~File()
            {
                if (value)
                    std::fclose(value);
            }
        };

        inline std::uint64_t Size(FILE* file)
        {
#ifdef _WIN32
            const auto size = _filelengthi64(_fileno(file));
            return size < 0 ? 0 : static_cast<std::uint64_t>(size);
#else
            struct stat state{};
            return fstat(fileno(file), &state) == 0 ? static_cast<std::uint64_t>(state.st_size) : 0;
#endif
        }

        inline std::string Read(FILE* file, std::size_t limit)
        {
            std::rewind(file);
            std::string result;
            char block[4096];
            while (result.size() < limit)
            {
                const auto n = std::fread(block, 1, (std::min)(sizeof(block), limit - result.size()), file);
                if (!n)
                    break;
                result.append(block, n);
            }
            return result;
        }

#ifdef _WIN32
        inline std::wstring Wide(const std::string& text)
        {
            const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                                              nullptr, 0);
            if (!n && !text.empty())
                throw std::runtime_error("invalid UTF-8 command");
            std::wstring result(n, L'\0');
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                                result.data(), n);
            return result;
        }

        inline std::wstring Argument(const std::string& text)
        {
            std::wstring result = L"\"";
            std::size_t slashes = 0;
            for (wchar_t ch : Wide(text))
            {
                if (ch == L'\\')
                    ++slashes;
                else
                {
                    result.append(slashes * (ch == L'"' ? 2 : 1) + (ch == L'"'), L'\\');
                    slashes = 0;
                    result += ch;
                }
            }
            result.append(slashes * 2, L'\\');
            return result + L'"';
        }

        struct Handle
        {
            HANDLE value = nullptr;
            ~Handle()
            {
                if (value && value != INVALID_HANDLE_VALUE)
                    CloseHandle(value);
            }
        };
#endif
    } // namespace detail

    // Dedicated synchronous child runner. No shell, inherited stdin or pipes;
    // temporary files prevent pipe deadlocks. Wall time includes process startup
    // and <=1ms polling. Kernel timing must come from the workload protocol.
    inline std::string Execute(const std::string& program, const std::vector<std::string>& arguments,
                               const std::string& cwd, std::int64_t timeoutMs, std::size_t outputLimit,
                               std::int32_t cpu)
    {
        if (program.empty() || program.find('\0') != std::string::npos || cwd.find('\0') != std::string::npos ||
            timeoutMs <= 0 || timeoutMs > 86400000 || outputLimit == 0 || outputLimit > 16 * 1024 * 1024 || cpu < -1)
            return "{\"status\":\"invalid-argument\",\"stdout\":\"\",\"stderr\":\"\"}";
        for (const auto& arg : arguments)
            if (arg.find('\0') != std::string::npos)
                return "{\"status\":\"invalid-argument\",\"stdout\":\"\",\"stderr\":\"\"}";
        detail::File out, err, in;
        if (!out.value || !err.value || !in.value)
            return "{\"status\":\"capture-failed\",\"stdout\":\"\",\"stderr\":\"\"}";
        std::string status = "passed";
        int exitCode = -1;
        std::int64_t cpuNs = -1;
        std::uint64_t peak = 0;
        bool peakAvailable = false;
        bool affinity = cpu == -1;
        const auto start = Now();
#if defined(_WIN32)
        detail::Handle job, stdoutHandle, stderrHandle, stdinHandle;
        job.value = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};
        limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!job.value || !SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limit, sizeof(limit)))
            return "{\"status\":\"isolation-failed\",\"stdout\":\"\",\"stderr\":\"\"}";
        const auto duplicate = [](FILE* file, detail::Handle& handle)
        {
            return DuplicateHandle(GetCurrentProcess(), reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file))),
                                   GetCurrentProcess(), &handle.value, 0, TRUE, DUPLICATE_SAME_ACCESS);
        };
        if (!duplicate(out.value, stdoutHandle) || !duplicate(err.value, stderrHandle) ||
            !duplicate(in.value, stdinHandle))
            return "{\"status\":\"capture-failed\",\"stdout\":\"\",\"stderr\":\"\"}";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = stdoutHandle.value;
        startup.hStdError = stderrHandle.value;
        startup.hStdInput = stdinHandle.value;
        PROCESS_INFORMATION process{};
        std::wstring command = detail::Argument(program);
        for (const auto& arg : arguments)
            command += L" " + detail::Argument(arg);
        const auto directory = detail::Wide(cwd);
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_SUSPENDED | CREATE_NO_WINDOW,
                            nullptr, directory.empty() ? nullptr : directory.c_str(), &startup, &process))
            return "{\"status\":\"launch-failed\",\"stdout\":\"\",\"stderr\":\"\"}";
        detail::Handle processHandle{process.hProcess}, threadHandle{process.hThread};
        if (!AssignProcessToJobObject(job.value, process.hProcess))
        {
            TerminateProcess(process.hProcess, 125);
            WaitForSingleObject(process.hProcess, INFINITE);
            return "{\"status\":\"isolation-failed\",\"stdout\":\"\",\"stderr\":\"\"}";
        }
        if (cpu >= 0)
        {
            affinity = cpu < static_cast<int>(sizeof(DWORD_PTR) * 8) &&
                       SetProcessAffinityMask(process.hProcess, DWORD_PTR{1} << cpu);
            if (!affinity)
                status = "affinity-failed";
        }
        if (status == "passed" && ResumeThread(process.hThread) == static_cast<DWORD>(-1))
            status = "launch-failed";
        while (status == "passed")
        {
            const auto waited = WaitForSingleObject(process.hProcess, 1);
            if (waited == WAIT_OBJECT_0)
                break;
            if (waited != WAIT_TIMEOUT)
                status = "wait-failed";
            else if ((Now() - start) / 1000000 >= timeoutMs)
                status = "timeout";
            else if (detail::Size(out.value) + detail::Size(err.value) > outputLimit)
                status = "output-limit";
        }
        if (status != "passed")
            TerminateJobObject(job.value, 124);
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD code = 0;
        if (GetExitCodeProcess(process.hProcess, &code))
            exitCode = static_cast<int>(code);
        FILETIME created{}, ended{}, kernel{}, user{};
        if (GetProcessTimes(process.hProcess, &created, &ended, &kernel, &user))
        {
            const auto ticks = [](FILETIME t) { return (std::uint64_t{t.dwHighDateTime} << 32) | t.dwLowDateTime; };
            cpuNs = static_cast<std::int64_t>((ticks(kernel) + ticks(user)) * 100);
        }
        PROCESS_MEMORY_COUNTERS memory{};
        if (GetProcessMemoryInfo(process.hProcess, &memory, sizeof(memory)))
        {
            peak = memory.PeakWorkingSetSize;
            peakAvailable = true;
        }
        // Terminate any descendants before reading capture files, even when
        // the root exited successfully. Benchmarks must not leave workers.
        TerminateJobObject(job.value, 124);
#elif defined(__unix__) || defined(__APPLE__)
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(program.c_str()));
        for (const auto& arg : arguments)
            argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);
        const pid_t child = fork();
        if (child < 0)
            return "{\"status\":\"launch-failed\",\"stdout\":\"\",\"stderr\":\"\"}";
        if (child == 0)
        {
            if (setpgid(0, 0) != 0 || dup2(fileno(out.value), STDOUT_FILENO) < 0 ||
                dup2(fileno(err.value), STDERR_FILENO) < 0 || dup2(fileno(in.value), STDIN_FILENO) < 0)
                _exit(125);
            if (!cwd.empty() && chdir(cwd.c_str()) != 0)
                _exit(126);
            if (cpu >= 0)
            {
#ifdef __linux__
                cpu_set_t mask;
                CPU_ZERO(&mask);
                if (cpu >= CPU_SETSIZE)
                    _exit(123);
                CPU_SET(cpu, &mask);
                if (sched_setaffinity(0, sizeof(mask), &mask) != 0)
                    _exit(123);
#else
                _exit(123);
#endif
            }
            execvp(program.c_str(), argv.data());
            _exit(127);
        }
        setpgid(child, child);
        int childStatus = 0;
        rusage usage{};
        while (true)
        {
            const auto waited = wait4(child, &childStatus, WNOHANG, &usage);
            if (waited == child)
                break;
            if (waited < 0 && errno != EINTR)
            {
                status = "wait-failed";
                break;
            }
            if ((Now() - start) / 1000000 >= timeoutMs)
                status = "timeout";
            if (detail::Size(out.value) + detail::Size(err.value) > outputLimit)
                status = "output-limit";
            if (status != "passed")
            {
                kill(-child, SIGKILL);
                kill(child, SIGKILL);
                while (wait4(child, &childStatus, 0, &usage) < 0 && errno == EINTR)
                {
                }
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        kill(-child, SIGKILL);
        exitCode = WIFEXITED(childStatus) ? WEXITSTATUS(childStatus) : 128 + WTERMSIG(childStatus);
        cpuNs = (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000000000LL +
                (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) * 1000LL;
        peak = usage.ru_maxrss;
#ifndef __APPLE__
        peak *= 1024;
#endif
        peakAvailable = true;
        affinity = cpu == -1 || exitCode != 123;
        if (!affinity)
            status = "affinity-failed";
#else
        status = "unsupported";
#endif
        const auto elapsed = Now() - start;
        if (detail::Size(out.value) + detail::Size(err.value) > outputLimit)
            status = "output-limit";
        if (status == "passed" && exitCode != 0)
            status = "nonzero-exit";
        return "{\"status\":" + Quote(status) + ",\"exitCode\":" + std::to_string(exitCode) +
               ",\"wallNanoseconds\":" + std::to_string(elapsed) +
               ",\"cpuNanoseconds\":" + (cpuNs < 0 ? "null" : std::to_string(cpuNs)) +
               ",\"peakResidentBytes\":" + (peakAvailable ? std::to_string(peak) : "null") +
               ",\"affinityApplied\":" + (cpu >= 0 && affinity ? "true" : "false") +
               ",\"stdoutEncoding\":\"byte-as-latin1\",\"stdout\":" + Quote(detail::Read(out.value, outputLimit)) +
               ",\"stderr\":" + Quote(detail::Read(err.value, outputLimit)) + "}";
    }
} // namespace wio::runtime::benchmark
