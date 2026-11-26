#pragma once

#include <filesystem>
#include <vector>

namespace wio::common::filesystem
{
    // External tools may use narrow output APIs even when launched with UTF-16
    // arguments. Keep their own output paths ASCII without changing cwd or the
    // interpretation of user-supplied relative compiler/linker arguments.
    class BackendWorkspace final
    {
    public:
        BackendWorkspace();
        // Native-API publication workspace, reserved beside the final output.
        explicit BackendWorkspace(const std::filesystem::path& parent);
        ~BackendWorkspace();
        BackendWorkspace(const BackendWorkspace&) = delete;
        BackendWorkspace& operator=(const BackendWorkspace&) = delete;

        std::filesystem::path file(const char* stem, std::size_t index = 0);
        const std::filesystem::path& directory() const noexcept;

    private:
        bool reserve(const std::filesystem::path& parent, bool relative);
        std::filesystem::path directory_;
        std::filesystem::path commandDirectory_;
        std::vector<std::filesystem::path> files_;
    };
} // namespace wio::common::filesystem
