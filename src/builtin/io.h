#pragma once

#include "../interpreter/scope.h"

namespace wio
{
    namespace builtin
    {
        class io
        {
        public:
            static void load(ref<scope> target_scope = nullptr);
        };
    }
}

