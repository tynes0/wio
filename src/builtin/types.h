#pragma once

#pragma once

#include "../interpreter/scope.h"

namespace wio
{
    namespace builtin
    {
        class types
        {
        public:
            static void load(ref<scope> target_scope = nullptr);
        };
    }
}