#pragma once

#include "compiler.h"
#include "wio/common/logger.h"
#include "wio/sema/scope.h"
#include "wio/sema/symbol.h"

namespace wio::runtime
{
    class IOLoader
    {
    public:
        static bool load(const Ref<sema::Scope>& scope, std::vector<Ref<sema::Symbol>>& symbols);
    };
}