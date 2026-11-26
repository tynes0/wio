#include "artifact.h"
#include "wio/bytecode/codec.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    namespace fs = std::filesystem;
    namespace artifact = wio::vm::artifact;
    using Bytes = std::vector<std::byte>;

    void expect(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    void write(const fs::path& path, std::span<const std::byte> data)
    {
        std::ofstream stream{path, std::ios::binary};
        stream.write(reinterpret_cast<const char*>(data.data()), data.size());
        expect(bool(stream), "fixture write failed");
    }

    Bytes read(const fs::path& path)
    {
        std::ifstream stream{path, std::ios::binary | std::ios::ate};
        expect(bool(stream), "fixture read failed");
        Bytes data(static_cast<std::size_t>(stream.tellg()));
        stream.seekg(0);
        stream.read(reinterpret_cast<char*>(data.data()), data.size());
        expect(bool(stream), "fixture read incomplete");
        return data;
    }

    template <class F> void rejects(F action, std::string_view reason)
    {
        try
        {
            action();
        }
        catch (const std::exception& error)
        {
            if (std::string_view(error.what()).find(reason) == std::string_view::npos)
                throw std::runtime_error("wrong rejection: " + std::string(error.what()));
            return;
        }
        throw std::runtime_error("unsafe native cohort accepted");
    }

    Bytes module()
    {
        namespace bc = wio::bytecode;
        bc::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.strings = {"Entry"};
        module.constants.emplace_back();
        module.types.push_back(bc::Type{.kind = 1});
        bc::Function entry;
        entry.id = 0;
        entry.name = 0;
        entry.returnType = 0;
        bc::Block block;
        block.id = 0;
        block.instructions.push_back(bc::Instruction{.opcode = bc::Opcode::Return, .constant = 0});
        entry.blocks.push_back(std::move(block));
        module.functions.push_back(std::move(entry));
        return bc::encode(module);
    }

    void repairChecksum(Bytes& image)
    {
        std::uint64_t checksum = 14695981039346656037ull;
        for (std::size_t index = 64; index < image.size() - 32; ++index)
        {
            checksum ^= std::to_integer<unsigned char>(image[index]);
            checksum *= 1099511628211ull;
        }
        for (unsigned index = 0; index < 8; ++index)
            image[image.size() - 16 + index] = std::byte((checksum >> (index * 8)) & 255);
    }
} // namespace

