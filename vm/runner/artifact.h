#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace wio::vm::artifact
{
    struct NativeDependency
    {
        std::string path;
        std::uint64_t size = 0;
        std::uint64_t checksum = 0;
        bool registry = false;
    };

    struct Payload
    {
        std::vector<std::byte> bytecode;
        std::vector<std::string> libraries;
        std::vector<NativeDependency> dependencies;
    };
    // No footer means an ordinary runner. A recognized but malformed footer
    // throws; it must never fall through to command-line tooling.
    bool read(const std::filesystem::path& executable, Payload& payload);
    // Verify before loading any registry or running Wio code. Version-1 images
    // have presence checks only; version-2 cohorts also bind every file's bytes.
    void verifyDependencies(const std::filesystem::path& executable, const Payload& payload);
    struct PackageOptions
    {
        bool replace = false;
        bool versionLibraries = false;
    };
    void package(const std::filesystem::path& runner, const std::filesystem::path& bytecode,
                 std::span<const std::filesystem::path> libraries, const std::filesystem::path& output,
                 PackageOptions options = {});
    void package(const std::filesystem::path& runner, const std::filesystem::path& bytecode,
                 std::span<const std::filesystem::path> libraries,
                 std::span<const std::filesystem::path> runtimeLibraries, const std::filesystem::path& output,
                 PackageOptions options = {});
    void deploy(const std::filesystem::path& runner, const std::filesystem::path& application,
                const std::filesystem::path& output);
} // namespace wio::vm::artifact
