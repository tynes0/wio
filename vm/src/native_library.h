#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace wio::vm
{
    class Machine;

    namespace detail
    {
        class NativeLibrary final
        {
        public:
            ~NativeLibrary();

            NativeLibrary(const NativeLibrary&) = delete;
            NativeLibrary& operator=(const NativeLibrary&) = delete;

            [[nodiscard]] static std::shared_ptr<NativeLibrary> open(const std::filesystem::path& path,
                                                                     std::string& error);
            [[nodiscard]] bool registerWith(Machine& machine, std::string& error) const;
            void releaseMachine() const noexcept;
            [[nodiscard]] const std::filesystem::path& path() const noexcept;

        private:
            NativeLibrary() = default;

            struct State;
            std::unique_ptr<State> state_;
        };
    } // namespace detail
} // namespace wio::vm
