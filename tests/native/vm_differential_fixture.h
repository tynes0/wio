#pragma once

#include <any.h>
#include <std_async.h>
#include <text.h>

#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace vm_differential
{
    struct Point
    {
        float x;
        float y;
    };

    template <class T> struct Cell
    {
        T value;
    };

    inline Point MakePoint()
    {
        return {2.0f, 3.0f};
    }
    inline float CallPoint(std::function<Point()> callback)
    {
        const Point point = callback();
        return point.x + point.y;
    }
    inline void Scale(Point& point, float factor)
    {
        point.x *= factor;
        point.y *= factor;
    }
    inline float Sum(const Point& point)
    {
        return point.x + point.y;
    }
    inline float Sum(Point&)
    {
        return -999;
    } // A view must select the const overload.
    template <class T> T Identity(T value)
    {
        return value;
    }
    template <class T> Cell<T> MakeCell(T value)
    {
        return {value};
    }
    template <class T> T ReadCell(const Cell<T>& cell)
    {
        return cell.value;
    }

    inline void Aliased(std::int32_t& left, std::int32_t& right)
    {
        left = 10;
        right += left;
    }
    inline std::int32_t ObserveAlias(std::int32_t& left, const std::int32_t& right)
    {
        left = 9;
        return right;
    }
    inline void Append(std::string& value)
    {
        value += "-native";
    }
    inline const std::string& Label()
    {
        static const std::string label = "borrowed";
        return label;
    }
    inline std::int8_t EchoI8(std::int8_t value)
    {
        return value;
    }
    inline std::uint64_t EchoU64(std::uint64_t value)
    {
        return value;
    }
    inline float EchoF32(float value)
    {
        return value;
    }

    inline std::int32_t Call(std::function<std::int32_t(std::int32_t)> callback)
    {
        return callback(4);
    }
    inline std::string CallString(std::function<std::string(std::string)> callback)
    {
        // Repeated calls replace temporary VM frames and exercise result storage ownership.
        std::string first = callback(std::string{"a\0b", 3});
        std::string second = callback("second");
        if (first != std::string{"a\0b!", 4} || second != "second!")
            throw std::runtime_error("string callback result corrupted");
        return first;
    }
    inline wio::runtime::Text CallText(std::function<wio::runtime::Text(wio::runtime::Text)> callback)
    {
        return callback(wio::runtime::Text::FromUtf8("Merhaba \xf0\x9f\x8c\x8d"));
    }
    inline std::function<std::int32_t(std::int32_t)> stored;
    inline std::function<std::string(std::string)> storedText;
    inline void StoreText(std::function<std::string(std::string)> callback)
    {
        storedText = std::move(callback);
    }
    inline std::string RunStoredText()
    {
        std::string result;
        std::exception_ptr error;
        std::thread worker(
            [&]
            {
                try
                {
                    result = storedText("worker");
                }
                catch (...)
                {
                    error = std::current_exception();
                }
            });
        worker.join();
        storedText = {};
        if (error)
            std::rethrow_exception(error);
        return result;
    }
    inline void Store(std::function<std::int32_t(std::int32_t)> callback)
    {
        stored = std::move(callback);
    }
    inline bool Expired()
    {
        bool expired = false;
        try
        {
            stored(1);
        }
        catch (const std::exception& error)
        {
            expired = std::string(error.what()).find("expired") != std::string::npos;
        }
        stored = {};
        return expired;
    }
    inline bool WrongThread(std::function<std::int32_t(std::int32_t)> callback)
    {
        bool rejected = false;
        std::thread thread(
            [&]
            {
                try
                {
                    callback(1);
                }
                catch (const std::exception& error)
                {
                    rejected = std::string(error.what()).find("wrong thread") != std::string::npos;
                }
            });
        thread.join();
        return rejected;
    }

    inline wio::runtime::Any MakeAny()
    {
        return wio::runtime::Any(std::int32_t{42});
    }
    inline std::int32_t ReadAny(const wio::runtime::Any& value)
    {
        return value.AsBoxed<std::int32_t>();
    }
    inline void ReplaceAny(wio::runtime::Any& value)
    {
        value = std::int32_t{43};
    }
    inline std::int32_t context = 9;
    inline void* Context()
    {
        return &context;
    }
    inline std::int32_t ReadContext(void* const& value)
    {
        return *static_cast<std::int32_t*>(value);
    }
    inline void ReplaceContext(void*& target, void* value)
    {
        target = value;
    }

    inline wio::runtime::AsyncTask<std::int32_t> Delayed(std::int32_t value)
    {
        co_await wio::runtime::AsyncSleep(1);
        co_return value;
    }
    inline wio::runtime::AsyncTask<wio::runtime::Any> DelayedAny()
    {
        co_await wio::runtime::AsyncSleep(1);
        co_return wio::runtime::Any(std::int32_t{42});
    }
    inline std::int32_t Fail(std::int32_t& value)
    {
        value = 123;
        std::cout << "before-native-failure\n";
        throw std::runtime_error("native-differential-failure");
    }
    inline void Print(std::string value)
    {
        std::cout << value << '\n';
    }
} // namespace vm_differential
