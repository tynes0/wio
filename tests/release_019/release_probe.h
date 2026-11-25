#pragma once

#include <wir_object_abi.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>

namespace release_probe
{
    template <class Container, class T> bool ContainerAlias(Container& values, T& item)
    {
        T* first;
        if constexpr (requires { values.find(std::string{"first"}); })
            first = &values.at("first");
        else
            first = &values[0];
        if (first != &item || item != T{5})
            return false;
        item += T{3};
        return *first == T{8};
    }

    inline std::int32_t OnThread(std::function<std::int32_t()> callback)
    {
        std::int32_t result = -1;
        std::exception_ptr failure;
        std::thread worker{[&]
                           {
                               try
                               {
                                   result = callback();
                               }
                               catch (...)
                               {
                                   failure = std::current_exception();
                               }
                           }};
        worker.join();
        if (failure)
            std::rethrow_exception(failure);
        return result;
    }

    inline std::size_t Read(const std::string& value)
    {
        return value.size();
    }

    inline void Grow(std::string& value)
    {
        value.append("-grown");
    }

    inline std::string Echo(std::string value)
    {
        return value;
    }

    template <class A> std::size_t Apply(std::function<std::size_t(A)> callback, const std::string& value)
    {
        return callback(A::borrowView(value));
    }

    inline void Fail(std::string& value)
    {
        value = "written-before-failure";
        throw std::runtime_error("release-probe failure");
    }
} // namespace release_probe
