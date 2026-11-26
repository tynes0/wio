#pragma once

#include "vm_object_polymorphic_fixture.h"
#include <optional>

namespace wio::wir_backend
{
    template <class T> class Place;
}

namespace vm_object_polymorphic_references
{
    inline WioNativeObjectHost ownerHost{};
    inline WioNativeObjectHost leafHost{};
    inline std::function<void()> ownerClaim;
    inline std::function<void()> leafClaim;

    template <class T> std::uint64_t CaptureOwner(wio::runtime::Ref<T> object)
    {
        const auto* proxy = dynamic_cast<const wio::wir_backend::abi::HostObject<T>*>(object.Get());
        wio::wir_backend::abi::require(proxy != nullptr);
        ownerHost = proxy->hostLease().host();
        ownerClaim = [object = std::move(object)] {};
        return reinterpret_cast<std::uintptr_t>(&ownerHost);
    }
    template <class T> std::uint64_t CaptureLeaf(wio::runtime::Ref<T> object)
    {
        const auto* proxy = dynamic_cast<const wio::wir_backend::abi::HostObject<T>*>(object.Get());
        wio::wir_backend::abi::require(proxy != nullptr);
        leafHost = proxy->hostLease().host();
        leafClaim = [object = std::move(object)] {};
        return reinterpret_cast<std::uintptr_t>(&leafHost);
    }
    inline void ClearCaptures()
    {
        ownerHost = {};
        leafHost = {};
        ownerClaim = {};
        leafClaim = {};
    }
    template <class T> void Replace(T& target, T source)
    {
        target = std::move(source);
    }
    template <class T> void ReplaceReader(T& target, T source)
    {
        target = std::move(source);
    }
    template <class T> void MixedNullable(T&, std::optional<T>&)
    {
    }

    template <class O, class B, class R> std::int32_t Exercise(O owner, B leaf, B plain, R sibling)
    {
        using BasePlace = wio::wir_backend::Place<B>;
        using ReaderPlace = wio::wir_backend::Place<R>;
        using NullableBase = std::optional<B>;
        using NullableReader = std::optional<R>;
        using NullableBasePlace = wio::wir_backend::Place<NullableBase>;
        using NullableReaderPlace = wio::wir_backend::Place<NullableReader>;
        const auto claims = leaf->StrongCount();
        auto self = leaf->_WF_AsRef();
        auto selfView = leaf->_WF_AsView();
        if (self.Get() != leaf.Get() || selfView.Get() != leaf.Get() || self.Get()->_WF_Read() != 137 ||
            selfView.Get()->_WF_Read() != 137 || self.canRebind() || selfView.canRebind() ||
            leaf->StrongCount() != claims)
            return 11;
        auto base = leaf;
        auto other = leaf;
        auto alias = owner->_WF_BorrowBase_ref_Base(BasePlace::borrow(base));
        auto observed = owner->_WF_ViewBase_view_Base(BasePlace::borrowView(base));
        if (alias.borrowIdentity() != &base || observed.borrowIdentity() != &base || observed.canRebind())
            return 1;
        auto assigned = owner->_WF_AssignBase_ref_Base_Base(BasePlace::borrow(base), plain);
        if (assigned.borrowIdentity() != &base || assigned.Get() != plain.Get() || observed.Get()->_WF_Read() != 7 ||
            other.Get() != leaf.Get())
            return 2;
        alias.write(leaf);
        if (!owner->_WF_AliasBase_ref_Base_view_Base_Base(BasePlace::borrow(base), BasePlace::borrowView(base), leaf) ||
            !owner->_WF_ReverseBase_view_Base_ref_Base_Base(BasePlace::borrowView(base), BasePlace::borrow(base), leaf))
            return 3;
        auto reader = R(leaf);
        auto readerAlias = owner->_WF_BorrowReader_ref_IRead(ReaderPlace::borrow(reader));
        auto readerView = owner->_WF_ViewReader_view_IRead(ReaderPlace::borrowView(reader));
        auto changed = owner->_WF_AssignReader_ref_IRead_IRead(ReaderPlace::borrow(reader), sibling);
        if (changed.borrowIdentity() != &reader || readerAlias.borrowIdentity() != &reader ||
            readerView.borrowIdentity() != &reader || readerView.Get() != sibling.Get() || base.Get() != leaf.Get())
            return 4;
        readerAlias.write(R(leaf));
        if (readerView.Get()->_WF_Read() != 137)
            return 5;
        NullableBase nullableBase;
        NullableReader nullableReader;
        auto nb = owner->_WF_NullableBase_ref_BaseN(NullableBasePlace::borrow(nullableBase));
        auto nr = owner->_WF_NullableReader_ref_IReadN(NullableReaderPlace::borrow(nullableReader));
        if (nb.borrowIdentity() != &nullableBase || nr.borrowIdentity() != &nullableReader || nb.read() || nr.read())
            return 6;
        nb.write(NullableBase{leaf});
        nr.write(NullableReader{sibling});
        nb = owner->_WF_NullableBase_ref_BaseN(NullableBasePlace::borrow(nullableBase));
        nr = owner->_WF_NullableReader_ref_IReadN(NullableReaderPlace::borrow(nullableReader));
        if (!nb.read() || nb.read()->Get() != leaf.Get() || !nr.read() || nr.read()->Get() != sibling.Get())
            return 7;
        nb.write(NullableBase{});
        nr.write(NullableReader{});
        if (nullableBase || nullableReader)
            return 8;
        // A common object does not make distinct handle cells aliases.
        assigned.write(other);
        if (&base == &other || observed.Get() != leaf.Get())
            return 9;
        try
        {
            observed.write(other);
            return 10;
        }
        catch (const std::runtime_error&)
        {
        }
        return 0;
    }
} // namespace vm_object_polymorphic_references
