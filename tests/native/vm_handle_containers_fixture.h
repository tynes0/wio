#pragma once

#include "vm_object_any_fixture.h"
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace vm_handle_containers
{
    using Any = wio::runtime::Any;
    using List = std::vector<Any>;
    using Map = std::unordered_map<std::string, Any>;
    using Ordered = std::map<std::string, Any>;
    inline List held;

    inline List Make()
    {
        return {Any::Box(std::int32_t{31}), Any::Box(std::string{"native"}), nullptr};
    }
    inline List Copy(List value)
    {
        value.at(0).AsBoxed<std::int32_t>() += 10;
        value.at(1) = Any::Box(std::int32_t{99});
        return value;
    }
    inline void Update(List& value)
    {
        value.at(1) = Any::Box(std::int32_t{56});
    }
    inline bool Same(const List& first, List& second)
    {
        return &first == &second;
    }
    inline std::vector<List> Nested(std::vector<List> value)
    {
        value.at(0).at(0).AsBoxed<std::int32_t>() += 5;
        value.at(0).at(1) = nullptr;
        return value;
    }
    inline Map CopyMap(Map value)
    {
        value.at("shared").AsBoxed<std::int32_t>() += 2;
        value.at("replace") = Any::Box(std::int32_t{67});
        return value;
    }
    inline void UpdateMap(Map& value)
    {
        value.at("replace") = Any::Box(std::int32_t{68});
    }
    inline bool SameMap(const Map& first, Map& second)
    {
        return &first == &second;
    }
    inline Ordered EchoOrdered(Ordered value)
    {
        value.at("replace") = nullptr;
        return value;
    }
    inline void Grow(Any value)
    {
        value.AsBoxed<List>().push_back(Any::Box(std::int32_t{77}));
    }
    inline void GrowMap(Any value)
    {
        value.AsBoxed<Map>().emplace("native", Any::Box(std::int32_t{78}));
    }
    inline void Hold(List value)
    {
        held = std::move(value);
    }
    inline List Held()
    {
        return held;
    }
    inline void Clear()
    {
        held.clear();
    }
    template <class T> std::vector<T> Touch(std::vector<T> value)
    {
        if constexpr (requires { value.at(0)->_WF_Set_i32(23); })
            value.at(0)->_WF_Set_i32(23);
        else
            value.at(0)->_WF_Set_T(23);
        std::swap(value.at(0), value.at(1));
        return value;
    }
    template <class T> void Rebind(std::vector<T>& value)
    {
        value.at(0) = value.at(1);
    }
    template <class T> std::vector<T> EchoObjects(std::vector<T> value)
    {
        return value;
    }
    template <class T> std::vector<std::optional<T>> Nullable(std::vector<std::optional<T>> value)
    {
        value.at(0) = value.at(1);
        value.at(1).reset();
        return value;
    }
    template <class T> std::unordered_map<std::string, T> Objects(std::unordered_map<std::string, T> value)
    {
        value.at("first")->_WF_Set_i32(24);
        value.at("first") = value.at("second");
        return value;
    }
    template <class T> void TouchBoxed(Any value)
    {
        auto& objects = value.AsBoxed<std::vector<T>>();
        objects.at(0)->_WF_Set_i32(25);
        objects.push_back(objects.at(0));
    }
    template <class T> void TouchAny(List value)
    {
        value.at(0).CastObject<typename T::element_type>()->_WF_Set_i32(26);
    }
    inline void Print()
    {
        std::cout << "native-handle-containers-ok:30\n";
    }
} // namespace vm_handle_containers
