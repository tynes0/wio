#include "std_benchmark.h"
#include <iostream>

int main(int argc, char** argv)
{
    using namespace wio::runtime::benchmark;
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "spawn")
    {
#ifdef _WIN32
        auto command = detail::Argument(argv[0]) + L" sleep";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                            &startup, &process))
            return 9;
        std::cout << process.dwProcessId;
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
#else
        const auto child = fork();
        if (child < 0)
            return 9;
        if (child == 0)
        {
            sleep(10);
            _exit(0);
        }
        std::cout << child;
#endif
        return 0;
    }
    if (mode == "cleanup")
    {
        auto captured = Execute(argv[0], {"spawn"}, "", 2000, 4096, -1);
        const std::string marker = "\"stdout\":\"";
        const auto at = captured.find(marker);
        if (at == std::string::npos)
            return 10;
        const auto pid = std::stoul(captured.substr(at + marker.size()));
        bool stopped = false;
#ifdef _WIN32
        auto handle = OpenProcess(SYNCHRONIZE, FALSE, pid);
        stopped = !handle || WaitForSingleObject(handle, 1000) == WAIT_OBJECT_0;
        if (handle)
            CloseHandle(handle);
#else
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            if (kill(static_cast<pid_t>(pid), 0) != 0 && errno == ESRCH)
            {
                stopped = true;
                break;
            }
#ifdef __linux__
            std::ifstream state("/proc/" + std::to_string(pid) + "/stat");
            std::string text;
            std::getline(state, text);
            if (text.find(") Z") != std::string::npos)
            {
                stopped = true;
                break;
            }
#endif
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
#endif
        // Check INSIDE this process, before the outer Python worker's own job
        // cleanup could hide a defect in the production Execute implementation.
        std::cout << captured;
        return stopped ? 0 : 11;
    }
    if (mode == "execute" && argc >= 6)
    {
        std::vector<std::string> args;
        for (int index = 5; index < argc; ++index)
            args.emplace_back(argv[index]);
        std::cout << Execute(argv[2], args, "", std::stoll(argv[3]), std::stoull(argv[4]), -1);
        return 0;
    }
    if (mode == "sleep")
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 0;
    }
    if (mode == "flood")
    {
        for (int index = 0; index < 100000; ++index)
            std::cout << "0123456789abcdef";
        return 0;
    }
    if (mode == "fail")
    {
        std::cerr << "expected failure\n";
        return 7;
    }
    if (mode == "bytes")
    {
        std::cout.write("a\0b\xff", 4);
        return 0;
    }
    if (mode == "echo")
    {
        std::cout << (argc > 2 ? argv[2] : "");
        return 0;
    }
    if (mode == "protocol" || mode == "wrong")
    {
        std::cout << "{\"schemaVersion\":1,\"checksum\":\"" << (mode == "protocol" ? "42" : "41")
                  << "\",\"elapsedNanoseconds\":1000000,\"operations\":10}";
        return 0;
    }
    return 3;
}
