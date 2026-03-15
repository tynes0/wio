#pragma once

#include <vector>

#include "wio/sema/scope.h"
#include "wio/sema/symbol.h"

namespace wio::runtime
{
    template <typename T>
    concept HasLoad = requires(Ref<sema::Scope> scope, std::vector<Ref<sema::Symbol>>& symbols)
    {
        { T::load(scope, symbols) } -> std::same_as<bool>;
    };

    template <typename T>
    class Loader
    {
    public:
        static bool Load(Ref<sema::Scope> scope, std::vector<Ref<sema::Symbol>>& symbols)
        {
            if constexpr (HasLoad<T>)
            {
                return T::load(std::move(scope), symbols);
            }
            else
            {
                return false;
            }
        }
    };
}