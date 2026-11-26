#pragma once

#include <any.h>
#include <ref.h>
#include <text.h>
#include <wir_object_abi.h>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

namespace vm_hosted_callback
{
    struct Point
    {
        std::int32_t x = 7;
        std::int32_t hidden = 71;
    };
    inline std::int32_t live = 0;
    inline void Constructed()
    {
        ++live;
    }
    inline void Destroyed()
    {
        --live;
    }
    inline bool Balanced()
    {
        return live == 0;
    }
    inline std::int32_t callbackOwners = 0;
    struct CallbackOwner
    {
        CallbackOwner()
        {
            ++callbackOwners;
        }
        ~CallbackOwner()
        {
            --callbackOwners;
        }
    };
    inline bool CallbacksBalanced()
    {
        return callbackOwners == 0;
    }
    inline std::int32_t Forward(std::function<std::int32_t(std::int32_t)> callback, std::int32_t input)
    {
        return callback(input);
    }
    inline std::function<std::int32_t(std::int32_t)> stored;
    inline void Remember(std::function<std::int32_t(std::int32_t)> callback)
    {
        stored = std::move(callback);
    }
    inline std::int32_t RunStored()
    {
        return stored(7);
    }
    inline void ClearStored()
    {
        stored = {};
    }
    inline WioNativeObjectHost capturedHost{};
    inline std::function<void()> captureLease;
    template <class T> std::uint64_t Capture(wio::runtime::Ref<T> object)
    {
        const auto* proxy = dynamic_cast<const wio::wir_backend::abi::HostObject<T>*>(object.Get());
        wio::wir_backend::abi::require(proxy != nullptr);
        capturedHost = proxy->hostLease().host();
        captureLease = [object = std::move(object)] {};
        return reinterpret_cast<std::uintptr_t>(&capturedHost);
    }
    inline void Clear()
    {
        captureLease = {};
        capturedHost = {};
    }
    template <class T, class U> std::int32_t Exercise(wio::runtime::Ref<T> processor, wio::runtime::Ref<U> item)
    {
        auto owner = std::make_shared<CallbackOwner>();
        std::function<std::int32_t(std::int32_t)> add = [owner](std::int32_t value) { return value + 2; };
        if (processor->_WF_Apply_function(add) != 9 || processor->_WF_Forwarded_function(add) != 9)
            return 1;
        processor->_WF_Remember_function(add);
        if (RunStored() != 9)
            return 8;
        ClearStored();
        std::function<std::string(std::string)> bytes = [](std::string value) { return value; };
        if (processor->_WF_Utf8_function(bytes) != std::string("a\0\xF0\x9F\x8C\x8D", 6))
            return 2;
        std::function<wio::runtime::Text(wio::runtime::Text)> text = [](auto value) { return value; };
        if (processor->_WF_Unicode_function(text).Utf8() != "\xC4\xB0\xF0\x9F\x8C\x8D")
            return 3;
        std::function<Point(Point)> pod = [](Point value)
        {
            value.x += value.hidden;
            return value;
        };
        if (processor->_WF_Pod_function(pod) != 78)
            return 4;
        std::function<std::vector<std::int32_t>(std::vector<std::int32_t>)> array = [](auto values)
        {
            values[0] += 2;
            return values;
        };
        if (processor->_WF_Array_function(array) != 28)
            return 5;
        using Map = std::unordered_map<std::string, std::vector<std::int32_t>>;
        std::function<Map(Map)> map = [](Map values)
        {
            values.at("key")[0] += 2;
            return values;
        };
        if (processor->_WF_Map_function(map) != 9)
            return 9;
        using NullableItem = std::optional<wio::runtime::Ref<U>>;
        std::function<NullableItem(NullableItem)> object = [](auto value) { return value; };
        const auto same = processor->_WF_Object_function_ItemN(object, NullableItem{item});
        if (!same || same->Get() != item.Get() || processor->_WF_Object_function_ItemN(object, {}).has_value())
            return 6;
        std::function<wio::runtime::Any(wio::runtime::Any)> any = [](auto value) { return value; };
        if (processor->_WF_Boxed_function(any) != 19)
            return 7;
        return 0;
    }
    template <class T> void Panic(wio::runtime::Ref<T> processor)
    {
        std::function<std::int32_t(std::int32_t)> callback = [](auto) -> std::int32_t
        { throw std::runtime_error("hosted-callback-failure"); };
        (void)processor->_WF_Apply_function(callback);
    }
    inline void Print()
    {
        std::cout << "hosted-callback-ok:30\n";
    }
} // namespace vm_hosted_callback
