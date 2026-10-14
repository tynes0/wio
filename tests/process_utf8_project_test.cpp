#include "entry_args.h"
#include "std_process.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
    namespace fs = std::filesystem;
    namespace process = wio::runtime::std_process;

    std::string utf8(std::u8string_view text)
    {
        return std::string(text.begin(), text.end());
    }

    std::string pathText(const fs::path& path)
    {
        return utf8(path.u8string());
    }

    void expect(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
} // namespace

int main(int argc, char* argv[])
{
    const auto args = wio::runtime::CollectEntryArguments(argc, argv);
    try
    {
        const auto unicode = utf8(u8"T\u00fcrk\u00e7e \U0001f30d");
        const auto folder = fs::path(std::u8string(u8"run T\u00fcrk\u00e7e \U0001f30d"));
        if (args.size() >= 2 && args[1] == "--child")
        {
            expect(args.size() == 8 && args[2] == unicode && args[3] == "two words" && args[4] == "\"quoted\"" &&
                       args[5] == "tail\\" && args[6] == "%WIO_UTF8_ARG%" && args[7] == "a&b",
                   "process arguments were re-encoded, shell-expanded or misquoted");
            expect(fs::current_path().filename() == folder, "process working directory was not preserved");
            std::cout << "process-utf8-ok\n";
            std::cerr << "process-stderr-ok\n";
            return 17;
        }
        if (args.size() >= 2 && args[1] == "--large")
        {
            std::cout << std::string(200000, 'x');
            std::cerr << "stderr";
#if defined(_WIN32)
            std::cout.write("a\0b", 3);
#endif
            return 23;
        }
        expect(args.size() == 2, "test requires its output directory");
        const auto root = fs::absolute(fs::u8path(args[1])) /
                          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        expect(!fs::exists(root), "test directory already exists");
        const auto directory = root / folder;
        fs::create_directories(directory);
        const auto source = fs::u8path(process::CurrentExecutablePath());
        const auto child =
            root / (std::u8string(u8"child T\u00fcrk\u00e7e \U0001f30d & name") + source.extension().u8string());
        fs::copy_file(source, child);
        const auto before = fs::current_path();
        const std::vector<std::string> values{"--child", unicode,          "two words", "\"quoted\"",
                                              "tail\\",  "%WIO_UTF8_ARG%", "a&b"};
        int code = 0;
        int nativeError = 0;
        process::ProcessError error{};
        std::string message;
        expect(process::TryRunResult(pathText(child), values, pathText(directory), code, error, nativeError, message) &&
                   code == 17,
               "UTF-8 Run did not preserve arguments, directory and exit code");
        expect(fs::current_path() == before, "Run changed parent working directory");
        std::string output;
        expect(process::TryRunCapture(pathText(child), values, pathText(directory), code, output, error, nativeError,
                                      message) &&
                   code == 17 && output.find("process-utf8-ok") != std::string::npos &&
                   output.find("process-stderr-ok") != std::string::npos,
               "UTF-8 Capture lost output or process state");
        expect(fs::current_path() == before, "Capture changed parent working directory");
        expect(process::TryRunCapture(pathText(child), {"--large"}, {}, code, output, error, nativeError, message) &&
                   code == 23,
               "large output deadlocked or lost its exit code");
        expect(output.size() >= 200006 && output.find("stderr") != std::string::npos, "large capture was truncated");
#if defined(_WIN32)
        expect(output.find(std::string("a\0b", 3)) != std::string::npos, "binary capture lost embedded NUL");
        expect(!process::TryRunResult(pathText(root / "missing.exe"), {}, {}, code, error, nativeError, message) &&
                   error == process::ProcessError::launch_failed,
               "missing program must be a launch failure");
#endif
        expect(!process::TryRunResult(pathText(child), values, pathText(root / "missing-directory"), code, error,
                                      nativeError, message) &&
                   error == process::ProcessError::invalid_working_directory,
               "invalid Run working directory lost its category");
        expect(!process::TryRunCapture(pathText(child), values, pathText(root / "missing-directory"), code, output,
                                       error, nativeError, message) &&
                   error == process::ProcessError::invalid_working_directory,
               "invalid Capture working directory lost its category");
        std::cout << "UTF-8 project process Run/Capture passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
