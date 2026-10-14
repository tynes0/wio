#pragma once

#include "wio/vm/module_adapter.h"

#include <wio_module_tasks.h>

namespace wio::vm
{
    // Factory/metadata storage may be static inside a library. Session machines
    // are destroyed explicitly, not by the factory/library destructor. An
    // optional pure-module compatibility adapter is separate from sessions.
    class ModuleSessionFactory final
    {
    public:
        explicit ModuleSessionFactory(bytecode::Module module, MachineOptions options = {});
        ~ModuleSessionFactory();
        ModuleSessionFactory(const ModuleSessionFactory&) = delete;
        ModuleSessionFactory& operator=(const ModuleSessionFactory&) = delete;

        [[nodiscard]] const WioSdkModuleSessionApi& api() const noexcept;
        [[nodiscard]] const WioSdkModuleTaskApi& taskApi() const noexcept;
        // Compatibility getter for old canonical hosts. Lifecycle-bearing
        // modules require explicit sessions and reject this getter.
        [[nodiscard]] const WioSdkModuleContract& defaultContract();

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace wio::vm
