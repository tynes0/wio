#pragma once

#include <ref.h>
#include <text.h>

#include <cstdint>
#include <functional>
#include <string>

namespace vm_object_proxy
{
    inline std::int32_t constructed = 0;
    inline std::int32_t destroyed = 0;
    inline std::function<std::int32_t()> stored;

    inline void Constructed()
    {
        ++constructed;
    }
    inline void Destroyed()
    {
        ++destroyed;
    }
    inline std::int32_t Constructions()
    {
        return constructed;
    }
    inline std::int32_t Destructions()
    {
        return destroyed;
    }

    template <class T> std::int32_t Add(wio::runtime::Ref<T> object, std::int32_t delta)
    {
        return object->_WF_Add_i32(delta);
    }
    template <class T> std::int32_t Read(wio::runtime::Ref<T> object)
    {
        return object->_WF_Read();
    }
    template <class T> std::string Label(wio::runtime::Ref<T> object, std::string prefix)
    {
        return object->_WF_Label_string(std::move(prefix));
    }
    template <class T> void Reset(wio::runtime::Ref<T> object, std::int32_t value)
    {
        object->_WF_Reset_i32(value);
    }
    template <class T> wio::runtime::Text EchoText(wio::runtime::Ref<T> object, wio::runtime::Text value)
    {
        return object->_WF_EchoText_text(std::move(value));
    }
    template <class T> void Store(wio::runtime::Ref<T> object)
    {
        stored = [object = std::move(object)] { return object->_WF_Read(); };
    }
    inline std::int32_t ReadStored()
    {
        return stored ? stored() : -1;
    }
    inline void Clear()
    {
        stored = {};
    }
} // namespace vm_object_proxy
