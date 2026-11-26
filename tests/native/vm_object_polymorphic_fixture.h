#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <ref.h>
#include <wir_object_abi.h>

namespace vm_object_polymorphic
{
    inline std::array<std::int32_t, 4> constructions{};
    inline std::array<std::int32_t, 4> destructions{};
    inline std::function<std::int32_t()> heldBase;
    inline std::function<std::int32_t()> heldReader;

    inline void Constructed(std::int32_t kind)
    {
        ++constructions.at(kind);
    }
    inline void Destroyed(std::int32_t kind)
    {
        ++destructions.at(kind);
    }
    inline std::int32_t Constructions(std::int32_t kind)
    {
        return constructions.at(kind);
    }
    inline std::int32_t Destructions(std::int32_t kind)
    {
        return destructions.at(kind);
    }

    template <class T> T EchoBase(T value)
    {
        return value;
    }
    template <class T> T EchoReader(T value)
    {
        return value;
    }
    template <class T> T EchoNullableBase(T value)
    {
        return value;
    }
    template <class T> T EchoNullableReader(T value)
    {
        return value;
    }
    template <class T> std::int32_t ReadBase(T value)
    {
        return value->_WF_Read();
    }
    template <class T> std::int32_t ReadReader(T value)
    {
        return value->_WF_Read();
    }
    template <class T> void Write(T value, std::int32_t next)
    {
        value->_WF_Set_i32(next);
    }
    template <class T> void HoldBase(T value)
    {
        heldBase = [value = std::move(value)] { return value->_WF_Read(); };
    }
    template <class T> void HoldReader(T value)
    {
        heldReader = [value = std::move(value)] { return value->_WF_Read(); };
    }
    inline std::int32_t ReadHeldBase()
    {
        return heldBase();
    }
    inline std::int32_t ReadHeldReader()
    {
        return heldReader();
    }
    inline void ClearBase()
    {
        heldBase = {};
    }
    inline void ClearReader()
    {
        heldReader = {};
    }
    template <class A, class B> bool Same(A left, B right)
    {
        return left->_WF_RuntimeObject() == right->_WF_RuntimeObject();
    }
    template <class R, class B> B ToBase(R value)
    {
        auto* base = dynamic_cast<typename B::element_type*>(value.Get());
        wio::wir_backend::abi::require(base != nullptr);
        return B(base);
    }
} // namespace vm_object_polymorphic
