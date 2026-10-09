#pragma once

#include <any.h>
#include <text.h>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace vm_nested_any
{
    using Any = wio::runtime::Any;

    struct Sample
    {
        std::int32_t value = 37;
        const char* hidden = "nested-producer";
    };

    inline Any retained;
    inline std::int32_t constructions = 0;
    inline std::int32_t destructions = 0;

    inline void Constructed()
    {
        ++constructions;
    }

    inline void Destroyed()
    {
        ++destructions;
    }

    inline std::int32_t Constructions()
    {
        return constructions;
    }

    inline std::int32_t Destructions()
    {
        return destructions;
    }

    inline void Fail()
    {
        throw std::runtime_error("nested-any-destructor-failure");
    }

    inline Any Nest(Any value)
    {
        return Any::Box(std::move(value));
    }

    inline Any Echo(Any value)
    {
        return value;
    }

    inline void Mutate(Any value, std::int32_t next)
    {
        value.AsBoxed<Any>().AsBoxed<std::int32_t>() = next;
    }

    inline void ReplaceInner(Any value, Any next)
    {
        value.AsBoxed<Any>() = std::move(next);
    }

    inline bool SharesInner(const Any& value, const Any& inner)
    {
        return value.AsBoxed<Any>().Cell() == inner.Cell();
    }

    inline void Hold(Any value)
    {
        retained = std::move(value);
    }

    inline Any Held()
    {
        return retained;
    }

    inline void Clear()
    {
        retained = Any{};
    }

    inline Any MakeSample()
    {
        return Nest(Nest(Any::Box(Sample{})));
    }

    inline bool ReadSample(const Any& value)
    {
        const auto& sample = value.AsBoxed<Sample>();
        return sample.value == 37 && std::string(sample.hidden) == "nested-producer";
    }

    inline Any Deep(std::int32_t count)
    {
        auto value = Any::Box(std::int32_t{7});
        while (count-- > 0)
            value = Nest(std::move(value));
        return value;
    }

    inline void Print()
    {
        std::cout << "native-nested-any-ok:30\n";
    }
} // namespace vm_nested_any
