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
        launch_failed = 3
    };

    [[nodiscard]] const char* ToString(ProcessError error) noexcept;
    [[nodiscard]] inline int ProcessErrorValue(const ProcessError error) noexcept
    {
        return static_cast<int>(error);
    }

    [[nodiscard]] std::string ExecutableSuffix();
    [[nodiscard]] std::string SharedLibrarySuffix();
    [[nodiscard]] std::string StaticLibrarySuffix();

    [[nodiscard]] bool TryRunResult(
        std::string_view program,
        const std::vector<std::string>& args,
        std::string_view workingDirectory,
        int& exitCode,
        ProcessError& error,
        int& nativeError,
        std::string& message) noexcept;
}
