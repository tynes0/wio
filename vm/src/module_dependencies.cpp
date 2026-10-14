#include "wio/vm/module_session.h"

#include <stdexcept>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#if defined(__linux__)
#include <link.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

namespace wio::vm
{
    namespace
    {
        std::filesystem::path imagePath(const void* anchor)
        {
            if (!anchor)
                throw std::invalid_argument("VM module image anchor cannot be null");
#if defined(_WIN32)
            HMODULE image = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                    reinterpret_cast<LPCWSTR>(anchor), &image))
                throw std::runtime_error("Cannot locate VM module image");
            std::vector<wchar_t> name(512);
            while (name.size() <= 65536)
            {
                const auto length = GetModuleFileNameW(image, name.data(), static_cast<DWORD>(name.size()));
                if (!length)
                    throw std::runtime_error("Cannot read VM module image path");
                if (length < name.size())
                    return std::filesystem::path{std::wstring{name.data(), length}};
                name.resize(name.size() * 2);
            }
            throw std::runtime_error("VM module image path is too long");
#else
            Dl_info info{};
            if (!dladdr(anchor, &info) || !info.dli_fname)
                throw std::runtime_error("Cannot locate VM module image");
#if defined(__linux__)
            struct Probe
            {
                std::uintptr_t address;
                bool executable = false;
            } probe{reinterpret_cast<std::uintptr_t>(anchor)};
            dl_iterate_phdr(
                [](dl_phdr_info* image, std::size_t, void* state)
                {
                    auto& probe = *static_cast<Probe*>(state);
                    if (image->dlpi_name && image->dlpi_name[0])
                        return 0;
                    for (std::size_t index = 0; index < image->dlpi_phnum; ++index)
                    {
                        const auto& segment = image->dlpi_phdr[index];
                        const auto start = image->dlpi_addr + segment.p_vaddr;
                        if (segment.p_type == PT_LOAD && probe.address >= start &&
                            probe.address - start < segment.p_memsz)
                        {
                            probe.executable = true;
                            return 1;
                        }
                    }
                    return 0;
                },
                &probe);
            if (probe.executable)
                return std::filesystem::read_symlink("/proc/self/exe");
#elif defined(__APPLE__)
            if (info.dli_fbase == _dyld_get_image_header(0))
            {
                std::uint32_t size = 0;
                (void)_NSGetExecutablePath(nullptr, &size);
                std::vector<char> name(size);
                if (_NSGetExecutablePath(name.data(), &size) != 0)
                    throw std::runtime_error("Cannot read VM host executable path");
                return std::filesystem::path{name.data()};
            }
#endif
            std::filesystem::path path{info.dli_fname};
            if (!path.is_absolute())
                throw std::runtime_error("VM module loader did not provide an absolute image path");
            return path;
#endif
        }

        bool inside(const std::filesystem::path& root, const std::filesystem::path& path)
        {
            auto child = path.begin();
            for (const auto& component : root)
            {
                if (child == path.end() || *child != component)
                    return false;
                ++child;
            }
            return child != path.end();
        }
    } // namespace

    std::vector<std::filesystem::path> moduleNativeLibraries(const void* anchor,
                                                             std::span<const std::filesystem::path> relativePaths)
    {
        if (relativePaths.empty())
            return {};
        const auto root = std::filesystem::canonical(imagePath(anchor)).parent_path();
        std::vector<std::filesystem::path> resolved;
        for (const auto& relative : relativePaths)
        {
            if (relative.empty() || relative.has_root_path() ||
                relative.native().find(std::filesystem::path::value_type{}) != std::filesystem::path::string_type::npos)
                throw std::invalid_argument("VM module native dependency must be a nonempty relative path without NUL");
            for (const auto& part : relative)
                if (part == ".." || part == ".")
                    throw std::invalid_argument(
                        "VM module native dependency cannot traverse parent/current directories");
            const auto path = std::filesystem::canonical(root / relative);
            if (!inside(root, path) || !std::filesystem::is_regular_file(path))
                throw std::invalid_argument("VM module native dependency escapes its image directory or is not a file");
            bool duplicate = false;
            for (const auto& previous : resolved)
                duplicate = duplicate || std::filesystem::equivalent(previous, path);
            if (!duplicate)
                resolved.push_back(path);
        }
        return resolved;
    }
} // namespace wio::vm
