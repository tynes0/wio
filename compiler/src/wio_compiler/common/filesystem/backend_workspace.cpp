#include "wio/common/filesystem/backend_workspace.h"

#include <chrono>
#include <stdexcept>

namespace wio::common::filesystem
{
    BackendWorkspace::BackendWorkspace()
    {
        const auto current = std::filesystem::current_path();
        std::error_code temporaryError;
        const auto temporary = std::filesystem::temp_directory_path(temporaryError);
        for (const auto& parent : {current, temporary})
        {
            if (parent.empty())
                continue;
            if (parent != current)
            {
#if defined(_WIN32)
                bool ascii = true;
                for (const auto value : parent.native())
                    ascii = ascii && value > 0 && value < 128;
                if (!ascii)
                    continue;
#endif
            }
            if (reserve(parent, parent == current))
                return;
        }
        throw std::runtime_error(
            "Could not create a writable ASCII backend workspace in cwd or the temporary directory");
    }

    BackendWorkspace::BackendWorkspace(const std::filesystem::path& parent)
    {
        if (!reserve(std::filesystem::absolute(parent), false))
            throw std::runtime_error("Could not reserve an adjacent backend publication workspace");
    }

    bool BackendWorkspace::reserve(const std::filesystem::path& parent, bool relative)
    {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0; attempt < 64; ++attempt)
        {
            const auto leaf = ".wio-backend-" + std::to_string(nonce) + "-" + std::to_string(attempt);
            const auto candidate = parent / leaf;
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error))
            {
                directory_ = candidate;
                commandDirectory_ = relative ? std::filesystem::path(leaf) : candidate;
                return true;
            }
            if (error)
                return false;
        }
        return false;
    }

    BackendWorkspace::~BackendWorkspace()
    {
        std::error_code ignored;
        for (const auto& file : files_)
            std::filesystem::remove(file, ignored);
        // Never recursively remove unknown files created by user tool flags.
        std::filesystem::remove(directory_, ignored);
    }

    std::filesystem::path BackendWorkspace::file(const char* stem, std::size_t index)
    {
        for (const char* character = stem; *character; ++character)
            if (!((*character >= 'a' && *character <= 'z') || *character == '-'))
                throw std::runtime_error("Backend workspace filenames require a literal ASCII stem");
        const auto leaf = std::string(stem) + "-" + std::to_string(index) + ".tmp";
        files_.push_back(directory_ / leaf);
        return commandDirectory_ / leaf;
    }

    const std::filesystem::path& BackendWorkspace::directory() const noexcept
    {
        return directory_;
    }
} // namespace wio::common::filesystem
