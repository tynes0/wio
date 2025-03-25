#pragma once

#include "../interpreter/scope.h"

namespace wio
{
    namespace builtin
    {
        class io
        {
        public:
            void load(ref<scope> target_scope);
        };
    }
}

