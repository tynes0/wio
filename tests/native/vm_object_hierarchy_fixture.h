#pragma once

#include <ref.h>
#include <wir_object_abi.h>
#include <array>
#include <cstdint>
#include <functional>
#include <stdexcept>

namespace vm_object_hierarchy
{
    inline std::array<std::int32_t, 3> constructed{};
    inline std::array<std::int32_t, 3> destroyed{};
    inline std::int32_t destructionOrder = 0;
    inline WioNativeObjectHost capturedHost{};
    inline std::function<void()> capturedReference;
    inline std::int32_t failureKind = -1;

    inline void SetFailure(std::int32_t kind)
    {
        failureKind = kind;
    }

    inline void Constructed(std::int32_t kind)
    {
        ++constructed.at(kind);
        if (kind == failureKind)
            throw std::runtime_error("hierarchy constructor failure oracle");
    }
    inline void Destroyed(std::int32_t kind)
    {
        ++destroyed.at(kind);
        destructionOrder = destructionOrder * 10 + kind + 1;
    }
    inline std::int32_t Constructions(std::int32_t kind)
    {
        return constructed.at(kind);
    }
    inline std::int32_t Destructions(std::int32_t kind)
    {
        return destroyed.at(kind);
    }
    inline std::int32_t Order()
    {
        return destructionOrder;
    }
    inline void ResetOrder()
    {
        destructionOrder = 0;
    }

    template <class T> std::uint64_t Capture(wio::runtime::Ref<T> object)
    {
        const auto* proxy = dynamic_cast<const wio::wir_backend::abi::HostObject<T>*>(object.Get());
        wio::wir_backend::abi::require(proxy != nullptr);
        capturedHost = proxy->hostLease().host();
        capturedReference = [object = std::move(object)] {};
        return reinterpret_cast<std::uintptr_t>(&capturedHost);
    }
    inline void ClearCapture()
    {
        capturedHost = {};
        capturedReference = {};
    }

    // Explicit Wio native specialization passes Ref<...> template arguments.
    // These casts exercise C++ base/interface views of one concrete proxy;
    // they do not yet marshal a Base/interface-typed free-native argument.
    template <class Object, class Base, class Reader, class Writer> std::int32_t Exercise(Object object)
    {
        auto* base = dynamic_cast<typename Base::element_type*>(object.Get());
        auto* reader = dynamic_cast<typename Reader::element_type*>(object.Get());
        auto* writer = dynamic_cast<typename Writer::element_type*>(object.Get());
        if (!base || !reader || !writer)
            return 1;
        const auto expected = object->_WF_Read();
        if (base->_WF_Read() != expected || reader->_WF_Read() != expected)
            return 2;
        writer->_WF_Set_i32(23);
        if (object->_WF_Read() != 23 + (expected >= 100 ? 100 : 0) || reader->_WF_Read() != object->_WF_Read() ||
            base->_WF_Read() != object->_WF_Read())
            return 3;
        if (base->_WF_BaseOnly_i32(2) != 9 || object->_WF_BaseOnly_i32(3) != 12)
            return 4;
        if (object->_WF_BaseRead() != 12)
            return 5;
        return 0;
    }
} // namespace vm_object_hierarchy
