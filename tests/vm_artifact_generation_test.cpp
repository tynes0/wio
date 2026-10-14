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

    void write(const fs::path& path, std::span<const std::byte> bytes)
    {
        std::ofstream stream{path, std::ios::binary};
        stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        expect(bool(stream), "fixture write failed");
    }

    Bytes read(const fs::path& path)
    {
        std::ifstream stream{path, std::ios::binary | std::ios::ate};
        expect(bool(stream), "fixture read failed");
        Bytes bytes(static_cast<std::size_t>(stream.tellg()));
        stream.seekg(0);
        stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
        expect(bool(stream), "fixture read incomplete");
        return bytes;
    }

    template <class F> void rejects(F action, std::string_view reason)
    {
        try
        {
            action();
        }
        catch (const std::exception& error)
        {
            expect(std::string_view(error.what()).find(reason) != std::string_view::npos, "wrong rejection reason");
            return;
        }
        throw std::runtime_error("unsafe generation accepted");
    }

    Bytes module(std::string revision)
    {
        namespace bc = wio::bytecode;
        bc::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.strings = {"Entry", std::move(revision)};
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
} // namespace

int main(int argc, char* argv[])
{
    try
    {
        expect(argc == 2, "test requires an output directory");
        const auto root = fs::absolute(fs::u8path(argv[1])) /
                          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        expect(!fs::exists(root), "test directory already exists");
        fs::create_directories(root / "deployed");
        const auto runner = root / "runner";
        const auto code = root / "module.wiob";
        const auto library = root / "bridge.dll";
        const auto app = root / "deployed" / "app";
        write(runner, Bytes(64, std::byte{'R'}));
        const auto firstCode = module("first");
        const auto secondCode = module("second");
        write(code, firstCode);
        write(library, Bytes{std::byte{1}});
        const std::vector<fs::path> libraries{library};
        const artifact::PackageOptions generation{.replace = true, .versionLibraries = true};
        artifact::package(runner, code, libraries, app, generation);
        artifact::Payload first;
        expect(artifact::read(app, first), "first artifact missing");
        expect(first.libraries.size() == 1 && first.libraries.front().starts_with("bridge.wio-") &&
                   first.libraries.front().ends_with(".dll"),
               "library name must preserve extension and contain a version");
        const auto firstLibrary = app.parent_path() / first.libraries.front();
        const auto firstImage = read(app);
        artifact::package(runner, code, libraries, app, generation);
        expect(read(app) == firstImage, "identical generation must be deterministic");
        rejects([&] { artifact::package(runner, code, libraries, app); }, "already exists");

        // Failed builds/packages leave the previous image and its libraries usable.
        write(code, Bytes{std::byte{0}});
        rejects([&] { artifact::package(runner, code, libraries, app, generation); }, "invalid bytecode");
        expect(read(app) == firstImage && read(firstLibrary) == Bytes{std::byte{1}}, "failure changed previous image");
        write(code, secondCode);
        write(library, Bytes{std::byte{2}});
        fs::create_directory(app.parent_path() / ".wio-vm-package.lock");
        rejects([&] { artifact::package(runner, code, libraries, app, generation); }, "locked");
        expect(read(app) == firstImage, "locked replacement changed previous image");
        fs::remove(app.parent_path() / ".wio-vm-package.lock");

        artifact::package(runner, code, libraries, app, generation);
        artifact::Payload second;
        expect(artifact::read(app, second) && second.bytecode == secondCode &&
                   second.libraries.front() != first.libraries.front(),
               "replacement did not publish the new generation");
        expect(read(firstLibrary) == Bytes{std::byte{1}}, "old generation library was overwritten");
        expect(read(app.parent_path() / second.libraries.front()) == Bytes{std::byte{2}}, "new library missing");
        expect(!fs::exists(app.parent_path() / ".wio-vm-package.lock"), "replacement leaked its lock");
        fs::rename(app.parent_path() / second.libraries.front(), root / "missing-generation");
        artifact::package(runner, code, libraries, app, generation);
        expect(read(app.parent_path() / second.libraries.front()) == Bytes{std::byte{2}},
               "replacement did not repair a missing owned dependency");

        const auto foreign = root / "foreign";
        const auto foreignBytes = Bytes(64, std::byte{'X'});
        write(foreign, foreignBytes);
        rejects([&] { artifact::package(runner, code, libraries, foreign, generation); }, "non-VM");
        expect(read(foreign) == foreignBytes, "foreign output was overwritten");
        auto corrupt = read(app);
        corrupt[64] ^= std::byte{1};
        write(foreign, corrupt);
        rejects([&] { artifact::package(runner, code, libraries, foreign, generation); }, "checksum");
        expect(read(foreign) == corrupt, "corrupt unowned output was overwritten");

        const auto deployed = root / "package" / "app";
        artifact::deploy(runner, app, deployed);
        artifact::Payload relocated;
        expect(artifact::read(deployed, relocated) && relocated.bytecode == second.bytecode &&
                   relocated.libraries == second.libraries,
               "deployment changed module or library identities");
        expect(read(deployed.parent_path() / relocated.libraries.front()) == Bytes{std::byte{2}},
               "deployment did not copy the selected library");
        expect(!fs::exists(deployed.parent_path() / first.libraries.front()),
               "deployment copied an obsolete generation");
        rejects([&] { artifact::deploy(runner, app, deployed); }, "already exists");
        fs::rename(app.parent_path() / second.libraries.front(), root / "offline");
        rejects([&] { artifact::deploy(runner, app, root / "missing" / "app"); }, "could not open");
        expect(!fs::exists(root / "missing" / "app"), "missing library published a partial deployment");
        std::cout << "VM generation replacement and deployment guards passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
