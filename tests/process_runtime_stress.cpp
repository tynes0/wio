#include "std_async_process.h"
#include "std_process.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
    namespace process = wio::runtime::std_process;

    void Require(const bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::string WithoutLineEnding(std::string value)
    {
        while (!value.empty() && (value.back() == '\r' || value.back() == '\n'))
            value.pop_back();
        return value;
    }

    std::string ReadAll(void* handle, const bool standardError)
    {
        std::string result;
        bool eof = false;
        while (!eof)
        {
            std::string chunk;
            process::ProcessError error = process::ProcessError::none;
            int nativeError = 0;
            std::string message;
            const bool succeeded = standardError
                ? process::ProcessReadStderr(
                    handle, 128, chunk, eof, error, nativeError, message)
                : process::ProcessReadStdout(
                    handle, 128, chunk, eof, error, nativeError, message);
            Require(succeeded, standardError
                ? "native process stderr drain"
                : "native process stdout drain");
            result += chunk;
        }
        return result;
    }

    void* SpawnSelf(const std::vector<std::string>& arguments)
    {
        void* handle = nullptr;
        process::ProcessError error = process::ProcessError::none;
        int nativeError = 0;
        std::string message;
        Require(process::Spawn(
            process::CurrentExecutablePath(), arguments, {}, handle,
            error, nativeError, message), "self process spawn");
        Require(handle != nullptr, "spawn returns an owned process handle");
        return handle;
    }

    template <typename StartOperation>
    void VerifyCallTimeLease(
        const std::uint64_t baseline, StartOperation&& startOperation)
    {
        void* child = SpawnSelf({"--wait-child"});
        auto task = startOperation(child);
        process::ProcessClose(child);
        Require(process::LiveProcessCount() == baseline + 1,
            "async process operation acquires ownership before returning");
        const auto result = wio::runtime::std_async_process::detail::Decode(
            wio::runtime::BlockOn(task));
        Require(!result.succeeded && result.error == process::ProcessError::process_closed,
            "pre-leased asynchronous operation observes process close");
        Require(process::LiveProcessCount() == baseline,
            "completed asynchronous operation releases its process lease");
    }
}

int main(const int argc, char* argv[])
{
    if (argc > 1 && std::string(argv[1]) == "--stream-child")
    {
        std::string input;
        std::getline(std::cin, input);
        std::cout << "stdout:" << input << std::endl;
        std::cerr << "stderr:" << input << std::endl;
        return 7;
    }
    if (argc > 1 && std::string(argv[1]) == "--wait-child")
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return 0;
    }

    try
    {
        const std::uint64_t baseline = process::LiveProcessCount();
        process::ProcessError error = process::ProcessError::none;
        int nativeError = 0;
        std::string message;
        void* invalidHandle = nullptr;
        Require(!process::Spawn({}, {}, {}, invalidHandle, error, nativeError, message),
            "empty process program is recoverable");
        Require(error == process::ProcessError::empty_program && invalidHandle == nullptr,
            "empty process program preserves its error category");

        void* child = SpawnSelf({"--stream-child"});
        std::size_t written = 0;
        Require(process::ProcessWriteStdin(
            child, "hello-native\n", written, error, nativeError, message),
            "native process stdin write");
        Require(written == 13, "native process stdin byte count");
        Require(process::ProcessCloseStdin(child, error, nativeError, message),
            "native process stdin close");

        const std::string standardOutput = ReadAll(child, false);
        const std::string standardError = ReadAll(child, true);
        int exitCode = -1;
        Require(process::ProcessWait(child, exitCode, error, nativeError, message),
            "native process wait");
        Require(WithoutLineEnding(standardOutput) == "stdout:hello-native",
            "stdout stays independent");
        Require(WithoutLineEnding(standardError) == "stderr:hello-native",
            "stderr stays independent");
        Require(exitCode == 7, "native process exit code");
        process::ProcessClose(child);
        Require(process::LiveProcessCount() == baseline, "completed process state is released");

        for (int iteration = 0; iteration < 32; ++iteration)
        {
            void* waitingChild = SpawnSelf({"--wait-child"});
            auto readTask = wio::runtime::std_async_process::ReadStdout(waitingChild, 32);
            process::ProcessClose(waitingChild);
            Require(process::LiveProcessCount() == baseline + 1,
                "async stdout read acquires ownership before returning");
            const auto result = wio::runtime::std_async_process::detail::Decode(
                wio::runtime::BlockOn(readTask));
            Require(!result.succeeded && result.error == process::ProcessError::process_closed,
                "close interrupts a pre-leased asynchronous pipe read");
        }
        Require(process::LiveProcessCount() == baseline, "close races release every process state");

        VerifyCallTimeLease(baseline, [](void* handle)
        {
            return wio::runtime::std_async_process::ReadStderr(handle, 32);
        });
        VerifyCallTimeLease(baseline, [](void* handle)
        {
            return wio::runtime::std_async_process::Wait(handle);
        });

        void* terminatedChild = SpawnSelf({"--wait-child"});
        Require(process::ProcessTerminate(terminatedChild, error, nativeError, message),
            "explicit native process terminate");
        exitCode = 0;
        Require(process::ProcessWait(
            terminatedChild, exitCode, error, nativeError, message),
            "terminated native process remains waitable");
        Require(exitCode != 0, "terminated native process has a non-zero exit code");
        process::ProcessClose(terminatedChild);
        Require(process::LiveProcessCount() == baseline, "terminated process state is released");

        wio::runtime::ShutdownAsyncRuntime();
        std::cout << "process-runtime-stress-ok\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        wio::runtime::ShutdownAsyncRuntime();
        std::cerr << "process runtime stress failed: " << exception.what() << '\n';
        return 1;
    }
}
