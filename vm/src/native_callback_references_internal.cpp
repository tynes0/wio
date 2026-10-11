#include "native_callback_references_internal.h"

#include <algorithm>

namespace wio::vm::detail
{
    namespace
    {
        constexpr std::uint8_t TypeReference = 29;
        constexpr std::uint8_t TypeNullable = 30;
        std::uint64_t identity(const bytecode::Module& module, std::uint32_t type)
        {
            // Nullable object codecs carry the underlying nominal identity.
            if (type < module.types.size() && module.types[type].kind == TypeNullable &&
                module.types[type].arguments.size() == 1)
                type = module.types[type].arguments.front();
            const auto found = std::ranges::find(module.contract.reflection, type, &bytecode::Reflection::type);
            return found == module.contract.reflection.end() ? 0 : found->stableTypeId;
        }
    } // namespace
    NativeCallbackReferences::~NativeCallbackReferences()
    {
        for (auto& slot : slots_)
        {
            WioNativeAbiReleaseValue(&slot.outgoing);
            if (slot.local && access_ && access_->cleanup)
            {
                std::string ignored;
                access_->cleanup(access_->state, slot.local->value, ignored);
            }
        }
    }

    NativeValueResult NativeCallbackReferences::read(const bytecode::Module& module, const WioNativeAbiValue& wire,
                                                     std::uint32_t type, const NativePodLayouts* layouts)
    {
        std::uint8_t marshalling = 0;
        if (!callbackArgumentMarshalling(module, type, marshalling, layouts) ||
            module.types[type].kind != TypeReference || module.types[type].arguments.empty())
            return {{}, "unsupported callback borrow type"};
        const bool mutableAccess = (module.types[type].flags & 1u) != 0;
        const auto base = module.types[type].arguments.front();
        if (wire.kind != WIO_NATIVE_ABI_REFERENCE ||
            (wire.flags != WIO_NATIVE_ABI_VALUE_BORROWED &&
             wire.flags != (WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_MUTABLE)) ||
            (mutableAccess && !(wire.flags & WIO_NATIVE_ABI_VALUE_MUTABLE)) || !wire.payload.mutableSlice.data ||
            wire.payload.mutableSlice.size != sizeof(WioNativeAbiValue) ||
            reinterpret_cast<std::uintptr_t>(wire.payload.mutableSlice.data) % alignof(WioNativeAbiValue) ||
            wire.owner.state || wire.owner.ops)
            return {{}, "callback borrow requires a compatible tagged-value cell"};
        auto* token = static_cast<WioNativeAbiValue*>(wire.payload.mutableSlice.data);
        const auto expected = identity(module, base);
        if ((wire.typeId && wire.typeId != expected) ||
            (marshalling != 6 && token->typeId && token->typeId != expected))
            return {{}, "callback reference and token identities disagree"};
        auto slot = std::ranges::find(slots_, token, &Slot::incoming);
        if (slot == slots_.end())
        {
            auto decoded = decodeNativeValue(module, *token, base, marshalling, layouts, 0, access_);
            if (!decoded.succeeded())
                return decoded;
            if (access_ && access_->pinValue)
                access_->pinValue(access_->state, decoded.value);
            auto local = std::make_unique<PlaceStorage>();
            local->value = std::move(decoded.value);
            local->initialized = true;
            local->mutableValue = mutableAccess;
            local->callbackBorrow = true;
            slots_.push_back({.incoming = token,
                              .local = std::move(local),
                              .type = base,
                              .marshalling = marshalling,
                              .mutableAccess = mutableAccess});
            slot = std::prev(slots_.end());
        }
        if (slot->type != base)
            return {{}, "aliased callback borrows disagree on their type"};
        slot->mutableAccess |= mutableAccess;
        slot->local->mutableValue |= mutableAccess;
        return {Value::place(slot->local.get()), {}};
    }

