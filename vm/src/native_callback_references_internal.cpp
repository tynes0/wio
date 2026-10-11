#include "native_callback_references_internal.h"

#include <algorithm>

namespace wio::vm::detail
{
    namespace
    {
        constexpr std::uint8_t TypeReference = 29;
        std::uint64_t identity(const bytecode::Module& module, std::uint32_t type)
        {
            const auto found = std::ranges::find(module.contract.reflection, type, &bytecode::Reflection::type);
            return found == module.contract.reflection.end() ? 0 : found->stableTypeId;
        }
    } // namespace
    NativeCallbackReferences::~NativeCallbackReferences()
    {
        for (auto& slot : slots_)
            WioNativeAbiReleaseValue(&slot.outgoing);
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
        if ((wire.typeId && wire.typeId != expected) || (token->typeId && token->typeId != expected))
            return {{}, "callback reference and token identities disagree"};
        auto slot = std::ranges::find(slots_, token, &Slot::incoming);
        if (slot == slots_.end())
        {
            auto decoded = decodeNativeValue(module, *token, base, marshalling, layouts);
            if (!decoded.succeeded())
                return decoded;
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
            if (!encodeOwnedNativeValue(module, place->storedValue(), base, marshalling, slot->outgoing, error,
                                        layouts))
                return false;
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
        wire.typeId = slot->outgoing.typeId;
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
                WioNativeAbiReleaseValue(&item.wire);
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
                                            replacement.wire, error, layouts))
                {
                    if (error.empty())
                        error = "callback cleared a borrowed cell";
                    return false;
                }
                replacement.wire.typeId = slot.incoming->typeId;
            }
            else
            {
                auto decoded = decodeNativeValue(module, slot.outgoing, slot.type, slot.marshalling, layouts);
                if (!decoded.succeeded())
                {
                    error = std::move(decoded.error);
                    return false;
                }
                replacement.value = std::move(decoded.value);
            }
        }
        // Validate every replacement before publishing any copy-back.
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
                slot.original->storedValue() = std::move(replacement.value);
        }
        return true;
    }
} // namespace wio::vm::detail
