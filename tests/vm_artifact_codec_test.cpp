#include "artifact.h"
#include "wio/bytecode/codec.h"
#include "wio/bytecode/verifier.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    namespace fs = std::filesystem;
    namespace artifact = wio::vm::artifact;
    using Bytes = std::vector<std::byte>;

    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    void write(const fs::path& path, std::span<const std::byte> data)
    {
        std::ofstream file{path, std::ios::binary};
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        expect(bool(file), "fixture write failed");
    }

    Bytes read(const fs::path& path)
    {
        std::ifstream file{path, std::ios::binary | std::ios::ate};
        expect(bool(file), "fixture read failed");
        Bytes bytes(static_cast<std::size_t>(file.tellg()));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
        expect(bool(file), "fixture read incomplete");
        return bytes;
    }

    template <class F> void rejects(F action, const char* expected)
    {
        try
        {
            action();
        }
        catch (const std::exception& error)
        {
            expect(std::string_view(error.what()).find(expected) != std::string_view::npos,
                   "rejection reported the wrong reason");
            return;
        }
        throw std::runtime_error("invalid artifact was accepted");
    }

    void integer(Bytes& bytes, std::size_t offset, std::uint64_t value, unsigned width)
    {
        for (unsigned index = 0; index < width; ++index)
            bytes.at(offset + index) = std::byte((value >> (8 * index)) & 255);
    }

    void checksum(Bytes& bytes)
    {
        // The fixture runner is exactly 64 bytes; the final footer is 32.
        std::uint64_t hash = 14695981039346656037ull;
        for (std::size_t index = 64; index < bytes.size() - 32; ++index)
        {
            hash ^= std::to_integer<unsigned char>(bytes[index]);
            hash *= 1099511628211ull;
        }
        integer(bytes, bytes.size() - 16, hash, 8);
    }

    Bytes module()
    {
        wio::bytecode::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.strings = {"Entry"};
        module.constants.push_back(wio::bytecode::Constant{});
        module.types.push_back(wio::bytecode::Type{.kind = 1});
        wio::bytecode::Function function;
        function.id = 0;
        function.name = 0;
        function.returnType = 0;
        wio::bytecode::Block block;
        block.id = 0;
        block.instructions.push_back(
            wio::bytecode::Instruction{.opcode = wio::bytecode::Opcode::Return, .constant = 0});
        function.blocks.push_back(std::move(block));
        module.functions.push_back(std::move(function));
        expect(wio::bytecode::Verifier{}.verify(module).succeeded(), "fixture bytecode verification failed");
        return wio::bytecode::encode(module);
    }
} // namespace