    bool NativeCallbackReferences::write(const bytecode::Module& module, const Value& value, std::uint32_t type,
                                         WioNativeAbiValue& wire, std::string& error, const NativePodLayouts* layouts)
    {
        std::uint8_t marshalling = 0;
        auto* place = value.asPlace();
        if (!callbackArgumentMarshalling(module, type, marshalling, layouts) ||
            module.types[type].kind != TypeReference || !place || !place->initialized ||
            ((module.types[type].flags & 1u) && !place->mutableValue))
        {
            error = "native callback borrow requires a live compatible place";
            return false;
        }
        const auto base = module.types[type].arguments.front();
        const bool mutableAccess = (module.types[type].flags & 1u) != 0;
        auto slot =
            std::ranges::find_if(slots_, [&](const auto& item)
                                 { return item.original && &item.original->storedValue() == &place->storedValue(); });
        if (slot == slots_.end())
        {
            slots_.push_back({.original = place, .type = base, .marshalling = marshalling});
            slot = std::prev(slots_.end());
            if (!encodeOwnedNativeValue(module, place->storedValue(), base, marshalling, slot->outgoing, error, layouts,
                                        access_))
                return false;
            if (marshalling != 6)
                slot->outgoing.typeId = identity(module, base);
        }
        if (slot->type != base)
        {
            error = "aliased native callback borrows disagree on their type";
            return false;
        }
        slot->mutableAccess |= mutableAccess;
        wire.kind = WIO_NATIVE_ABI_REFERENCE;
        wire.flags = WIO_NATIVE_ABI_VALUE_BORROWED | (mutableAccess ? WIO_NATIVE_ABI_VALUE_MUTABLE : 0u);
        wire.typeId = identity(module, base);
        wire.payload.mutableSlice = {&slot->outgoing, sizeof(WioNativeAbiValue)};
        return true;
    }

    bool NativeCallbackReferences::commit(const bytecode::Module& module, std::string& error,
                                          const NativePodLayouts* layouts)
    {
        struct Replacement
        {
            Slot* slot;
            WioNativeAbiValue wire{};
            Value value;
        };
        std::vector<Replacement> staged;
        staged.reserve(slots_.size());
        const auto release = [&](void*) noexcept
        {
            for (auto& item : staged)
            {
                WioNativeAbiReleaseValue(&item.wire);
                if (access_ && access_->cleanup)
                {
                    std::string ignored;
                    access_->cleanup(access_->state, item.value, ignored);
                }
            }
        };
        const std::unique_ptr<void, decltype(release)> guard{this, release};
        for (auto& slot : slots_)
        {
            if (!slot.mutableAccess)
                continue;
            staged.push_back({&slot});
            auto& replacement = staged.back();
            if (slot.incoming)
            {
                if (!slot.local->initialized ||
                    !encodeOwnedNativeValue(module, slot.local->storedValue(), slot.type, slot.marshalling,
                                            replacement.wire, error, layouts, access_))
                {
                    if (error.empty())
                        error = "callback cleared a borrowed cell";
                    return false;
                }
                // Owned slice leases carry their canonical identity. Only
                // identity-less legacy scalar tokens keep their original tag.
                if (slot.marshalling == 1)
                    replacement.wire.typeId = slot.incoming->typeId;
                if (access_ && access_->pinWire)
                    access_->pinWire(access_->state, replacement.wire);
            }
            else
            {
                const auto expected = identity(module, slot.type);
                if (slot.marshalling != 6 && slot.outgoing.typeId && slot.outgoing.typeId != expected)
                {
                    error = "native callback copy-back identity disagrees with its pointee type";
                    return false;
                }
                auto decoded =
                    decodeNativeValue(module, slot.outgoing, slot.type, slot.marshalling, layouts, 0, access_);
                if (!decoded.succeeded())
                {
                    error = std::move(decoded.error);
                    return false;
                }
                replacement.value = std::move(decoded.value);
                if (access_ && access_->pinValue)
                    access_->pinValue(access_->state, replacement.value);
            }
        }
        // Validate every replacement before publishing any copy-back.
        std::vector<Value> retired;
        retired.reserve(staged.size());
        for (auto& replacement : staged)
        {
            auto& slot = *replacement.slot;
            if (slot.incoming)
            {
                WioNativeAbiReleaseValue(slot.incoming);
                *slot.incoming = replacement.wire;
                replacement.wire = {};
            }
            else
            {
                slot.original->retireProjections();
                retired.push_back(std::move(slot.original->storedValue()));
                slot.original->storedValue() = std::move(replacement.value);
            }
        }
        bool cleaned = true;
        for (auto& value : retired)
            if (access_ && access_->cleanup && !access_->cleanup(access_->state, value, error))
                cleaned = false;
        return cleaned;
    }

    bool NativeCallbackReferences::finish(std::string& error)
    {
        bool cleaned = true;
        for (auto& slot : slots_)
            if (slot.local && access_ && access_->cleanup &&
                !access_->cleanup(access_->state, slot.local->value, error))
                cleaned = false;
        return cleaned;
    }
} // namespace wio::vm::detail
