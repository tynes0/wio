#include "wio/codegen/native_dependency_snapshot.h"
#include "wio/common/filesystem/filesystem.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
} // namespace

int main()
{
    using Snapshot = wio::codegen::NativeDependencySnapshot;
    try
    {
        std::vector<std::string> paths;
        require(Snapshot::parseMakeDependencies("target: main.cpp one\\ two.h \\\r\n unicode-İ.h\n", paths),
                "continuations, spaces and UTF-8");
        require(paths == std::vector<std::string>({"main.cpp", "one two.h", "unicode-İ.h"}), "dependency values");
        require(Snapshot::parseMakeDependencies(
                    "C:/out/artifact.tmp: C:/native/a.h C:\\native\\b.h x\\#y.h cash$$.h\nC:/native/a.h:\n", paths),
                "Windows drives and phony rules");
        require(paths == std::vector<std::string>({"C:/native/a.h", "C:\\native\\b.h", "cash$.h", "x#y.h"}),
                "Windows escaping");
        require(Snapshot::parseMakeDependencies("t: a.h a.h\ntwo: b.h\n", paths) && paths.size() == 2,
                "multiple rules must not lose inputs");
        require(!Snapshot::parseMakeDependencies("t: $(UNSUPPORTED)/a.h", paths), "reject Make variables");
        require(!Snapshot::parseMakeDependencies("t: a\\", paths), "reject dangling escape");
        require(!Snapshot::parseMakeDependencies("no rule", paths), "reject malformed rules");
        require(!Snapshot::parseMakeDependencies("t:\n", paths), "empty closure is not complete");
        require(!Snapshot::parseMakeDependencies(std::string("t: a\0b", 7), paths), "reject NUL");

        std::vector<std::string> missing;
        std::string darwin;
        const auto record = [&](unsigned char opcode, std::string_view value)
        {
            darwin += static_cast<char>(opcode);
            darwin += value;
            darwin += '\0';
        };
        record(0x00, "ld64-test");
        record(0x10, "/native/a.o");
        record(0x11, "/native/absent.dylib");
        record(0x40, "/out/app.dylib");
        require(Snapshot::parseDarwinDependencies(darwin, paths, missing), "Darwin record stream");
        require(paths == std::vector<std::string>({"/native/a.o"}) &&
                    missing == std::vector<std::string>({"/native/absent.dylib"}),
                "Darwin input and negative lookup classification");
        darwin.pop_back();
        require(!Snapshot::parseDarwinDependencies(darwin, paths, missing), "reject truncated Darwin record");
        darwin += '\0';
        record(0x77, "unknown");
        require(!Snapshot::parseDarwinDependencies(darwin, paths, missing), "reject unknown Darwin opcode");
        require(!Snapshot::parseDarwinDependencies({}, paths, missing), "reject empty Darwin capture");

        wio::common::filesystem::BackendWorkspace fixtures;
        const auto tool = fixtures.file("tool");
        const auto header = fixtures.file("header");
        const auto source = fixtures.file("source");
        const auto artifact = fixtures.file("artifact");
        const auto write = [](const std::filesystem::path& path, std::string_view value)
        {
            std::ofstream stream(path, std::ios::binary);
            stream << value;
        };
        write(tool, "fixture compiler");
        write(header, "before");
        write(source, "fixture generated source");
        write(artifact, "fixture artifact");
        wio::codegen::NativeDependencyConfig config;
        config.compiler = std::filesystem::absolute(tool);
        config.generator = config.compiler;
        config.generatedSource = source;
        config.compileArguments = {"-std=c++20"};
        const auto runner = [&](const std::vector<std::string>& args) -> wio::codegen::NativeProbeResult
        {
            for (std::size_t i = 0; i + 1 < args.size(); ++i)
            {
                if (args[i] == "-MF")
                    write(wio::common::filesystem::fromUtf8(args[i + 1]),
                          "wio_native_inputs: " + wio::common::filesystem::toUtf8(header) + "\n");
                if (args[i] == "-o")
                    write(wio::common::filesystem::fromUtf8(args[i + 1]), "preprocessed fixture");
            }
            if (args.back().starts_with("-print-prog-name="))
                return {0, wio::common::filesystem::toUtf8(config.compiler)};
            return {0, "gcc fixture --dependency-file -dependency_info"};
        };
        Snapshot changed(config, runner);
        changed.captureBeforeBuild();
        require(changed.containsInput(header), "protect observed header destinations");
        write(header, "after");
        changed.captureAfterBuild(artifact);
        const auto json = changed.toJson();
        require(json.find("\"knownInputsStable\": false") != std::string::npos, "detect concurrent input changes");
        require(json.find("input-changed-during-build") != std::string::npos, "explain changed inputs");

        config.compileArguments.push_back("@untrusted-response-file");
        bool preprocessed = false;
        Snapshot unknown(config,
                         [&](const std::vector<std::string>& args)
                         {
                             for (const auto& arg : args)
                                 preprocessed |= arg == "-E";
                             return runner(args);
                         });
        unknown.captureBeforeBuild();
        require(!preprocessed, "do not forward unknown options into extra processes");
        require(unknown.linkerCaptureArguments().empty(), "do not change an unclassified link invocation");
        unknown.captureAfterBuild(artifact);
        require(unknown.toJson().find("unclassified-compile-option") != std::string::npos, "unknown input reason");
        config.compileArguments.pop_back();
        config.linkArguments = {"-fuse-ld=custom"};
        Snapshot customLinker(config, runner);
        customLinker.captureBeforeBuild();
        require(customLinker.linkerCaptureArguments().empty(), "do not probe a different linker than the build uses");
#ifdef __APPLE__
        config.linkArguments = {"-Xlinker", "-install_name", "-Xlinker", "@rpath/fixture.dylib"};
        Snapshot installName(config, runner);
        installName.captureBeforeBuild();
        require(!installName.linkerCaptureArguments().empty(), "Apple install-name values are not response files");
#endif
        std::cout << "Native dependency codec checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
