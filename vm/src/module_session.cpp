#include "wio/vm/module_session.h"

#include <mutex>
#include <stdexcept>

namespace wio::vm
{
    struct ModuleSessionFactory::State
    {
        bytecode::Module module;
        MachineOptions options;
        WioSdkModuleSessionApi api{};
        std::mutex defaultMutex;
        std::unique_ptr<ModuleAdapter> legacy;

        static ModuleAdapter* adapter(const WioSdkModuleSession* session) noexcept
        {
            if (!session || !session->context || !session->contract)
                return nullptr;
            auto* result = static_cast<ModuleAdapter*>(session->context);
            return &result->contract() == session->contract ? result : nullptr;
        }

        explicit State(bytecode::Module input, MachineOptions machineOptions)
            : module(std::move(input)), options(machineOptions)
        {
            // Qualify the complete immutable module before exposing the factory.
            ModuleAdapter checked{module, options};
            api = {WIO_SDK_MODULE_SESSION_VERSION,
                   sizeof(WioSdkModuleSessionApi),
                   this,
                   [](void* factory, WioSdkModuleSession* result) noexcept -> std::int32_t
                   {
                       if (!factory || !result || result->context || result->contract)
                           return WIO_NATIVE_ABI_INVALID_ARGUMENT;
                       try
                       {
                           const auto& source = *static_cast<State*>(factory);
                           auto instance = std::make_unique<ModuleAdapter>(source.module, source.options);
                           result->contract = &instance->contract();
                           result->context = instance.release();
                           return WIO_NATIVE_ABI_OK;
                       }
                       catch (...)
                       {
                           return WIO_NATIVE_ABI_EXCEPTION;
                       }
                   },
                   [](WioSdkModuleSession* session) noexcept -> std::int32_t
                   {
                       if (!session)
                           return WIO_NATIVE_ABI_INVALID_ARGUMENT;
                       if (!session->context && !session->contract)
                           return WIO_NATIVE_ABI_OK;
                       auto* instance = adapter(session);
                       if (!instance)
                           return WIO_NATIVE_ABI_INVALID_ARGUMENT;
                       const auto status = instance->close();
                       if (status == WIO_NATIVE_ABI_WRONG_THREAD || status == WIO_NATIVE_ABI_INVALID_ARGUMENT)
                           return status;
                       delete instance;
                       *session = {};
                       return status;
                   },
                   [](const WioSdkModuleSession* session) noexcept -> std::int32_t
                   {
                       auto* value = adapter(session);
                       return value ? value->start() : WIO_NATIVE_ABI_STALE_HANDLE;
                   },
                   [](const WioSdkModuleSession* session, double delta) noexcept -> std::int32_t
                   {
                       auto* value = adapter(session);
                       return value ? value->update(delta) : WIO_NATIVE_ABI_STALE_HANDLE;
                   },
                   [](const WioSdkModuleSession* session, std::uint32_t* result) noexcept -> std::int32_t
                   {
                       auto* value = adapter(session);
                       if (!result)
                           return WIO_NATIVE_ABI_INVALID_ARGUMENT;
                       return value ? value->apiVersion(*result) : WIO_NATIVE_ABI_STALE_HANDLE;
                   },
                   [](const WioSdkModuleSession* session, WioNativeAbiValue* result) noexcept -> std::int32_t
                   {
                       auto* value = adapter(session);
                       if (!result)
                           return WIO_NATIVE_ABI_INVALID_ARGUMENT;
                       return value ? value->saveState(*result) : WIO_NATIVE_ABI_STALE_HANDLE;
                   },
                   [](const WioSdkModuleSession* session, const WioNativeAbiValue* input) noexcept -> std::int32_t
                   {
                       auto* value = adapter(session);
                       if (!input)
                           return WIO_NATIVE_ABI_INVALID_ARGUMENT;
                       return value ? value->restoreState(*input) : WIO_NATIVE_ABI_STALE_HANDLE;
                   }};
        }
    };

    ModuleSessionFactory::ModuleSessionFactory(bytecode::Module module, MachineOptions options)
        : state_(std::make_unique<State>(std::move(module), options))
    {
    }

    ModuleSessionFactory::~ModuleSessionFactory() = default;

    const WioSdkModuleSessionApi& ModuleSessionFactory::api() const noexcept
    {
        return state_->api;
    }

    const WioSdkModuleContract& ModuleSessionFactory::defaultContract()
    {
        std::scoped_lock lock{state_->defaultMutex};
        const auto& lifecycle = state_->module.contract.lifecycle;
        if (lifecycle.apiVersion != bytecode::InvalidIndex || lifecycle.load != bytecode::InvalidIndex ||
            lifecycle.update != bytecode::InvalidIndex || lifecycle.unload != bytecode::InvalidIndex ||
            lifecycle.saveState != bytecode::InvalidIndex || lifecycle.restoreState != bytecode::InvalidIndex)
            throw std::invalid_argument("VM lifecycle modules require explicit SDK sessions");
        if (!state_->legacy)
            state_->legacy = std::make_unique<ModuleAdapter>(state_->module, state_->options);
        return state_->legacy->contract();
    }
} // namespace wio::vm
