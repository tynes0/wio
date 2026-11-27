#pragma once

#include "wio/common/filesystem/backend_workspace.h"

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace wio::codegen
{
    struct NativeProbeResult final
    {
        int exitCode = 1;
        std::string output;
    };

    struct NativeDependencyConfig final
    {
        std::filesystem::path compiler;
        std::filesystem::path generator;
        std::filesystem::path generatedSource;
        std::vector<std::filesystem::path> sources;
        std::vector<std::string> compileArguments;
        std::vector<std::string> linkArguments;
        std::vector<std::filesystem::path> explicitInputs;
        std::string requirementsSha256;
        std::string version;
        std::string target;
    };

    // An observational build record, deliberately not an authorization to reuse
    // an adapter. Fresh preprocessing/link resolution and cohort publication are
    // separate requirements of the subsequent cache layer.
    class NativeDependencySnapshot final
    {
    public:
        using Runner = std::function<NativeProbeResult(const std::vector<std::string>&)>;

        NativeDependencySnapshot(NativeDependencyConfig config, Runner runner);
        void captureBeforeBuild();
        [[nodiscard]] std::vector<std::string> linkerCaptureArguments();
        void captureAfterBuild(const std::filesystem::path& artifact);
        [[nodiscard]] bool containsInput(const std::filesystem::path& path) const;
        [[nodiscard]] std::string toJson() const;

        [[nodiscard]] static bool parseMakeDependencies(std::string_view text, std::vector<std::string>& paths);
        [[nodiscard]] static bool parseDarwinDependencies(std::string_view bytes, std::vector<std::string>& paths,
                                                          std::vector<std::string>& missing);

    private:
        struct Input final
        {
            std::string path;
            std::string resolvedPath;
            std::string sha256;
            std::string role;
            std::uintmax_t bytes = 0;
        };

        NativeDependencyConfig config_;
        Runner runner_;
        common::filesystem::BackendWorkspace workspace_;
        std::map<std::string, Input> inputs_;
        std::map<std::string, std::string> probes_;
        std::map<std::string, std::string> environment_;
        std::vector<std::string> reasons_;
        std::vector<std::string> preprocessed_;
        std::vector<std::filesystem::path> observedPaths_;
        std::vector<std::string> missing_;
        std::filesystem::path linkerDependencies_;
        std::string linkerFormat_;
        std::string artifactSha256_;
        std::size_t hashedBytes_ = 0;
        bool knownInputsStable_ = true;
        bool captureFinished_ = false;
        bool optionsUnderstood_ = false;

        void reason(std::string message);
        void addInput(const std::filesystem::path& path, std::string_view role);
        [[nodiscard]] bool readBounded(const std::filesystem::path& path, std::string& bytes, std::size_t limit) const;
        std::string probe(const std::string& argument);
        [[nodiscard]] std::filesystem::path resolveTool(const std::string& value) const;
        [[nodiscard]] bool preprocessingArguments(std::vector<std::string>& result);
    };
} // namespace wio::codegen
