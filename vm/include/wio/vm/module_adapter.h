#pragma once

#include "wio/vm/machine.h"

#include <wio_module_contract.h>

#include <memory>
#include <string_view>

namespace wio::vm
{
    // Owns the immutable module, machine and SDK descriptor storage together.
    // Calls and native registration belong to the constructing thread. Hosts
    // must keep this adapter (and its library code) alive through every call.
    // Returned owning strings/text may outlive the adapter, but not its DLL.
    class ModuleAdapter final
    {
    public:
        explicit ModuleAdapter(bytecode::Module module, MachineOptions options = {});
        ~ModuleAdapter();

        ModuleAdapter(const ModuleAdapter&) = delete;
        ModuleAdapter& operator=(const ModuleAdapter&) = delete;
        ModuleAdapter(ModuleAdapter&&) = delete;
        ModuleAdapter& operator=(ModuleAdapter&&) = delete;

        [[nodiscard]] const WioSdkModuleContract& contract() const noexcept;
        [[nodiscard]] std::string_view lastError() const noexcept;
        [[nodiscard]] bool loadNativeLibrary(const std::filesystem::path& path, std::string* error = nullptr);
        [[nodiscard]] std::int32_t start() noexcept;
        [[nodiscard]] std::int32_t update(double deltaSeconds) noexcept;
        [[nodiscard]] std::int32_t apiVersion(std::uint32_t& result) noexcept;
        [[nodiscard]] std::int32_t saveState(WioNativeAbiValue& result) noexcept;
        [[nodiscard]] std::int32_t restoreState(const WioNativeAbiValue& value) noexcept;
        // Idempotent; marks the adapter closed before running unload. The
        // caller must destroy it on the owning thread after this safe point.
        [[nodiscard]] std::int32_t close() noexcept;

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace wio::vm
