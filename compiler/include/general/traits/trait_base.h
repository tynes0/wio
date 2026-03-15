#pragma once

namespace wio::common::traits
{
    template <typename T>
    class TraitBase
    {
        using Type = T;
    };
}

namespace wio
{
    namespace traits = common::traits;
}
