#include "artifact.h"
#include "wio/bytecode/codec.h"
#include "wio/bytecode/verifier.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <charconv>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string_view>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace wio::vm::artifact
{
    namespace
    {
        constexpr std::array<std::byte, 8> Magic{std::byte{'W'}, std::byte{'I'}, std::byte{'O'}, std::byte{'V'},
                                                 std::byte{'M'}, std::byte{'P'}, std::byte{'K'}, std::byte{'G'}};
        constexpr std::size_t FooterSize = 32;
        constexpr std::uint64_t Limit = 512ull * 1024ull * 1024ull + 65536;
        constexpr std::string_view LockName = ".wio-vm-package.lock";

        void require(bool condition, const char* message)
        {
            if (!condition)
                throw std::runtime_error(message);
        }
        std::vector<std::byte> bytes(const std::filesystem::path& path)
        {
            std::ifstream input{path, std::ios::binary | std::ios::ate};
            require(bool(input), "could not open VM artifact input");
            const auto length = input.tellg();
            require(length > 0 && std::uint64_t(length) <= Limit,
                    "VM artifact input exceeds its size limit or is empty");
            std::vector<std::byte> result(static_cast<std::size_t>(length));
            input.seekg(0);
            require(bool(input.read(reinterpret_cast<char*>(result.data()), result.size())), "VM artifact read failed");
            return result;
        }
        std::uint64_t hash(std::span<const std::byte> data)
        {
            std::uint64_t result = 14695981039346656037ull;
            for (auto value : data)
            {
                result ^= std::to_integer<unsigned char>(value);
                result *= 1099511628211ull;
            }
            return result;
        }
        void appendInteger(std::vector<std::byte>& data, std::uint64_t value, unsigned width)
        {
            for (unsigned index = 0; index < width; ++index)
                data.push_back(std::byte((value >> (index * 8)) & 255));
        }
        std::uint64_t readInteger(std::span<const std::byte> data, std::size_t& cursor, unsigned width)
        {
            require(cursor <= data.size() && width <= data.size() - cursor, "truncated VM artifact metadata");
            std::uint64_t value = 0;
            for (unsigned index = 0; index < width; ++index)
                value |= std::uint64_t(std::to_integer<unsigned char>(data[cursor++])) << (index * 8);
            return value;
        }
        bool validUtf8(std::string_view value)
        {
            for (std::size_t cursor = 0; cursor < value.size();)
            {
                const auto first = static_cast<unsigned char>(value[cursor++]);
                if (first < 0x80)
                    continue;
                unsigned remaining = 0;
                std::uint32_t scalar = 0;
                std::uint32_t minimum = 0;
                if (first >= 0xc2 && first <= 0xdf)
                {
                    remaining = 1;
                    scalar = first & 0x1f;
                    minimum = 0x80;
                }
                else if (first >= 0xe0 && first <= 0xef)
                {
                    remaining = 2;
                    scalar = first & 0x0f;
                    minimum = 0x800;
                }
                else if (first >= 0xf0 && first <= 0xf4)
                {
                    remaining = 3;
                    scalar = first & 0x07;
                    minimum = 0x10000;
                }
                else
                    return false;
                if (remaining > value.size() - cursor)
                    return false;
                for (unsigned index = 0; index < remaining; ++index)
                {
                    const auto next = static_cast<unsigned char>(value[cursor++]);
                    if ((next & 0xc0) != 0x80)
                        return false;
                    scalar = (scalar << 6) | (next & 0x3f);
                }
                if (scalar < minimum || scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff))
                    return false;
            }
            return true;
        }

        bool safeName(const std::string& name)
        {
            return !name.empty() && name.size() <= 1024 && name != "." && name != ".." &&
                   name.find_first_of("/\\:") == std::string::npos && name.find('\0') == std::string::npos &&
                   validUtf8(name);
        }
        std::string key(std::string name)
        {
#if defined(_WIN32)
            for (char& value : name)
                if (value >= 'A' && value <= 'Z')
                    value += 'a' - 'A';
#endif
            return name;
        }

        std::string versionedName(const std::filesystem::path& source, std::span<const std::byte> data)
        {
            std::array<char, 16> encoded{};
            const auto result = std::to_chars(encoded.data(), encoded.data() + encoded.size(), hash(data), 16);
            const auto stem = source.stem().u8string();
            const auto extension = source.extension().u8string();
            return std::string(stem.begin(), stem.end()) + ".wio-" +
                   std::string(16 - (result.ptr - encoded.data()), '0') + std::string(encoded.data(), result.ptr) +
                   std::string(extension.begin(), extension.end());
        }

        class PackageLock final
        {
        public:
            explicit PackageLock(const std::filesystem::path& directory) : path_(directory / LockName)
            {
                require(std::filesystem::create_directory(path_),
                        "VM package directory is locked by another invocation or an interrupted package");
            }
            PackageLock(const PackageLock&) = delete;
            PackageLock& operator=(const PackageLock&) = delete;
            ~PackageLock()
            {
                std::error_code ignored;
                std::filesystem::remove(path_, ignored);
            }

        private:
            std::filesystem::path path_;
        };

        void verify(std::span<const std::byte> data)
        {
            auto decoded = bytecode::decode(data);
            require(decoded.succeeded(), "VM artifact contains invalid bytecode");
            require(bytecode::Verifier{}.verify(decoded.module).succeeded(),
                    "VM artifact bytecode verification failed");
            const auto& module = decoded.module;
            auto entry = module.contract.hasApplication ? module.contract.application.entry : bytecode::InvalidIndex;
            if (entry == bytecode::InvalidIndex)
            {
                for (std::string_view name : {std::string_view{"main"}, std::string_view{"Entry"}})
                {
                    for (const auto& function : module.functions)
                        if (module.string(function.name) == name)
                        {
                            entry = function.id;
                            break;
                        }
                    if (entry != bytecode::InvalidIndex)
                        break;
                }
            }
            require(entry < module.functions.size() && !module.functions[entry].blocks.empty() &&
                        !(module.functions[entry].flags & bytecode::FunctionTemplateDeclaration),
                    "VM executable requires a concrete runnable entry");
            const auto& parameters = module.functions[entry].parameters;
            require(parameters.size() <= 1, "VM executable entry accepts more than one parameter");
            if (!parameters.empty())
            {
                const auto type = parameters.front().type;
                require(type < module.types.size() && module.types[type].kind == 31 &&
                            module.types[type].arguments.size() == 1 &&
                            module.types[type].arguments.front() < module.types.size() &&
                            module.types[module.types[type].arguments.front()].kind == 17,
                        "VM executable entry parameter must be string[]");
            }
        }
    } // namespace

    bool read(const std::filesystem::path& executable, Payload& payload)
    {
        payload = {};
        std::ifstream input{executable, std::ios::binary | std::ios::ate};
        require(bool(input), "could not open VM executable");
        const auto fileSize = input.tellg();
        if (fileSize < static_cast<std::streamoff>(FooterSize))
            return false;
        std::array<std::byte, FooterSize> footer{};
        input.seekg(fileSize - static_cast<std::streamoff>(FooterSize));
        require(bool(input.read(reinterpret_cast<char*>(footer.data()), footer.size())), "VM footer read failed");
        if (!std::equal(Magic.begin(), Magic.end(), footer.begin()))
            return false;
        std::size_t cursor = Magic.size();
        const auto length = readInteger(footer, cursor, 8);
        const auto checksum = readInteger(footer, cursor, 8);
        const auto version = readInteger(footer, cursor, 4);
        const auto reserved = readInteger(footer, cursor, 4);
        require(version == 1 && reserved == 0, "unsupported VM executable artifact version");
        require(length >= 12 && length <= Limit && length < std::uint64_t(fileSize) - FooterSize,
                "invalid VM executable payload bounds");
        std::vector<std::byte> data(static_cast<std::size_t>(length));
        input.seekg(fileSize - static_cast<std::streamoff>(FooterSize + length));
        require(bool(input.read(reinterpret_cast<char*>(data.data()), data.size())), "VM payload read failed");
        require(hash(data) == checksum, "VM executable payload checksum mismatch");
        cursor = 0;
        const auto moduleLength = readInteger(data, cursor, 8);
        const auto libraryCount = readInteger(data, cursor, 4);
        require(libraryCount <= 64, "VM executable has too many native libraries");
        std::set<std::string> names;
        for (std::uint64_t index = 0; index < libraryCount; ++index)
        {
            const auto size = readInteger(data, cursor, 4);
            require(size <= 1024 && size <= data.size() - cursor, "invalid VM native library name length");
            std::string name(reinterpret_cast<const char*>(data.data() + cursor), static_cast<std::size_t>(size));
            cursor += size;
            require(safeName(name) && names.insert(key(name)).second, "unsafe or duplicate VM native library name");
            payload.libraries.push_back(std::move(name));
        }
        require(moduleLength > 0 && moduleLength == data.size() - cursor, "invalid embedded bytecode length");
        payload.bytecode.assign(data.begin() + cursor, data.end());
        verify(payload.bytecode);
        return true;
    }

    namespace
    {
        void packageImage(const std::filesystem::path& runner, std::span<const std::byte> code,
                          std::span<const std::filesystem::path> libraries, const std::filesystem::path& destination,
                          const PackageOptions options)
        {
            const auto output = std::filesystem::absolute(destination);
            if (std::filesystem::exists(output))
            {
                require(options.replace, "VM package output already exists; choose a new output path");
                Payload prior;
                require(read(output, prior), "refusing to replace a non-VM executable artifact");
            }
            const auto outputName = output.filename().u8string();
            require(key(std::string(outputName.begin(), outputName.end())) != LockName,
                    "VM executable name is reserved for package locking");
            require(libraries.size() <= 64, "VM package has too many native libraries");
            Payload existing;
            require(!read(runner, existing), "a packaged application cannot be used as a runner template");
            auto prefix = bytes(runner);
            verify(code);
            std::vector<std::byte> data;
            appendInteger(data, code.size(), 8);
            appendInteger(data, libraries.size(), 4);
            std::set<std::string> names;
            std::vector<std::filesystem::path> copies;
            std::vector<std::uint64_t> expectedHashes;
            for (const auto& library : libraries)
            {
                const auto encoded = library.filename().u8string();
                const auto libraryBytes = bytes(library);
                const std::string name = options.versionLibraries ? versionedName(library, libraryBytes)
                                                                  : std::string(encoded.begin(), encoded.end());
                require(safeName(name) && names.insert(key(name)).second, "unsafe or duplicate VM native library name");
                require(key(name) != LockName, "native library name is reserved for package locking");
                const auto target = output.parent_path() / std::filesystem::u8path(name);
                require(key(name) != key(std::string(outputName.begin(), outputName.end())),
                        "native library collides with VM executable");
                if (std::filesystem::exists(target))
                    require(bytes(target) == libraryBytes, "VM package would overwrite a different native library");
                copies.push_back(target);
                expectedHashes.push_back(hash(libraryBytes));
                appendInteger(data, name.size(), 4);
                for (unsigned char value : name)
                    data.push_back(std::byte{value});
            }
            data.insert(data.end(), code.begin(), code.end());
            require(data.size() <= Limit, "VM package payload exceeds its size limit");
            std::vector<std::byte> footer(Magic.begin(), Magic.end());
            appendInteger(footer, data.size(), 8);
            appendInteger(footer, hash(data), 8);
            appendInteger(footer, 1, 4);
            appendInteger(footer, 0, 4);
            std::filesystem::create_directories(output.parent_path());
            // Serialize cooperating packagers in one deployment directory. Without
            // this, a failed writer could roll back a DLL another writer just used.
            const PackageLock lock{output.parent_path()};
            if (std::filesystem::exists(output))
            {
                require(options.replace, "VM package output already exists; choose a new output path");
                Payload prior;
                require(read(output, prior), "refusing to replace a non-VM executable artifact");
                // A replacement must not modify a library used by the previous
                // generation. Changed bytes therefore require a new versioned name.
                for (const auto& name : prior.libraries)
                    for (std::size_t index = 0; index < copies.size(); ++index)
                    {
                        const auto encoded = copies[index].filename().u8string();
                        if (key(name) == key(std::string(encoded.begin(), encoded.end())))
                            require(!std::filesystem::exists(copies[index]) ||
                                        bytes(copies[index]) == bytes(libraries[index]),
                                    "replacement would change a library owned by the previous VM generation");
                    }
            }
            auto temporary = output;
            temporary += ".wio-package-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            require(!std::filesystem::exists(temporary), "VM package temporary path already exists");
            std::vector<std::filesystem::path> created;
            try
            {
                std::ofstream stream{temporary, std::ios::binary};
                require(bool(stream), "could not create VM package output");
                for (const auto& block : {std::span<const std::byte>{prefix}, std::span<const std::byte>{data},
                                          std::span<const std::byte>{footer}})
                    stream.write(reinterpret_cast<const char*>(block.data()), block.size());
                stream.close();
                require(bool(stream), "VM package write failed");
                std::filesystem::permissions(temporary, std::filesystem::status(runner).permissions());
                for (std::size_t index = 0; index < copies.size(); ++index)
                    if (std::filesystem::exists(copies[index]))
                    {
                        const auto deployed = bytes(copies[index]);
                        require(deployed == bytes(libraries[index]) && hash(deployed) == expectedHashes[index],
                                "VM package native library changed after preflight");
                    }
                    else
                    {
                        std::filesystem::copy_file(libraries[index], copies[index]);
                        created.push_back(copies[index]);
                        require(hash(bytes(copies[index])) == expectedHashes[index],
                                "VM package native library changed during copy");
                    }
                // Publish the complete executable without an overwrite or a
                // partially visible destination, including concurrent writers.
#if defined(_WIN32)
                const DWORD flags = MOVEFILE_WRITE_THROUGH | (options.replace ? MOVEFILE_REPLACE_EXISTING : 0);
                require(MoveFileExW(temporary.c_str(), output.c_str(), flags) != 0, "could not publish VM executable");
#else
                // Same directory/filesystem by construction. link is exclusive;
                // removing the temporary name does not remove the published file.
                if (options.replace)
                    std::filesystem::rename(temporary, output);
                else
                    std::filesystem::create_hard_link(temporary, output);
#endif
                // Publication is complete. A temporary-file cleanup failure must
                // not roll back libraries required by the published executable.
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
            }
            catch (...)
            {
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
                for (const auto& path : created)
                    std::filesystem::remove(path, ignored);
                throw;
            }
        }
    } // namespace

    void package(const std::filesystem::path& runner, const std::filesystem::path& module,
                 std::span<const std::filesystem::path> libraries, const std::filesystem::path& destination,
                 const PackageOptions options)
    {
        const auto code = bytes(module);
        packageImage(runner, code, libraries, destination, options);
    }

    void deploy(const std::filesystem::path& runner, const std::filesystem::path& application,
                const std::filesystem::path& output)
    {
        Payload source;
        require(read(application, source), "deployment input is not a VM executable artifact");
        std::vector<std::filesystem::path> libraries;
        for (const auto& name : source.libraries)
            libraries.push_back(std::filesystem::absolute(application).parent_path() / std::filesystem::u8path(name));
        packageImage(runner, source.bytecode, libraries, output, {});
    }
} // namespace wio::vm::artifact
