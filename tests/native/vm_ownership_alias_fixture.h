#pragma once

#include <any.h>
#include <ref.h>
#include <wir_native.h>
#include <atomic>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace vm_ownership_alias
{
    inline std::atomic_int opened{0};
    inline std::atomic_int closed{0};
    inline std::atomic_int openedSum{0};
    inline std::atomic_int closedSum{0};
    struct Resource
    {
        std::int32_t value;
    };
    inline void* Open(std::int32_t value)
    {
        ++opened;
        openedSum += value;
        return new Resource{value};
    }
    inline void Close(void* pointer)
    {
        if (pointer)
        {
            ++closed;
            closedSum += static_cast<Resource*>(pointer)->value;
            delete static_cast<Resource*>(pointer);
        }
    }
    inline std::int32_t Read(void* pointer)
    {
        return static_cast<Resource*>(pointer)->value;
    }
    inline bool Balanced()
    {
        if (opened != closed)
            std::cerr << "resource counts: " << opened << '/' << closed << " outstanding sum: " << openedSum - closedSum
                      << '\n';
        return opened == closed;
    }
    inline std::int32_t Closed()
    {
        return closed;
    }
    inline std::int32_t Outstanding()
    {
        return opened.load() - closed.load();
    }
    template <class T> inline T retained;
    template <class T> std::int32_t Consume(T value)
    {
        const auto result = value->_WF_Read();
        retained<T> = std::move(value);
        return result;
    }
    template <class T> void Clear()
    {
        retained<T> = {};
    }
    template <class T> T Echo(T value)
    {
        return value;
    }
    template <class T> T MakeNative(std::int32_t value)
    {
        return T::Create(Open(value));
    }
    template <class T> std::int32_t NativeRead(T value)
    {
        return value->_WF_Read();
    }
    template <class T> void Fail(T value)
    {
        (void)value;
        throw std::runtime_error("ownership native failure");
    }
    inline std::int32_t Catch(std::function<void()> callback)
    {
        const auto before = closed.load();
        try
        {
            callback();
        }
        catch (const std::exception&)
        {
            return closed.load() - before;
        }
        return -1;
    }
    template <class T> std::int32_t Box(wio::runtime::Ref<T> value)
    {
        const auto payload = [&]
        {
            try
            {
                return value->_WF_Get();
            }
            catch (const std::exception& error)
            {
                throw std::runtime_error("Box.Get: " + std::string(error.what()));
            }
        }();
        auto cloned = [&]
        {
            try
            {
                return value->_WF_Clone();
            }
            catch (const std::exception& error)
            {
                throw std::runtime_error("Box.Clone: " + std::string(error.what()));
            }
        }();
        value->_WF_Set_T(payload);
        const auto previous = value->_WF_Replace_T(payload);
        (void)previous;
        value->_WF_Swap_ref_std_heap__box_T_(wio::wir_backend::Place<wio::runtime::Ref<T>>::borrow(cloned));
        if constexpr (requires { payload.size(); })
            return static_cast<std::int32_t>(payload.size() + cloned->_WF_Get().size());
        else if constexpr (std::is_same_v<std::decay_t<decltype(payload)>, wio::runtime::Any>)
            return payload.template AsBoxed<std::int32_t>();
        else
            return payload->_WF_Read() + cloned->_WF_Get()->_WF_Read();
    }
    struct Point
    {
        std::int32_t x;
        std::int32_t y;
    };
    using Row = std::vector<Point>;
    using Map = std::unordered_map<std::string, Row>;
    using Grid = std::vector<Map>;
    template <class RowPlace, class FieldPlace>
    std::int32_t CallbackOverlap(std::function<void(RowPlace, FieldPlace)> callback)
    {
        Row row{{7, 11}};
        try
        {
            callback(RowPlace::borrow(row), FieldPlace::borrow(row[0].x));
            return 1;
        }
        catch (const std::exception&)
        {
            return row[0].x == 7 ? 0 : 2;
        }
    }
    inline bool Aliases(std::int32_t& first, Row& row, const Grid& grid, Point& point, Map& map, std::int32_t& second)
    {
        if (&first != &second || &first != &point.x || &point != &row[0] || &row != &map.at("key") || &map != &grid[0])
            return false;
        first += 2;
        point.x += 3;
        row[0].x += 5;
        map.at("key")[0].x += 7;
        return second == 24 && grid[0].at("key")[0].x == 24;
    }
    inline std::int32_t& Borrow(std::int32_t& first, Grid& grid, std::int32_t& second)
    {
        if (&first != &second || &first != &grid[0].at("key")[0].x)
            throw std::runtime_error("alias identity split");
        return second;
    }
    inline void AliasFailure(Grid& grid, std::int32_t& first, std::int32_t& second)
    {
        if (&first != &second || &first != &grid[0].at("key")[0].x)
            throw std::runtime_error("alias identity split");
        second = 37;
        throw std::runtime_error("alias native failure");
    }
    inline void Print()
    {
        std::cout << "native-ownership-alias-ok:40\n";
    }
} // namespace vm_ownership_alias
