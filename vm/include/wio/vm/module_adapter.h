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

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace wio::vm