int main(int argc, char* argv[])
{
    try
    {
        expect(argc == 2, "test requires its output directory");
        const auto root = fs::absolute(fs::u8path(argv[1])) /
                          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        expect(!fs::exists(root), "test output directory already exists");
        fs::create_directories(root / "deployed");
        fs::create_directories(root / "other");
        const auto runner = root / "runner";
        const auto code = root / "module.wiob";
        const auto app = root / "deployed" / "app";
        const auto library = root / "bridge.dll";
        const auto codeBytes = module();
        write(runner, Bytes(64, std::byte{'R'}));
        write(code, codeBytes);
        write(library, Bytes{std::byte{1}, std::byte{2}});
        artifact::Payload payload;
        expect(!artifact::read(runner, payload), "plain runner was recognized as an application");
        const std::vector<fs::path> libraries{library};
        artifact::package(runner, code, libraries, app);
        expect(artifact::read(app, payload) && payload.bytecode == codeBytes &&
                   payload.libraries == std::vector<std::string>{"bridge.dll"},
               "embedded module round trip failed");
        expect(read(root / "deployed" / "bridge.dll") == read(library), "native library was not copied");
        expect(!fs::exists(root / "deployed" / ".wio-vm-package.lock"), "successful package retained its lock");
        fs::create_directory(root / "deployed" / ".wio-vm-package.lock");
        rejects([&] { artifact::package(runner, code, libraries, root / "deployed" / "second"); }, "locked");
        expect(!fs::exists(root / "deployed" / "second"), "locked directory published another executable");
        fs::remove(root / "deployed" / ".wio-vm-package.lock");
        artifact::package(runner, code, libraries, root / "deployed" / "second");
        expect(read(root / "deployed" / "bridge.dll") == read(library), "identical existing library was changed");
        const auto original = read(app);
        rejects([&] { artifact::package(runner, code, {}, app); }, "already exists");
        expect(read(app) == original, "existing application was overwritten");
        rejects([&] { artifact::package(app, code, {}, root / "nested"); }, "runner template");
        const std::vector<fs::path> duplicates{library, library};
        rejects([&] { artifact::package(runner, code, duplicates, root / "duplicate"); }, "duplicate");
        rejects([&] { artifact::package(runner, code, libraries, root / "other" / "bridge.dll"); }, "collides");
        write(root / "other" / "bridge.dll", Bytes{std::byte{9}});
        rejects([&] { artifact::package(runner, code, libraries, root / "other" / "app"); }, "overwrite");
        expect(read(root / "other" / "bridge.dll") == Bytes{std::byte{9}}, "library collision overwrote a file");
        expect(!fs::exists(root / "other" / "app"), "failed package published an application");
        write(root / "bad.wiob", Bytes{std::byte{0}});
        rejects([&] { artifact::package(runner, root / "bad.wiob", {}, root / "bad"); }, "invalid bytecode");
        expect(!fs::exists(root / "bad"), "bad bytecode created an output");
        auto noEntry = wio::bytecode::decode(codeBytes);
        noEntry.module.functions.front().name = wio::bytecode::InvalidIndex;
        write(root / "library.wiob", wio::bytecode::encode(noEntry.module));
        rejects([&] { artifact::package(runner, root / "library.wiob", {}, root / "library-app"); }, "runnable entry");
        expect(!fs::exists(root / "library-app"), "library-only bytecode created an executable");
        auto badEntry = wio::bytecode::decode(codeBytes);
        badEntry.module.types.push_back(wio::bytecode::Type{.kind = 5});
        badEntry.module.functions.front().parameters.push_back(wio::bytecode::Parameter{.value = 0, .type = 1});
        write(root / "bad-entry.wiob", wio::bytecode::encode(badEntry.module));
        rejects([&] { artifact::package(runner, root / "bad-entry.wiob", {}, root / "bad-entry"); }, "string[]");

        const auto malformed = root / "malformed";
        auto check = [&](Bytes bytes, const char* reason)
        {
            write(malformed, bytes);
            rejects([&] { artifact::read(malformed, payload); }, reason);
        };
        auto bytes = original;
        bytes[64] ^= std::byte{1};
        check(bytes, "checksum");
        bytes = original;
        integer(bytes, bytes.size() - 8, 2, 4);
        check(bytes, "version");
        bytes = original;
        integer(bytes, bytes.size() - 4, 1, 4);
        check(bytes, "version");
        bytes = original;
        integer(bytes, bytes.size() - 24, ~std::uint64_t{0}, 8);
        check(bytes, "bounds");
        bytes = original;
        integer(bytes, 72, 65, 4);
        checksum(bytes);
        check(bytes, "too many");
        bytes = original;
        integer(bytes, 76, 1025, 4);
        checksum(bytes);
        check(bytes, "name length");
        bytes = original;
        bytes[80] = std::byte{'/'};
        checksum(bytes);
        check(bytes, "unsafe");
        bytes = original;
        bytes[80] = std::byte{0xc0}; // Invalid overlong UTF-8 lead byte.
        checksum(bytes);
        check(bytes, "unsafe");
        bytes = original;
        integer(bytes, 64, 1, 8);
        checksum(bytes);
        check(bytes, "bytecode length");
        bytes = original;
        bytes[80 + 10] ^= std::byte{255}; // Corrupt the bytecode magic, not the outer checksum.
        checksum(bytes);
        check(bytes, "invalid bytecode");
        // Recognized footer plus a short metadata area must fail bounded reads.
        bytes = Bytes(original.begin(), original.begin() + 64);
        bytes.insert(bytes.end(), 12, std::byte{0});
        bytes.insert(bytes.end(), original.end() - 32, original.end());
        integer(bytes, bytes.size() - 24, 12, 8);
        integer(bytes, 72, 1, 4);
        checksum(bytes);
        check(bytes, "truncated");
        std::cout << "VM artifact codec, bounds and publication guards passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
