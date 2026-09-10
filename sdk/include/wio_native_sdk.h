#pragma once
#include "wio_sdk.h"
#include <cstring>

namespace wio::sdk {
// Move-only owner of a canonical value. Keep the producing DLL loaded until
// its release callback has run, including values surviving their Module view.
class NativeValue {
    WioNativeAbiValue value_{};
    std::shared_ptr<void> keepAlive_;
    NativeValue* borrowed_ = nullptr;
    friend class NativeModule;
    void pin(std::shared_ptr<void> owner) {
        if (!keepAlive_ || keepAlive_.get() == owner.get()) keepAlive_ = std::move(owner);
        else keepAlive_ = std::make_shared<std::pair<std::shared_ptr<void>, std::shared_ptr<void>>>(keepAlive_, std::move(owner));
    }
public:
    NativeValue() = default;
    NativeValue(const NativeValue&) = delete;
    NativeValue& operator=(const NativeValue&) = delete;
    NativeValue(NativeValue&& other) noexcept : value_(std::exchange(other.value_, {})), keepAlive_(std::move(other.keepAlive_)), borrowed_(std::exchange(other.borrowed_, nullptr)) {}
    NativeValue& operator=(NativeValue&& other) noexcept {
        if (this != &other) { reset(); value_ = std::exchange(other.value_, {}); keepAlive_ = std::move(other.keepAlive_); borrowed_ = std::exchange(other.borrowed_, nullptr); }
        return *this;
    }
    ~NativeValue() { reset(); }
    void reset() noexcept { WioNativeAbiReleaseValue(&value_); keepAlive_.reset(); borrowed_ = nullptr; }
    static NativeValue integer(std::int64_t n) { NativeValue v; v.value_.kind=WIO_NATIVE_ABI_I64; v.value_.payload.signedInteger=n; return v; }
    static NativeValue unsignedInteger(std::uint64_t n) { NativeValue v; v.value_.kind=WIO_NATIVE_ABI_U64; v.value_.payload.unsignedInteger=n; return v; }
    static NativeValue boolean(bool b) { NativeValue v; v.value_.kind=WIO_NATIVE_ABI_BOOL; v.value_.payload.boolean=b; return v; }
    static NativeValue string(std::string text) {
        NativeValue v; auto storage = std::make_shared<std::string>(std::move(text));
        v.value_.kind=WIO_NATIVE_ABI_BYTES; v.value_.payload.slice={storage->data(), storage->size()}; v.keepAlive_=storage; return v;
    }
    static NativeValue text(std::u32string text) {
        NativeValue v; auto storage = std::make_shared<std::u32string>(std::move(text));
        for (char32_t c : *storage) if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
            throw std::invalid_argument("expected Unicode scalar values");
        v.value_.kind=WIO_NATIVE_ABI_TEXT_UTF32;
        v.value_.payload.slice={storage->data(), storage->size()*sizeof(char32_t)};
        v.keepAlive_=storage; return v;
    }
    NativeValue borrow(bool mutableAccess = true) & {
        NativeValue v; v.value_.kind=WIO_NATIVE_ABI_REFERENCE; v.value_.typeId=value_.typeId;
        v.value_.flags=WIO_NATIVE_ABI_VALUE_BORROWED | (mutableAccess ? WIO_NATIVE_ABI_VALUE_MUTABLE : 0);
        v.value_.payload.mutableSlice={&value_,sizeof(value_)}; v.borrowed_=this; return v;
    }
    NativeValue clone() const {
        NativeValue copy; copy.value_=value_; copy.keepAlive_=keepAlive_; copy.borrowed_=borrowed_;
        if (value_.flags & WIO_NATIVE_ABI_VALUE_OWNED) {
            WioNativeAbiRetain(value_.owner);
            if (value_.kind==WIO_NATIVE_ABI_OBJECT || value_.kind==WIO_NATIVE_ABI_RUNTIME_VALUE) WioNativeAbiRetain(value_.payload.handle);
            if (value_.kind==WIO_NATIVE_ABI_CALLBACK && value_.payload.callback.ops && value_.payload.callback.ops->retain)
                value_.payload.callback.ops->retain(value_.payload.callback.userdata);
        }
        return copy;
    }
    std::int64_t asInteger() const { if (value_.kind!=WIO_NATIVE_ABI_I64) throw std::runtime_error("expected signed ABI integer"); return value_.payload.signedInteger; }
    bool asBoolean() const { if (value_.kind!=WIO_NATIVE_ABI_BOOL) throw std::runtime_error("expected ABI bool"); return value_.payload.boolean; }
    std::string asString() const {
        if (value_.kind!=WIO_NATIVE_ABI_BYTES) throw std::runtime_error("expected ABI string");
        return value_.payload.slice.size ? std::string(static_cast<const char*>(value_.payload.slice.data),static_cast<std::size_t>(value_.payload.slice.size)) : std::string{};
    }
    const WioNativeAbiValue& raw() const noexcept { return value_; }
    std::u32string asText() const {
        if (value_.kind!=WIO_NATIVE_ABI_TEXT_UTF32 || value_.payload.slice.size%sizeof(char32_t) ||
            (!value_.payload.slice.data && value_.payload.slice.size)) throw std::runtime_error("expected ABI text");
        std::u32string result(value_.payload.slice.size/sizeof(char32_t), U'\0');
        if (!result.empty()) std::memcpy(result.data(),value_.payload.slice.data,value_.payload.slice.size);
        return result;
    }
};

class NativeModule {
    struct State {
        decltype(detail::openLibrary(std::filesystem::path{})) handle{};
        const WioSdkModuleContract* contract{};
        const WioModuleApi* legacy{};
        const WioNativeTaskApi* tasks{};
        const WioNativeAbiRegistry* natives{};
        bool started=false;
        ~State() {
            if (tasks && tasks->shutdown) tasks->shutdown();
            if (started && legacy && legacy->unload) legacy->unload();
            if (handle) detail::closeLibrary(handle);
        }
    };
    std::shared_ptr<State> state_;
public:
    static NativeModule open(const std::filesystem::path& path) {
        NativeModule module; auto state=std::make_shared<State>();
        state->handle=detail::openLibrary(std::filesystem::absolute(path));
        if (!state->handle) throw std::runtime_error("cannot open canonical Wio module");
        auto get=detail::loadSymbol<WioGetSdkModuleContractFn>(state->handle,"WioGetSdkModuleContract");
        if (!get || !WioValidateSdkModuleContract(state->contract=get())) throw std::runtime_error("invalid canonical Wio module descriptor");
        for (std::uint32_t i=0;i<state->contract->exportCount;++i)
            if (state->contract->exports[i].reserved!=WIO_SDK_CALL_NATIVE_ABI_V2) throw std::runtime_error("unsupported canonical value ABI");
        auto legacy=detail::loadSymbol<WioModuleGetApiFn>(state->handle,"WioModuleGetApi");
        if (legacy) { state->legacy=legacy(); detail::validateModuleApi(state->legacy); }
        auto natives=detail::loadSymbol<WioGetNativeAbiRegistryFn>(state->handle,"WioGetNativeAbiRegistry");
        if (natives) {
            state->natives=natives();
            if (!state->natives || state->natives->abiVersion!=WIO_NATIVE_ABI_VERSION ||
                (state->natives->functionCount && !state->natives->functions)) throw std::runtime_error("invalid native ABI registry");
            for (std::uint32_t i=0;i<state->natives->functionCount;++i) {
                const auto& f=state->natives->functions[i];
                if (f.abiVersion!=WIO_NATIVE_ABI_VERSION || !f.thunk || !f.stableKey || !f.nativeSymbol || !f.thunkSymbol)
                    throw std::runtime_error("invalid native ABI entry");
            }
        }
        auto tasks=detail::loadSymbol<WioGetNativeTaskApiFn>(state->handle,"WioGetNativeTaskApi");
        if (tasks) {
            const auto* api=tasks();
            if (!api || api->abiVersion!=WIO_NATIVE_ABI_VERSION || !api->ready || !api->cancel || !api->read ||
                !api->bindMain || !api->pumpMain || !api->shutdown) throw std::runtime_error("unsupported task ABI");
            state->tasks=api;
        }
        module.state_=std::move(state); return module;
    }
    void start() {
        if (!state_) throw std::runtime_error("closed canonical module");
        if (!state_->started) { if (state_->tasks && state_->tasks->bindMain) state_->tasks->bindMain(); if (state_->legacy && state_->legacy->load && state_->legacy->load()!=0) throw std::runtime_error("module load failed"); state_->started=true; }
    }
    void close() noexcept { state_.reset(); }
    const WioSdkModuleContract* contract() const noexcept { return state_ ? state_->contract : nullptr; }
    const WioModuleApi* legacyApi() const noexcept { return state_ ? state_->legacy : nullptr; }
    const WioNativeAbiRegistry* nativeRegistry() const noexcept { return state_ ? state_->natives : nullptr; }
    std::uint64_t pumpMain() const { if (!state_ || !state_->tasks || !state_->tasks->pumpMain) return 0; return state_->tasks->pumpMain(); }
    bool taskReady(const NativeValue& task) const {
        bool ready=false;
        if (!state_ || !state_->tasks || state_->tasks->ready(&task.value_,&ready)!=WIO_NATIVE_ABI_OK) throw std::runtime_error("invalid canonical task");
        return ready;
    }
    void cancelTask(const NativeValue& task) const {
        if (!state_ || !state_->tasks || state_->tasks->cancel(&task.value_)!=WIO_NATIVE_ABI_OK) throw std::runtime_error("invalid canonical task");
    }
    NativeValue readTask(const NativeValue& task) const {
        if (!state_ || !state_->tasks) throw std::runtime_error("canonical task API unavailable");
        NativeValue result; result.keepAlive_=state_; WioNativeAbiFailure error;
        auto status=state_->tasks->read(&task.value_,&result.value_,&error);
        if (status!=WIO_NATIVE_ABI_OK) throw std::runtime_error("task read failed with status "+std::to_string(status));
        return result;
    }
    NativeValue invoke(std::uint64_t stableId, std::span<const NativeValue> arguments = {}) const {
        if (!state_) throw std::runtime_error("closed canonical module");
        const auto* entry=WioFindSdkExport(state_->contract,stableId);
        if (!entry) throw std::runtime_error("canonical export not found");
        std::vector<WioNativeAbiValue> wire; for (const auto& arg:arguments) wire.push_back(arg.value_);
        NativeValue result; result.keepAlive_=state_;
        const auto& call=state_->contract->callTable[entry->callTableSlot];
        auto status=call.invoke(wire.data(),static_cast<std::uint32_t>(wire.size()),&result.value_,call.context);
        for (const auto& arg:arguments) if (arg.borrowed_ && (arg.value_.flags&WIO_NATIVE_ABI_VALUE_MUTABLE)) arg.borrowed_->pin(state_);
        if (status!=WIO_NATIVE_ABI_OK) throw std::runtime_error("canonical invocation failed with status " + std::to_string(status));
        return result;
    }
    NativeValue invoke(std::string_view name, std::span<const NativeValue> arguments = {}) const {
        if (!state_) throw std::runtime_error("closed canonical module");
        for (std::uint32_t i=0;i<state_->contract->exportCount;++i) {
            const auto& entry=state_->contract->exports[i];
            if (entry.logicalName==name) return invoke(entry.stableId,arguments);
        }
        throw std::runtime_error("canonical export name not found");
    }
};
}