int main(int argc, char* argv[])
{
    try
    {
        expect(argc == 2, "test requires an output directory");
        const auto root = fs::absolute(fs::u8path(argv[1])) /
                          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        fs::create_directories(root / "source");
        const auto runner = root / "runner";
        const auto code = root / "module.wiob";
        const auto producer = root / "source" / "bridge.dll";
        const auto helper = root / "source" / "helper.dll";
        const auto app = root / "deploy" / "app";
        write(runner, Bytes(64, std::byte{'R'}));
        write(code, module());
        write(producer, Bytes{std::byte{1}, std::byte{2}});
        write(helper, Bytes{std::byte{3}, std::byte{4}});
        const std::vector<fs::path> registries{producer};
        const std::vector<fs::path> runtime{helper};
        const artifact::PackageOptions replace{.replace = true, .versionLibraries = true};
        artifact::package(runner, code, registries, runtime, app, replace);
        artifact::Payload first;
        expect(artifact::read(app, first), "cohort image missing");
        expect(first.libraries.size() == 1 && first.dependencies.size() == 2 && first.dependencies[0].registry &&
                   !first.dependencies[1].registry,
               "private library became a registry producer");
        expect(first.libraries.front().starts_with("wio-native/") && first.libraries.front().ends_with("/bridge.dll") &&
                   first.dependencies[1].path.ends_with("/helper.dll"),
               "native import filenames were renamed");
        artifact::verifyDependencies(app, first);
        const auto original = read(app);
        artifact::package(runner, code, registries, runtime, app, replace);
        expect(read(app) == original, "identical cohort was not deterministic");

        const auto deployedHelper = app.parent_path() / first.dependencies[1].path;
        write(deployedHelper, Bytes{std::byte{8}, std::byte{9}});
        rejects([&] { artifact::verifyDependencies(app, first); }, "checksum");
        rejects([&] { artifact::package(runner, code, registries, runtime, app, replace); }, "overwrite");
        rejects([&] { artifact::deploy(runner, app, root / "bad" / "app"); }, "checksum");
        expect(read(app) == original && !fs::exists(root / "bad" / "app"), "failed update changed image");
        fs::remove(deployedHelper);
        rejects([&] { artifact::verifyDependencies(app, first); }, "missing");
        artifact::package(runner, code, registries, runtime, app, replace);
        artifact::verifyDependencies(app, first);

        write(helper, Bytes{std::byte{5}, std::byte{6}});
        artifact::package(runner, code, registries, runtime, app, replace);
        artifact::Payload second;
        expect(artifact::read(app, second) && first.libraries != second.libraries, "helper change reused old cohort");
        artifact::verifyDependencies(app, second);
        artifact::verifyDependencies(app, first);
        expect(read(deployedHelper) == Bytes({std::byte{3}, std::byte{4}}), "old cohort was mutated");
        const auto relocated = root / "fresh" / "renamed-app";
        artifact::deploy(runner, app, relocated);
        artifact::Payload copied;
        expect(artifact::read(relocated, copied) && copied.libraries == second.libraries &&
                   copied.dependencies[1].path == second.dependencies[1].path,
               "relocation changed dependency identity");
        artifact::verifyDependencies(relocated, copied);
        expect(!fs::exists(relocated.parent_path() / first.libraries.front()), "fresh deployment copied old cohort");

        const std::vector<fs::path> duplicate{producer};
        rejects([&] { artifact::package(runner, code, registries, duplicate, root / "duplicate"); }, "duplicate");
        rejects([&] { artifact::package(runner, code, registries, runtime, root / "reserved-dir" / "wio-native"); },
                "reserved");
        expect(!fs::exists(root / "reserved-dir"), "reserved output created a partial deployment");
        const auto reserved = root / "source" / "wio-native-manifest.json";
        write(reserved, Bytes{std::byte{1}});
        const std::vector<fs::path> reservedLibraries{reserved};
        rejects([&] { artifact::package(runner, code, registries, reservedLibraries, root / "reserved"); }, "unsafe");

        // Check structural validation even when the outer checksum is recalculated.
        auto invalid = original;
        invalid[64 + 16] = std::byte{2}; // First dependency role.
        repairChecksum(invalid);
        write(root / "invalid", invalid);
        rejects([&] { artifact::read(root / "invalid", copied); }, "role");
        invalid = original;
        invalid[64 + 16 + 24] = std::byte{'.'}; // First relative path.
        repairChecksum(invalid);
        write(root / "invalid", invalid);
        rejects([&] { artifact::read(root / "invalid", copied); }, "unsafe");

        const auto symlinkRoot = root / "symlink";
        fs::create_directories(symlinkRoot);
        std::error_code symlinkError;
        fs::create_directory_symlink(app.parent_path() / "wio-native", symlinkRoot / "wio-native", symlinkError);
        if (!symlinkError)
        {
            rejects([&] { artifact::package(runner, code, registries, runtime, symlinkRoot / "app"); }, "escapes");
            expect(!fs::exists(symlinkRoot / "app"), "symlink escape published image");
        }
        else
            std::cout << "Symlink fixture unavailable: " << symlinkError.message() << '\n';

#if !defined(_WIN32)
        // Filename collision policy is portable/case-folded, but filesystem
        // containment must remain case-sensitive on Unix.
        fs::create_directories(root / "case-package");
        fs::create_directories(root / "CASE-PACKAGE" / "wio-native");
        fs::create_directory_symlink(root / "CASE-PACKAGE" / "wio-native", root / "case-package" / "wio-native");
        rejects([&] { artifact::package(runner, code, registries, runtime, root / "case-package" / "app"); },
                "escapes");
#endif

        const auto legacy = root / "legacy" / "app";
        artifact::package(runner, code, registries, legacy);
        expect(artifact::read(legacy, copied) && copied.dependencies.empty() && copied.libraries[0] == "bridge.dll",
               "version-1 package compatibility changed");
        artifact::verifyDependencies(legacy, copied);
        artifact::deploy(runner, legacy, root / "legacy-copy" / "app");
        // Runtime-only cohorts are valid, but no registry is inferred from them.
        artifact::package(runner, code, {}, runtime, root / "runtime-only" / "app");
        expect(artifact::read(root / "runtime-only" / "app", copied) && copied.libraries.empty() &&
                   copied.dependencies.size() == 1,
               "runtime-only package invented a registry");
        std::cout << "VM native cohort integrity, relocation, retention and compatibility passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
